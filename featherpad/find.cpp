/*
 * texxy/find.cpp
 */

#include "fpwin.h"
#include "ui_fp.h"
#include <QTextDocument>
#include <QTextCursor>
#include <QPlainTextEdit>

namespace FeatherPad {

/* This order is preserved everywhere for selections:
   current line -> replacement -> found matches -> selection highlights -> column highlight -> bracket matches */

void FPwin::find(bool forward) {
    if (!isReady())
        return;

    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    const QString txt = tabPage->searchEntry();
    bool newSrch = false;
    if (textEdit->getSearchedText() != txt) {
        textEdit->setSearchedText(txt);
        newSrch = true;
    }

    disconnect(textEdit, &TextEdit::resized, this, &FPwin::hlight);
    disconnect(textEdit, &TextEdit::updateRect, this, &FPwin::hlight);
    disconnect(textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);

    if (txt.isEmpty()) {
        QList<QTextEdit::ExtraSelection> es;
        textEdit->setGreenSel(es);  // clear green selections fast
        if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
            es.prepend(textEdit->currentLineSelection());
        es.append(textEdit->getBlueSel());
        es.append(textEdit->getColSel());
        es.append(textEdit->getRedSel());
        textEdit->setExtraSelections(es);
        return;
    }

    const QTextDocument::FindFlags baseFlags = getSearchFlags();
    const QTextDocument::FindFlags flags = forward ? baseFlags : (baseFlags | QTextDocument::FindBackward);

    QTextCursor start = textEdit->textCursor();
    QTextCursor found = textEdit->finding(txt, start, flags, tabPage->matchRegex());

    if (found.isNull()) {
        // wrap once to the opposite edge to avoid scanning entire doc twice
        start.movePosition(forward ? QTextCursor::Start : QTextCursor::End, QTextCursor::MoveAnchor);
        found = textEdit->finding(txt, start, flags, tabPage->matchRegex());
    }

    if (!found.isNull()) {
        start.setPosition(found.anchor());
        if (newSrch)  // ensure selectionChanged is emitted consistently
            textEdit->setTextCursor(start);
        start.setPosition(found.position(), QTextCursor::KeepAnchor);
        textEdit->skipSelectionHighlighting();  // skip transient selection paints
        textEdit->setTextCursor(start);
    }

    hlight();

    connect(textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
    connect(textEdit, &TextEdit::updateRect, this, &FPwin::hlight);
    connect(textEdit, &TextEdit::resized, this, &FPwin::hlight);
}

/*************************/
// Highlight found matches in the visible part of the text
void FPwin::hlight() const {
    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    const QString txt = textEdit->getSearchedText();
    if (txt.isEmpty())
        return;

    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();  // prepend green highlights

    // compute visible range using viewport coordinates for speed and correctness
    const QWidget* vp = textEdit->viewport();
    QPoint vpTopLeft(0, 0);
    QPoint vpBottomRight(vp->width() - 1, vp->height() - 1);

    QTextCursor start = textEdit->cursorForPosition(vpTopLeft);
    QTextCursor end = textEdit->cursorForPosition(vpBottomRight);

    // extend the scan window a little so matches straddling edges are included
    const int ext = (!tabPage->matchRegex() ? txt.length() : 0);
    const int startPos = qMax(0, start.position() - ext);
    const int endPosSoft = end.position() + ext;

    start.setPosition(startPos);
    end.setPosition(endPosSoft);

    // quick reject if scan window is smaller than the literal search text
    if (tabPage->matchRegex() || (end.position() - start.position()) >= txt.length()) {
        // compute highlight color with opacity tuned for dark and light schemes
        QColor color =
            textEdit->hasDarkScheme()
                ? QColor(255, 255, 0,
                         static_cast<int>(
                             static_cast<double>(textEdit->getDarkValue() * (textEdit->getDarkValue() - 257)) / 414) +
                             90)
                : Qt::yellow;

        const QTextDocument::FindFlags flags = getSearchFlags();
        const int endLimit = end.position();
        QTextCursor found;

        // iterate forward through visible range only
        while (!(found = textEdit->finding(txt, start, flags, tabPage->matchRegex(), endLimit)).isNull()) {
            QTextEdit::ExtraSelection extra;
            extra.format.setBackground(color);
            extra.cursor = found;
            es.append(extra);
            start.setPosition(found.position());
        }
    }

    if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
        es.prepend(textEdit->currentLineSelection());  // keep current line first when present

    es.append(textEdit->getBlueSel());
    es.append(textEdit->getColSel());
    es.append(textEdit->getRedSel());

    textEdit->setExtraSelections(es);
}

/*************************/
void FPwin::searchFlagChanged() {
    if (!isReady())
        return;

    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();

    // deselect text for consistency and to reduce subsequent paints
    QTextCursor c = textEdit->textCursor();
    if (c.hasSelection()) {
        c.setPosition(c.anchor());
        textEdit->setTextCursor(c);
    }

    hlight();
}

/*************************/
QTextDocument::FindFlags FPwin::getSearchFlags() const {
    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    QTextDocument::FindFlags flags;
    if (tabPage) {
        if (tabPage->matchWhole())
            flags = QTextDocument::FindWholeWords;
        if (tabPage->matchCase())
            flags |= QTextDocument::FindCaseSensitively;
    }
    return flags;
}

}  // namespace FeatherPad
