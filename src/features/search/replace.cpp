// src/features/search/replace.cpp

#include "texxywindow.h"
#include "ui_texxywindow.h"

#include <QColor>
#include <QList>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QWidget>

namespace Texxy {

// selection layering order is consistent across the app
// current line -> replacement -> found matches -> selection highlights -> column highlight -> bracket matches

namespace {

// RAII to pause widget updates for large edits
struct ScopedUpdatesOff {
    explicit ScopedUpdatesOff(QWidget* w) : widget(w), was(w ? w->updatesEnabled() : true) {
        if (widget)
            widget->setUpdatesEnabled(false);  // avoid repaint storms during bulk ops
    }
    ~ScopedUpdatesOff() {
        if (widget)
            widget->setUpdatesEnabled(was);  // restore previous state
    }
    QWidget* widget;
    bool was;
};

// ensures we always make progress when a match is zero length
inline void advanceAtLeastOne(QTextCursor& c) {
    c.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor);
}

// compute replacement highlight color once per pass
inline QColor replaceColor(const TextEdit* te) {
    return te->hasDarkScheme() ? QColor(Qt::darkGreen) : QColor(Qt::green);
}

}  // namespace

void TexxyWindow::removeGreenSel() {
    // remove only green replacement highlights, keep the rest of the selection stack intact
    const int count = ui->tabWidget->count();
    for (int i = 0; i < count; ++i) {
        auto* tab = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
        if (!tab)
            continue;
        TextEdit* textEdit = tab->textEdit();
        QList<QTextEdit::ExtraSelection> emptySelections;
        textEdit->setGreenSel(emptySelections);
        textEdit->setExtraSelections(composeSelections(textEdit, emptySelections));
    }
}

/*************************/
void TexxyWindow::replaceDock() {
    if (!isReady())
        return;

    if (!ui->dockReplace->isVisible()) {
        const int count = ui->tabWidget->count();
        for (int i = 0; i < count; ++i)  // replace dock relies on the search bar
            qobject_cast<TabPage*>(ui->tabWidget->widget(i))->setSearchBarVisible(true);
        ui->dockReplace->setWindowTitle(tr("Replacement"));
        ui->dockReplace->setVisible(true);
        ui->dockReplace->raise();
        ui->dockReplace->activateWindow();
        if (!ui->lineEditFind->hasFocus())
            ui->lineEditFind->setFocus();
        return;
    }

    ui->dockReplace->setVisible(false);
    // dockVisibilityChanged(false) will run via signal
}

/*************************/
// when the dock hides, clear just the green replacements and restore focus
// docking transitions may briefly flip visibility
void TexxyWindow::dockVisibilityChanged(bool visible) {
    if (visible || isMinimized())
        return;

    removeGreenSel();

    // return focus to the active editor and clear any temporary title
    if (ui->tabWidget->count() > 0) {
        auto* tab = currentTabPage();
        if (!tab)
            return;
        TextEdit* textEdit = tab->textEdit();
        textEdit->setFocus();
        textEdit->setReplaceTitle(QString());
    }
}

/*************************/
// resize the floating dock to its minimum to avoid unused space
void TexxyWindow::resizeDock(bool topLevel) {
    if (topLevel)
        ui->dockReplace->resize(ui->dockReplace->minimumWidth(), ui->dockReplace->minimumHeight());
}

/*************************/
void TexxyWindow::replace() {
    TabPage* tabPage = nullptr;
    TextEdit* textEdit = nullptr;
    if (!resolveActiveTextEdit(true, &tabPage, &textEdit))
        return;

    textEdit->setReplaceTitle(QString());
    ui->dockReplace->setWindowTitle(tr("Replacement"));

    const QString txtFind = ui->lineEditFind->text();
    if (txtFind.isEmpty())
        return;

    const QString txtReplace = ui->lineEditReplace->text();

    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();
    // drop stale green highlights when replacement text changes
    if (!es.isEmpty() && es.first().cursor.selectedText() != txtReplace) {
        textEdit->setGreenSel(QList<QTextEdit::ExtraSelection>());
        es.clear();
    }
    es.reserve(es.size() + 4);  // small heuristic to reduce reallocs

    const QTextDocument::FindFlags searchFlags = getSearchFlags();

    // precompile regex if enabled
    QRegularExpression regexFind;
    const bool useRegex = tabPage->matchRegex();
    if (useRegex) {
        regexFind = QRegularExpression(txtFind, (searchFlags & QTextDocument::FindCaseSensitively)
                                                    ? QRegularExpression::NoPatternOption
                                                    : QRegularExpression::CaseInsensitiveOption);
    }

    // block signals briefly and pause updates to minimize repaints during cursor gymnastics
    const QSignalBlocker blocker(textEdit);
    ScopedUpdatesOff updatesOff(textEdit->viewport());

    QTextCursor start = textEdit->textCursor();
    QTextCursor tmp = start;

    const QObject* s = QObject::sender();
    const bool forward = (s == ui->toolButtonNext);

    QTextCursor found = textEdit->finding(
        txtFind, start, forward ? searchFlags : (searchFlags | QTextDocument::FindBackward), useRegex);

    const QColor color = replaceColor(textEdit);

    if (!found.isNull()) {
        start.setPosition(found.anchor());
        const int anchorPos = found.anchor();
        start.setPosition(found.position(), QTextCursor::KeepAnchor);
        textEdit->skipSelectionHighlighting();  // skip transient selection paints
        textEdit->setTextCursor(start);

        QString realTxtReplace;
        if (useRegex) {
            // apply capturing groups on the matched selection
            realTxtReplace = found.selectedText().replace(regexFind, txtReplace);
            textEdit->insertPlainText(realTxtReplace);
        }
        else {
            realTxtReplace = txtReplace;
            textEdit->insertPlainText(realTxtReplace);
        }

        // highlight the replaced span in green
        start = textEdit->textCursor();  // now at end of inserted text
        tmp.setPosition(anchorPos);
        tmp.setPosition(start.position(), QTextCursor::KeepAnchor);

        QTextEdit::ExtraSelection extra;
        extra.format.setBackground(color);
        extra.cursor = tmp;
        es.append(extra);

        if (!forward) {
            // for backward replace, move cursor before inserted text to allow repeated replacement
            start.setPosition(start.position() - realTxtReplace.length());
            textEdit->setTextCursor(start);
        }
    }

    textEdit->setGreenSel(es);
    textEdit->setExtraSelections(composeSelections(textEdit, es));

    // refresh yellow search highlights in the viewport
    hlight();
}

/*************************/
void TexxyWindow::replaceAll() {
    TabPage* tabPage = nullptr;
    TextEdit* textEdit = nullptr;
    if (!resolveActiveTextEdit(true, &tabPage, &textEdit))
        return;

    const QString txtFind = ui->lineEditFind->text();
    if (txtFind.isEmpty())
        return;

    const QString txtReplace = ui->lineEditReplace->text();

    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();
    // drop stale green highlights when replacement text changes
    if (!es.isEmpty() && es.first().cursor.selectedText() != txtReplace) {
        textEdit->setGreenSel(QList<QTextEdit::ExtraSelection>());
        es.clear();
    }
    es.reserve(es.size() + 128);  // reduce realloc churn for many matches

    const QTextDocument::FindFlags searchFlags = getSearchFlags();

    // precompile regex if enabled
    QRegularExpression regexFind;
    const bool useRegex = tabPage->matchRegex();
    if (useRegex) {
        regexFind = QRegularExpression(txtFind, (searchFlags & QTextDocument::FindCaseSensitively)
                                                    ? QRegularExpression::NoPatternOption
                                                    : QRegularExpression::CaseInsensitiveOption);
    }

    // align the primary cursor to its anchor to avoid accidental partial selections
    QTextCursor origin = textEdit->textCursor();
    origin.setPosition(origin.anchor());
    textEdit->setTextCursor(origin);

    const QColor color = replaceColor(textEdit);

    QTextCursor start = origin;
    QTextCursor tmp = start;
    int count = 0;

    QTextEdit::ExtraSelection extra;
    extra.format.setBackground(color);

    // block signals and updates during the batch edit for speed
    const QSignalBlocker blocker(textEdit);
    ScopedUpdatesOff updatesOff(textEdit->viewport());

    makeBusy();

    start.beginEditBlock();
    start.setPosition(0);

    // repeatedly find then replace, enforcing progress for zero-length matches
    QTextCursor found;
    int lastPos = -1;

    while (!(found = textEdit->finding(txtFind, start, searchFlags, useRegex)).isNull()) {
        const int matchStart = found.anchor();
        const int matchEnd = found.position();

        // guard against zero-length progress to avoid infinite loops on patterns like ^ or lookarounds
        if (matchEnd == matchStart && matchEnd == lastPos) {
            advanceAtLeastOne(start);
            continue;
        }
        lastPos = matchEnd;

        start.setPosition(matchStart);
        start.setPosition(matchEnd, QTextCursor::KeepAnchor);

        if (useRegex) {
            const QString replaced = found.selectedText().replace(regexFind, txtReplace);
            start.insertText(replaced);
            if (count < 1000) {
                tmp.setPosition(matchStart);
                tmp.setPosition(start.position(), QTextCursor::KeepAnchor);
                extra.cursor = tmp;
                es.append(extra);
            }
        }
        else {
            start.insertText(txtReplace);
            if (count < 1000) {
                tmp.setPosition(matchStart);
                tmp.setPosition(start.position(), QTextCursor::KeepAnchor);
                extra.cursor = tmp;
                es.append(extra);
            }
        }

        // continue scanning from end of the inserted text
        start.setPosition(start.position());
        ++count;

        // for zero-length matches ensure we always advance at least one character
        if (start.position() == matchEnd)
            advanceAtLeastOne(start);
    }

    start.endEditBlock();
    unbusy();

    textEdit->setGreenSel(es);
    textEdit->setExtraSelections(composeSelections(textEdit, es));

    // refresh yellow search highlights after the replacements
    hlight();

    QString title;
    if (count == 0)
        title = tr("No Replacement");
    else if (count == 1)
        title = tr("One Replacement");
    else
        title = tr("%Ln Replacements", "", count);

    ui->dockReplace->setWindowTitle(title);
    textEdit->setReplaceTitle(title);

    if (count > 1000 && !txtReplace.isEmpty())
        showWarningBar("<center><b><big>" + tr("The first 1000 replacements are highlighted.") + "</big></b></center>");
}

}  // namespace Texxy
