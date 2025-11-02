/*
 * texxy/search/find.cpp
 */

#include "texxywindow.h"
#include "ui_texxywindow.h"
#include <QColor>
#include <QList>
#include <QPoint>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextDocument>
#include <algorithm>  // std::max

namespace Texxy {

/* This order is preserved everywhere for selections:
   current line -> replacement -> found matches -> selection highlights -> column highlight -> bracket matches */

void TexxyWindow::find(bool forward) {
    TabPage* tabPage = nullptr;
    TextEdit* textEdit = nullptr;
    if (!resolveActiveTextEdit(false, &tabPage, &textEdit))
        return;

    const QString txt = tabPage->searchEntry();
    bool newSrch = false;
    if (textEdit->getSearchedText() != txt) {
        textEdit->setSearchedText(txt);
        newSrch = true;
    }

    // reduce redundant paints during search
    disconnect(textEdit, &TextEdit::resized, this, &TexxyWindow::hlight);
    disconnect(textEdit, &TextEdit::updateRect, this, &TexxyWindow::hlight);
    disconnect(textEdit, &QPlainTextEdit::textChanged, this, &TexxyWindow::hlight);

    if (txt.isEmpty()) {
        QList<QTextEdit::ExtraSelection> empty;
        textEdit->setGreenSel(empty);  // clear green selections fast
        textEdit->setExtraSelections(composeSelections(textEdit, empty));
        return;
    }

    const bool rx = tabPage->matchRegex();
    const QTextDocument::FindFlags baseFlags = getSearchFlags();
    const QTextDocument::FindFlags flags = forward ? baseFlags : (baseFlags | QTextDocument::FindBackward);

    QTextCursor start = textEdit->textCursor();
    QTextCursor found = textEdit->finding(txt, start, flags, rx);

    if (found.isNull()) {
        start.movePosition(forward ? QTextCursor::Start : QTextCursor::End, QTextCursor::MoveAnchor);
        found = textEdit->finding(txt, start, flags, rx);
    }

    if (!found.isNull()) {
        start.setPosition(found.anchor());
        if (newSrch)
            textEdit->setTextCursor(start);
        start.setPosition(found.position(), QTextCursor::KeepAnchor);
        textEdit->skipSelectionHighlighting();  // skip transient selection paints
        textEdit->setTextCursor(start);
    }

    hlight();

    // reconnect after highlight
    connect(textEdit, &QPlainTextEdit::textChanged, this, &TexxyWindow::hlight);
    connect(textEdit, &TextEdit::updateRect, this, &TexxyWindow::hlight);
    connect(textEdit, &TextEdit::resized, this, &TexxyWindow::hlight);
}

/*************************/
// Highlight found matches in the visible part of the text
void TexxyWindow::hlight() const {
    TabPage* tabPage = currentTabPage();
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    const QString txt = textEdit->getSearchedText();
    if (txt.isEmpty())
        return;

    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();  // prepend green highlights

    const QWidget* vp = textEdit->viewport();
    const QPoint vpTopLeft(0, 0);
    const QPoint vpBottomRight(vp->width() - 1, vp->height() - 1);

    QTextCursor start = textEdit->cursorForPosition(vpTopLeft);
    QTextCursor end = textEdit->cursorForPosition(vpBottomRight);

    const bool rx = tabPage->matchRegex();
    const int ext = rx ? 0 : txt.length();
    const int startPos = std::max(0, start.position() - ext);
    const int endPosSoft = end.position() + ext;

    start.setPosition(startPos);
    end.setPosition(endPosSoft);

    if (rx || (end.position() - start.position()) >= txt.length()) {
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
        while (!(found = textEdit->finding(txt, start, flags, rx, endLimit)).isNull()) {
            QTextEdit::ExtraSelection extra;
            extra.format.setBackground(color);
            extra.cursor = found;
            es.append(extra);
            start.setPosition(found.position());
        }
    }

    textEdit->setExtraSelections(composeSelections(textEdit, es));
}

/*************************/
void TexxyWindow::searchFlagChanged() {
    TabPage* tabPage = nullptr;
    TextEdit* textEdit = nullptr;
    if (!resolveActiveTextEdit(false, &tabPage, &textEdit))
        return;

    // deselect text for consistency and to reduce subsequent paints
    QTextCursor c = textEdit->textCursor();
    if (c.hasSelection()) {
        c.setPosition(c.anchor());
        textEdit->setTextCursor(c);
    }

    hlight();
}

/*************************/
QTextDocument::FindFlags TexxyWindow::getSearchFlags() const {
    TabPage* tabPage = currentTabPage();
    QTextDocument::FindFlags flags;
    if (tabPage) {
        if (tabPage->matchWhole())
            flags = QTextDocument::FindWholeWords;
        if (tabPage->matchCase())
            flags |= QTextDocument::FindCaseSensitively;
    }
    return flags;
}

}  // namespace Texxy
