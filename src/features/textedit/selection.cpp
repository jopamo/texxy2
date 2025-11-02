#include "textedit/textedit_prelude.h"

namespace Texxy {

/*************************/
void TextEdit::onSelectionChanged() {
    // bracket matching isn't based only on cursorPositionChanged because removing a selection at its start won't emit
    // it in such cases emit updateBracketMatching manually
    const QTextCursor cur = textCursor();
    if (!cur.hasSelection()) {
        if (cur.position() == prevPos_ && cur.position() < prevAnchor_)
            emit updateBracketMatching();
        prevAnchor_ = prevPos_ = -1;
    }
    else {
        prevAnchor_ = cur.anchor();
        prevPos_ = cur.position();
    }

    if (selectionTimerId_) {
        killTimer(selectionTimerId_);
        selectionTimerId_ = 0;
    }
    selectionTimerId_ = startTimer(kUpdateIntervalMs);

    // selection highlighting
    if (!selectionHighlighting_)
        return;

    if (highlightThisSelection_) {
        removeSelectionHighlights_ = false;  // reset
    }
    else {
        removeSelectionHighlights_ = true;
        highlightThisSelection_ = true;  // reset
    }
}

/*************************/
void TextEdit::setSelectionHighlighting(bool enable) {
    selectionHighlighting_ = enable;
    highlightThisSelection_ = true;     // reset
    removeSelectionHighlights_ = true;  // start without highlighting if enable is true

    if (enable) {
        // avoid duplicate connections when toggled repeatedly
        connect(document(), &QTextDocument::contentsChange, this, &TextEdit::onContentsChange, Qt::UniqueConnection);
        connect(this, &TextEdit::updateRect, this, &TextEdit::selectionHlight, Qt::UniqueConnection);
        connect(this, &TextEdit::resized, this, &TextEdit::selectionHlight, Qt::UniqueConnection);
    }
    else {
        disconnect(document(), &QTextDocument::contentsChange, this, &TextEdit::onContentsChange);
        disconnect(this, &TextEdit::updateRect, this, &TextEdit::selectionHlight);
        disconnect(this, &TextEdit::resized, this, &TextEdit::selectionHlight);

        // remove all blue highlights
        if (!blueSel_.isEmpty()) {
            QList<QTextEdit::ExtraSelection> es = extraSelections();
            const int nCol = colSel_.count();
            const int nRed = redSel_.count();
            int n = blueSel_.count();
            while (n > 0 && es.size() - nCol - nRed > 0) {
                es.removeAt(es.size() - 1 - nCol - nRed);
                --n;
            }
            blueSel_.clear();
            setExtraSelections(es);
        }
    }
}

/*************************/
// set the blue selection highlights before the red bracket highlights
void TextEdit::selectionHlight() {
    if (!selectionHighlighting_)
        return;

    QList<QTextEdit::ExtraSelection> es = extraSelections();
    const QTextCursor selCursor = textCursor();
    const int selStart = std::min(selCursor.anchor(), selCursor.position());
    const int selEnd = std::max(selCursor.anchor(), selCursor.position());
    const int selLen = selEnd - selStart;

    const int nCol = colSel_.count();  // column highlight comes last but one
    const int nRed = redSel_.count();  // bracket highlights come last

    // remove all blue highlights
    int n = blueSel_.count();
    while (n > 0 && es.size() - nCol - nRed > 0) {
        es.removeAt(es.size() - 1 - nCol - nRed);
        --n;
    }

    // clear when disabled, empty, or absurdly large to avoid heavy scans
    if (removeSelectionHighlights_ || selLen <= 0 || selLen > 100000) {
        if (!blueSel_.isEmpty()) {
            blueSel_.clear();
            setExtraSelections(es);
        }
        return;
    }

    // restrict search to the visible viewport to avoid scanning the entire document
    QPoint tl(0, 0);
    QPoint br = viewport()->rect().bottomRight();

    QTextCursor start = cursorForPosition(tl);  // top-left of viewport
    QTextCursor end = cursorForPosition(br);    // bottom-right of viewport

    // move start backward by selection length, but keep it outside the active selection
    const int startPos = start.position() - selLen;
    start.setPosition(std::max(0, startPos));
    if (start.position() >= selStart && start.position() < selEnd)
        start.setPosition(selEnd);

    // move end forward by selection length up to document end, but keep it outside the active selection
    const int endTarget = end.position() + selLen;
    end.movePosition(QTextCursor::End);
    if (endTarget <= end.position())
        end.setPosition(endTarget);
    if (end.position() > selStart && end.position() <= selEnd)
        end.setPosition(selStart);

    // if the visible window cannot even contain one more occurrence of the selection, skip work
    if (end.position() - start.position() < selLen) {
        if (!blueSel_.isEmpty()) {
            blueSel_.clear();
            setExtraSelections(es);
        }
        return;
    }

    blueSel_.clear();

    const QString selTxt = selCursor.selection().toPlainText();
    const QTextDocument::FindFlags flags = QTextDocument::FindWholeWords | QTextDocument::FindCaseSensitively;
    const QColor color = hasDarkScheme() ? QColor(0, 77, 160) : QColor(130, 255, 255);  // blue highlights

    const int endLimit = end.anchor();
    QTextCursor found;

    while (!(found = finding(selTxt, start, flags, false, endLimit)).isNull()) {
        // avoid re-highlighting the active selection
        if (found.anchor() >= selEnd || found.position() <= selStart) {
            QTextEdit::ExtraSelection extra;
            extra.format.setBackground(color);
            extra.cursor = found;
            blueSel_.append(extra);
            es.insert(es.size() - nCol - nRed, extra);
        }
        start.setPosition(found.position());
    }

    setExtraSelections(es);
}

/*************************/
void TextEdit::onContentsChange(int /*position*/, int charsRemoved, int charsAdded) {
    if (!selectionHighlighting_)
        return;

    if (charsRemoved > 0 || charsAdded > 0) {
        // defer until layout updates so end cursor stays in range
        QTimer::singleShot(0, this, &TextEdit::selectionHlight);
    }
}

/*************************/
bool TextEdit::toSoftTabs() {
    bool res = false;
    const QString tab = QString(QChar(QChar::Tabulation));
    QTextCursor orig = textCursor();
    orig.setPosition(orig.anchor());
    setTextCursor(orig);

    QTextCursor start = orig;
    start.beginEditBlock();
    start.setPosition(0);

    QTextCursor found;
    while (!(found = finding(tab, start)).isNull()) {
        res = true;
        start.setPosition(found.anchor());
        const QString softTab = remainingSpaces(textTab_, start);
        start.setPosition(found.position(), QTextCursor::KeepAnchor);
        start.insertText(softTab);
        start.setPosition(start.position());
    }

    start.endEditBlock();
    return res;
}

}  // namespace Texxy
