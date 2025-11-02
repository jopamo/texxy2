// src/features/search/find.cpp

#include "texxywindow.h"
#include "ui_texxywindow.h"

#include <QColor>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTextCursor>
#include <QTextDocument>
#include <algorithm>  // std::clamp, std::max

namespace Texxy {

// selection layering order is preserved
// current line -> replacement -> found matches -> selection highlights -> column highlight -> bracket matches

namespace {

// RAII helper to block all QObject signals temporarily to avoid repaint storms
struct ScopedBlockSignals {
    explicit ScopedBlockSignals(QObject* o) : obj(o) {
        if (obj)
            prev = obj->blockSignals(true);  // save previous state
    }
    ~ScopedBlockSignals() {
        if (obj)
            obj->blockSignals(prev);  // restore previous state
    }
    QPointer<QObject> obj;
    bool prev{false};
};

// compute highlight color once per pass
inline QColor matchColor(const TextEdit* te) {
    if (!te->hasDarkScheme())
        return Qt::yellow;
    const int dv = te->getDarkValue();
    const int alpha = static_cast<int>(static_cast<double>(dv * (dv - 257)) / 414) + 90;
    return QColor(255, 255, 0, std::clamp(alpha, 30, 255));
}

// ensure progress for zero-length regex matches
inline void advanceAtLeastOne(QTextCursor& c) {
    c.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor);
}

}  // namespace

void TexxyWindow::find(bool forward) {
    TabPage* tabPage = nullptr;
    TextEdit* textEdit = nullptr;
    if (!resolveActiveTextEdit(false, &tabPage, &textEdit))
        return;

    const QString txt = tabPage->searchEntry();
    bool newSearch = false;
    if (textEdit->getSearchedText() != txt) {
        textEdit->setSearchedText(txt);
        newSearch = true;
    }

    // pause signals to avoid redundant paints while we adjust the cursor and selections
    ScopedBlockSignals pause(textEdit);

    if (txt.isEmpty()) {
        QList<QTextEdit::ExtraSelection> empty;
        textEdit->setGreenSel(empty);  // clear green selections fast
        textEdit->setExtraSelections(composeSelections(textEdit, empty));
        return;
    }

    const bool useRegex = tabPage->matchRegex();
    const QTextDocument::FindFlags baseFlags = getSearchFlags();
    const QTextDocument::FindFlags flags = forward ? baseFlags : (baseFlags | QTextDocument::FindBackward);

    QTextCursor start = textEdit->textCursor();
    QTextCursor found = textEdit->finding(txt, start, flags, useRegex);

    if (found.isNull()) {
        start.movePosition(forward ? QTextCursor::Start : QTextCursor::End, QTextCursor::MoveAnchor);
        found = textEdit->finding(txt, start, flags, useRegex);
    }

    if (!found.isNull()) {
        start.setPosition(found.anchor());
        if (newSearch)
            textEdit->setTextCursor(start);
        start.setPosition(found.position(), QTextCursor::KeepAnchor);
        textEdit->skipSelectionHighlighting();  // skip transient selection paints
        textEdit->setTextCursor(start);
    }

    // ScopedBlockSignals ends here so hlight can emit selections once
    hlight();
}

/*************************/
// highlight found matches only within the visible viewport for speed
void TexxyWindow::hlight() const {
    TabPage* tabPage = currentTabPage();
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    const QString txt = textEdit->getSearchedText();
    if (txt.isEmpty())
        return;

    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();  // prepend existing green highlights

    const QWidget* vp = textEdit->viewport();
    const QPoint vpTopLeft(0, 0);
    const QPoint vpBottomRight(vp->width() - 1, vp->height() - 1);

    QTextCursor start = textEdit->cursorForPosition(vpTopLeft);
    QTextCursor end = textEdit->cursorForPosition(vpBottomRight);

    const bool useRegex = tabPage->matchRegex();
    const int docLen = std::max(0, textEdit->document()->characterCount() - 1);
    const int ext = useRegex ? 0 : txt.length();
    const int startPos = std::clamp(start.position() - ext, 0, docLen);
    const int endPosSoft = std::clamp(end.position() + ext, 0, docLen);

    start.setPosition(startPos);
    end.setPosition(endPosSoft);

    if (useRegex || (end.position() - start.position()) >= txt.length()) {
        const QColor color = matchColor(textEdit);
        const QTextDocument::FindFlags flags = getSearchFlags();
        const int endLimit = end.position();

        es.reserve(es.size() + 32);  // reduce reallocs under dense matches

        QTextCursor found;
        int lastPos = -1;

        // iterate forward through visible range only
        while (!(found = textEdit->finding(txt, start, flags, useRegex, endLimit)).isNull()) {
            // guard against zero-length progress
            if (found.position() == lastPos) {
                advanceAtLeastOne(start);
                continue;
            }
            lastPos = found.position();

            QTextEdit::ExtraSelection extra;
            extra.format.setBackground(color);
            extra.cursor = found;
            es.append(extra);

            start.setPosition(found.position());
            if (found.position() == found.anchor())
                advanceAtLeastOne(start);
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
    QTextDocument::FindFlags flags{};
    if (tabPage) {
        if (tabPage->matchWhole())
            flags = QTextDocument::FindWholeWords;
        if (tabPage->matchCase())
            flags |= QTextDocument::FindCaseSensitively;
    }
    return flags;
}

}  // namespace Texxy
