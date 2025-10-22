/*
 * texxy/replace.cpp
 */

#include "texxywindow.h"
#include "ui_texxywindow.h"

#include <algorithm>  // std::min
#include <QtGlobal>   // qsizetype

namespace Texxy {

void TexxyWindow::removeGreenSel() {
    // remove green highlights, considering the selection order:
    // current line -> replacement -> found matches -> selection highlights -> column highlight -> bracket matches
    const int count = ui->tabWidget->count();
    const bool lineNumShown = ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible();

    for (int i = 0; i < count; ++i) {
        auto* tab = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
        if (!tab)
            continue;

        TextEdit* textEdit = tab->textEdit();
        QList<QTextEdit::ExtraSelection> es = textEdit->extraSelections();

        // compute how many leading items to drop from the extra selections
        const int offsetForLine = (lineNumShown && !es.isEmpty()) ? 1 : 0;
        const int greenCount = textEdit->getGreenSel().count();
        const qsizetype dropQs = std::min<qsizetype>(es.size(), static_cast<qsizetype>(offsetForLine + greenCount));
        const int drop = static_cast<int>(dropQs);

        // rebuild in O(n) instead of repeated removeFirst
        QList<QTextEdit::ExtraSelection> newEs = es.mid(drop);

        // re-prepend current line selection if needed
        if (lineNumShown)
            newEs.prepend(textEdit->currentLineSelection());

        // clear green selections and apply new list
        textEdit->setGreenSel(QList<QTextEdit::ExtraSelection>());
        textEdit->setExtraSelections(newEs);
    }
}
/*************************/
void TexxyWindow::replaceDock() {
    if (!isReady())
        return;

    if (!ui->dockReplace->isVisible()) {
        const int count = ui->tabWidget->count();
        for (int i = 0; i < count; ++i)  // replace dock needs searchbar
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
    // dockVisibilityChanged(false) is automatically called here
}
/*************************/
// When the dock becomes invisible, clear the replacing text and remove only green highlights
// Although it doesn't concern us, when docking or undocking, the widget first becomes invisible
// for a moment and then visible again
void TexxyWindow::dockVisibilityChanged(bool visible) {
    if (visible || isMinimized())
        return;

    removeGreenSel();

    // return focus to the document and remove the title
    // other titles will be removed by tabSwitch()
    if (ui->tabWidget->count() > 0) {
        auto* tab = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
        if (!tab)
            return;
        TextEdit* textEdit = tab->textEdit();
        textEdit->setFocus();
        textEdit->setReplaceTitle(QString());
    }
}
/*************************/
// Resize the floating dock widget to its minimum size
void TexxyWindow::resizeDock(bool topLevel) {
    if (topLevel)
        ui->dockReplace->resize(ui->dockReplace->minimumWidth(), ui->dockReplace->minimumHeight());
}
/*************************/
void TexxyWindow::replace() {
    if (!isReady())
        return;

    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    if (textEdit->isReadOnly())
        return;

    textEdit->setReplaceTitle(QString());
    ui->dockReplace->setWindowTitle(tr("Replacement"));

    const QString txtFind = ui->lineEditFind->text();
    if (txtFind.isEmpty())
        return;

    const QString txtReplace = ui->lineEditReplace->text();

    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();
    // remove previous green highlights if the replacing text is changed
    if (!es.isEmpty() && es.first().cursor.selectedText() != txtReplace) {
        textEdit->setGreenSel(QList<QTextEdit::ExtraSelection>());
        es.clear();
    }

    const bool lineNumShown = ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible();
    const QTextDocument::FindFlags searchFlags = getSearchFlags();

    // for covering regex capturing groups
    QString realTxtReplace;
    QRegularExpression regexFind;
    if (tabPage->matchRegex()) {
        regexFind = QRegularExpression(txtFind, (searchFlags & QTextDocument::FindCaseSensitively)
                                                    ? QRegularExpression::NoPatternOption
                                                    : QRegularExpression::CaseInsensitiveOption);
    }

    QTextCursor start = textEdit->textCursor();
    QTextCursor tmp = start;
    QTextCursor found;

    const QObject* s = QObject::sender();
    const bool forward = (s == ui->toolButtonNext);
    found = textEdit->finding(txtFind, start, forward ? searchFlags : (searchFlags | QTextDocument::FindBackward),
                              tabPage->matchRegex());

    const QColor color = textEdit->hasDarkScheme() ? QColor(Qt::darkGreen) : QColor(Qt::green);

    if (!found.isNull()) {
        start.setPosition(found.anchor());
        const int pos = found.anchor();
        start.setPosition(found.position(), QTextCursor::KeepAnchor);
        textEdit->skipSelectionHighlighting();
        textEdit->setTextCursor(start);

        if (tabPage->matchRegex())
            textEdit->insertPlainText((realTxtReplace = found.selectedText().replace(regexFind, txtReplace)));
        else
            textEdit->insertPlainText(txtReplace);

        start = textEdit->textCursor();  // now at end of inserted text
        tmp.setPosition(pos);
        tmp.setPosition(start.position(), QTextCursor::KeepAnchor);

        QTextEdit::ExtraSelection extra;
        extra.format.setBackground(color);
        extra.cursor = tmp;
        es.append(extra);

        if (!forward) {
            // for backward replace, move cursor before inserted text to allow repeated replacement
            const int backLen = tabPage->matchRegex() ? realTxtReplace.length() : txtReplace.length();
            start.setPosition(start.position() - backLen);
            textEdit->setTextCursor(start);
        }
    }

    textEdit->setGreenSel(es);

    // rebuild final selection list in one pass
    QList<QTextEdit::ExtraSelection> finalEs;
    finalEs.reserve((lineNumShown ? 1 : 0) + es.size() + textEdit->getBlueSel().size() + textEdit->getColSel().size() +
                    textEdit->getRedSel().size());

    if (lineNumShown)
        finalEs.append(textEdit->currentLineSelection());
    finalEs += es;
    finalEs += textEdit->getBlueSel();
    finalEs += textEdit->getColSel();
    finalEs += textEdit->getRedSel();

    textEdit->setExtraSelections(finalEs);

    // add yellow highlights
    hlight();
}
/*************************/
void TexxyWindow::replaceAll() {
    if (!isReady())
        return;

    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    if (textEdit->isReadOnly())
        return;

    const QString txtFind = ui->lineEditFind->text();
    if (txtFind.isEmpty())
        return;

    const QString txtReplace = ui->lineEditReplace->text();

    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();
    // remove previous green highlights if the replacing text is changed
    if (!es.isEmpty() && es.first().cursor.selectedText() != txtReplace) {
        textEdit->setGreenSel(QList<QTextEdit::ExtraSelection>());
        es.clear();
    }

    const QTextDocument::FindFlags searchFlags = getSearchFlags();

    QRegularExpression regexFind;
    if (tabPage->matchRegex()) {
        regexFind = QRegularExpression(txtFind, (searchFlags & QTextDocument::FindCaseSensitively)
                                                    ? QRegularExpression::NoPatternOption
                                                    : QRegularExpression::CaseInsensitiveOption);
    }

    QTextCursor orig = textEdit->textCursor();
    orig.setPosition(orig.anchor());
    textEdit->setTextCursor(orig);

    const QColor color = textEdit->hasDarkScheme() ? QColor(Qt::darkGreen) : QColor(Qt::green);

    QTextCursor start = orig;
    start.beginEditBlock();
    start.setPosition(0);

    QTextCursor tmp = start;
    int count = 0;

    QTextEdit::ExtraSelection extra;
    extra.format.setBackground(color);

    makeBusy();
    QTextCursor found;
    while (!(found = textEdit->finding(txtFind, start, searchFlags, tabPage->matchRegex())).isNull()) {
        const int anchorPos = found.anchor();

        start.setPosition(anchorPos);
        start.setPosition(found.position(), QTextCursor::KeepAnchor);

        if (tabPage->matchRegex())
            start.insertText(found.selectedText().replace(regexFind, txtReplace));
        else
            start.insertText(txtReplace);

        if (count < 1000) {
            tmp.setPosition(anchorPos);
            tmp.setPosition(start.position(), QTextCursor::KeepAnchor);
            extra.cursor = tmp;
            es.append(extra);
        }

        // keep scanning forward from the end of the inserted text
        start.setPosition(start.position());
        ++count;
    }
    unbusy();

    textEdit->setGreenSel(es);
    start.endEditBlock();

    // build final selection list
    QList<QTextEdit::ExtraSelection> finalEs;
    const bool lineNumShown = ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible();
    if (lineNumShown)
        finalEs.append(textEdit->currentLineSelection());
    finalEs += es;
    finalEs += textEdit->getBlueSel();
    finalEs += textEdit->getColSel();
    finalEs += textEdit->getRedSel();
    textEdit->setExtraSelections(finalEs);

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
