// src/features/textedit/input.cpp
#include "textedit/textedit_prelude.h"

namespace Texxy {

/*************************/
void TextEdit::keyPressEvent(QKeyEvent* event) {
    keepTxtCurHPos_ = false;

    // first, handle special cases of pressing Ctrl
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->modifiers() == Qt::ControlModifier) {  // no other modifier is pressed
            // deal with hyperlinks
            if (event->key() == Qt::Key_Control) {  // no other key is pressed either
                if (highlighter_) {
                    const auto pos = viewport()->mapFromGlobal(QCursor::pos());
                    const bool hasUrl = !getUrl(cursorForPosition(pos).position()).isEmpty();
                    const auto want = hasUrl ? Qt::PointingHandCursor : Qt::IBeamCursor;
                    if (viewport()->cursor().shape() != want)
                        viewport()->setCursor(want);
                    QPlainTextEdit::keyPressEvent(event);
                    return;
                }
            }
        }
        if (event->key() != Qt::Key_Control) {  // another modifier/key is pressed
            if (highlighter_ && viewport()->cursor().shape() != Qt::IBeamCursor)
                viewport()->setCursor(Qt::IBeamCursor);
        }
    }

    // workarounds for copy/cut/... -- see TextEdit::copy()/cut()/...
    if (event->matches(QKeySequence::Copy)) {
        copy();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Cut)) {
        if (!isReadOnly())
            cut();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        if (!isReadOnly())
            paste();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        selectAll();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Undo)) {
        // QWidgetTextControl::undo ensures cursor visible even when nothing to undo
        // avoid confusing scroll jumps by checking availability first
        if (!isReadOnly() && document()->isUndoAvailable())
            undo();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Redo)) {
        // same rationale as Undo
        if (!isReadOnly() && document()->isRedoAvailable())
            redo();
        event->accept();
        return;
    }

    if (isReadOnly()) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    if (event->matches(QKeySequence::Delete) || event->matches(QKeySequence::DeleteStartOfWord)) {
        if (!colSel_.isEmpty() && event->matches(QKeySequence::Delete)) {
            deleteColumn();
            event->accept();
            return;
        }
        const bool hadSelection = textCursor().hasSelection();
        QPlainTextEdit::keyPressEvent(event);
        if (!hadSelection)
            emit updateBracketMatching();  // not emitted otherwise
        return;
    }

    if (event->key() == Qt::Key_Backspace) {
        if (!colSel_.isEmpty()) {
            keepTxtCurHPos_ = false;
            txtCurHPos_ = -1;
            if (colSel_.count() > 1000) {
                QTimer::singleShot(0, this, [this]() { emit hugeColumn(); });
            }
            else {
                const bool savedPressed = mousePressed_;
                mousePressed_ = true;
                QTextCursor cur = textCursor();
                cur.beginEditBlock();
                for (const auto& extra : std::as_const(colSel_)) {
                    if (!extra.cursor.hasSelection())
                        continue;
                    cur = extra.cursor;
                    cur.setPosition(std::min(cur.anchor(), cur.position()));
                    if (cur.columnNumber() == 0)
                        continue;  // don't go to the previous line
                    cur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                    cur.insertText(QString());
                }
                cur.endEditBlock();
                mousePressed_ = savedPressed;
                if (lineWrapMode() != QPlainTextEdit::NoWrap) {  // selection may have changed with wrapped text
                    if (selectionTimerId_) {
                        killTimer(selectionTimerId_);
                        selectionTimerId_ = 0;
                    }
                    selectionTimerId_ = startTimer(kUpdateIntervalMs);
                }
            }
            event->accept();
            return;
        }
        keepTxtCurHPos_ = true;
        if (txtCurHPos_ < 0) {
            QTextCursor startCur = textCursor();
            startCur.movePosition(QTextCursor::StartOfLine);
            txtCurHPos_ = std::abs(cursorRect().left() - cursorRect(startCur).left());  // negative for RTL
        }
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        removeColumnHighlight();
        keepTxtCurHPos_ = true;
        if (txtCurHPos_ < 0) {
            QTextCursor startCur = textCursor();
            startCur.movePosition(QTextCursor::StartOfLine);
            txtCurHPos_ = std::abs(cursorRect().left() - cursorRect(startCur).left());
        }

        QTextCursor cur = textCursor();
        const QString selTxt = cur.selectedText();

        if (autoReplace_ && selTxt.isEmpty()) {
            const int p = cur.positionInBlock();
            if (p > 1) {
                bool replaceStr = true;
                cur.beginEditBlock();
                if (prog_ == "url" || prog_ == "changelog" || lang_ == "url" || lang_ == "changelog") {  // not for code
                    cur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 2);
                    const QString sel = cur.selectedText();
                    replaceStr = sel.endsWith('.');
                    if (!replaceStr) {
                        if (sel == "--") {
                            QTextCursor prevCur = cur;
                            prevCur.setPosition(cur.position());
                            prevCur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                            if (prevCur.selectedText() != "-")
                                cur.insertText("—");
                        }
                        else if (sel == "->") {
                            cur.insertText("→");
                        }
                        else if (sel == "<-") {
                            cur.insertText("←");
                        }
                        else if (sel == ">=") {
                            cur.insertText("≥");
                        }
                        else if (sel == "<=") {
                            cur.insertText("≤");
                        }
                    }
                }
                if (replaceStr && p > 2) {
                    cur = textCursor();
                    cur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 3);
                    const QString sel3 = cur.selectedText();
                    if (sel3 == "...") {
                        QTextCursor prevCur = cur;
                        prevCur.setPosition(cur.position());
                        if (p > 3) {
                            prevCur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                            if (prevCur.selectedText() != ".")
                                cur.insertText("…");
                        }
                        else {
                            cur.insertText("…");
                        }
                    }
                }
                cur.endEditBlock();
                cur = textCursor();  // reset the current cursor
            }
        }

        bool isBracketed = false;
        QString prefix, indent;
        const bool withShift = event->modifiers() & Qt::ShiftModifier;

        // with Shift+Enter, find the non-letter prefix
        if (withShift) {
            cur.clearSelection();
            setTextCursor(cur);
            const QString blockText = cur.block().text();
            int i = 0;
            const int curBlockPos = cur.position() - cur.block().position();
            while (i < curBlockPos) {
                const QChar ch = blockText.at(i);
                if (!ch.isLetterOrNumber()) {
                    prefix += ch;
                    ++i;
                }
                else {
                    break;
                }
            }
            // still check if a letter or number follows
            if (i < curBlockPos) {
                const QChar c = blockText.at(i);
                if (c.isLetter()) {
                    if (i + 1 < curBlockPos && !prefix.isEmpty() && !prefix.at(prefix.size() - 1).isSpace() &&
                        blockText.at(i + 1).isSpace()) {  // non-space symbol -> single letter -> space
                        prefix = blockText.left(i + 2);
                        QChar cc = QChar(c.unicode() + 1);
                        if (cc.isLetter())
                            prefix.replace(c, cc);
                    }
                    else if (i + 2 < curBlockPos && !blockText.at(i + 1).isLetterOrNumber() &&
                             !blockText.at(i + 1).isSpace() &&
                             blockText.at(i + 2).isSpace()) {  // letter -> symbol -> space
                        prefix = blockText.left(i + 3);
                        QChar cc = QChar(c.unicode() + 1);
                        if (cc.isLetter())
                            prefix.replace(c, cc);
                    }
                }
                else if (c.isNumber()) {  // numbered lists
                    QString num;
                    while (i < curBlockPos) {
                        const QChar ch = blockText.at(i);
                        if (ch.isNumber()) {
                            num += ch;
                            ++i;
                        }
                        else {
                            if (!num.isEmpty()) {
                                QLocale l = locale();
                                l.setNumberOptions(QLocale::OmitGroupSeparator);
                                const QChar ch2 = blockText.at(i);
                                if (ch2.isSpace()) {  // symbol -> number -> space
                                    if (!prefix.isEmpty() && !prefix.at(prefix.size() - 1).isSpace())
                                        num = l.toString(locale().toInt(num) + 1) + ch2;
                                    else
                                        num = QString();
                                }
                                else if (i + 1 < curBlockPos && !ch2.isLetterOrNumber() && !ch2.isSpace() &&
                                         blockText.at(i + 1).isSpace()) {  // number -> symbol -> space
                                    num = l.toString(locale().toInt(num) + 1) + ch2 + blockText.at(i + 1);
                                }
                                else {
                                    num = QString();
                                }
                            }
                            break;
                        }
                    }
                    if (i < curBlockPos)  // otherwise it will be just a number
                        prefix += num;
                }
            }
        }
        else {
            // find indentation
            if (autoIndentation_)
                indent = computeIndentation(cur);
            // check whether a bracketed text is selected with caret at start
            QTextCursor anchorCur = cur;
            anchorCur.setPosition(cur.anchor());
            if (autoBracket_ && cur.position() == cur.selectionStart() && !cur.atBlockStart() &&
                !anchorCur.atBlockEnd()) {
                cur.setPosition(cur.position());
                cur.movePosition(QTextCursor::PreviousCharacter);
                cur.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, selTxt.size() + 2);
                const QString selTxt1 = cur.selectedText();
                if (selTxt1 == "{" + selTxt + "}" || selTxt1 == "(" + selTxt + ")")
                    isBracketed = true;
                cur = textCursor();  // reset
            }
        }

        if (withShift || autoIndentation_ || isBracketed) {
            cur.beginEditBlock();
            // first press Enter normally
            cur.insertText(QChar(QChar::ParagraphSeparator));
            // then insert indentation
            cur.insertText(indent);
            // handle Shift+Enter or brackets
            if (withShift) {
                cur.insertText(prefix);
            }
            else if (isBracketed) {
                cur.movePosition(QTextCursor::PreviousBlock);
                cur.movePosition(QTextCursor::EndOfBlock);
                int start = -1;
                const QStringList lines = selTxt.split(QChar(QChar::ParagraphSeparator));
                if (lines.size() == 1) {
                    cur.insertText(QChar(QChar::ParagraphSeparator));
                    cur.insertText(indent);
                    start = cur.position();
                    if (!isOnlySpaces(lines.at(0)))
                        cur.insertText(lines.at(0));
                }
                else {  // multi-line
                    for (int i = 0; i < lines.size(); ++i) {
                        if (i == 0 && isOnlySpaces(lines.at(0)))
                            continue;
                        cur.insertText(QChar(QChar::ParagraphSeparator));
                        if (i == 0) {
                            cur.insertText(indent);
                            start = cur.position();
                        }
                        else if (i == 1 && start == -1) {
                            start = cur.position();  // the first line was only spaces
                        }
                        cur.insertText(lines.at(i));
                    }
                }
                cur.setPosition(start,
                                start >= cur.block().position() ? QTextCursor::MoveAnchor : QTextCursor::KeepAnchor);
                setTextCursor(cur);
            }
            cur.endEditBlock();
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_ParenLeft || event->key() == Qt::Key_BraceLeft ||
             event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_QuoteDbl) {
        if (!colSel_.isEmpty()) {
            prependToColumn(event);
            return;
        }
        if (autoBracket_) {
            QTextCursor cursor = textCursor();
            bool autoB = false;
            if (!cursor.hasSelection()) {
                if (cursor.atBlockEnd())
                    autoB = true;
                else {
                    QTextCursor tmp = cursor;
                    tmp.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                    if (!tmp.selectedText().at(0).isLetterOrNumber())
                        autoB = true;
                }
            }
            else if (cursor.position() == cursor.selectionStart()) {
                autoB = true;
            }
            if (autoB) {
                const int pos = cursor.position();
                const int anch = cursor.anchor();
                cursor.beginEditBlock();
                cursor.setPosition(anch);
                if (event->key() == Qt::Key_ParenLeft) {
                    cursor.insertText(")");
                    cursor.setPosition(pos);
                    cursor.insertText("(");
                }
                else if (event->key() == Qt::Key_BraceLeft) {
                    cursor.insertText("}");
                    cursor.setPosition(pos);
                    cursor.insertText("{");
                }
                else if (event->key() == Qt::Key_BracketLeft) {
                    cursor.insertText("]");
                    cursor.setPosition(pos);
                    cursor.insertText("[");
                }
                else {  // Qt::Key_QuoteDbl
                    cursor.insertText("\"");
                    cursor.setPosition(pos);
                    cursor.insertText("\"");
                }
                // select the text and set the cursor at its start
                cursor.setPosition(anch + 1, QTextCursor::MoveAnchor);
                cursor.setPosition(pos + 1, QTextCursor::KeepAnchor);
                cursor.endEditBlock();
                highlightThisSelection_ = false;
                // WARNING: putting setTextCursor before endEditBlock crashes on huge lines in some Qt versions
                setTextCursor(cursor);
                event->accept();
                return;
            }
        }
    }
    else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        mousePressed_ = false;  // remove column highlighting
        // when text is selected, use arrows to jump to start or end of the selection
        QTextCursor cursor = textCursor();
        if (event->modifiers() == Qt::NoModifier && cursor.hasSelection()) {
            const QString selTxt = cursor.selectedText();
            if (event->key() == Qt::Key_Left) {
                if (selTxt.isRightToLeft())
                    cursor.setPosition(cursor.selectionEnd());
                else
                    cursor.setPosition(cursor.selectionStart());
            }
            else {
                if (selTxt.isRightToLeft())
                    cursor.setPosition(cursor.selectionStart());
                else
                    cursor.setPosition(cursor.selectionEnd());
            }
            cursor.clearSelection();
            setTextCursor(cursor);
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Up) {
        mousePressed_ = false;  // remove column highlighting
        if (event->modifiers() == Qt::ControlModifier) {
            if (QScrollBar* vbar = verticalScrollBar()) {  // scroll without changing the cursor position
                vbar->setValue(vbar->value() + (event->key() == Qt::Key_Down ? 1 : -1));
                event->accept();
                return;
            }
        }
        else if (event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {  // move the line(s)
            QTextCursor cursor = textCursor();
            const int anch = cursor.anchor();
            const int pos = cursor.position();

            QTextCursor tmp = cursor;
            tmp.setPosition(anch);
            const int anchorInBlock = tmp.positionInBlock();
            tmp.setPosition(pos);
            const int posInBlock = tmp.positionInBlock();

            if (event->key() == Qt::Key_Down) {
                highlightThisSelection_ = false;
                cursor.beginEditBlock();
                cursor.setPosition(std::min(anch, pos));
                cursor.movePosition(QTextCursor::StartOfBlock);
                cursor.setPosition(std::max(anch, pos), QTextCursor::KeepAnchor);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                const QString txt = cursor.selectedText();
                if (cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor)) {
                    cursor.deleteChar();
                    cursor.movePosition(QTextCursor::EndOfBlock);
                    cursor.insertText(QString(QChar::ParagraphSeparator));
                    const int firstLine = cursor.position();
                    cursor.insertText(txt);
                    if (anch >= pos) {
                        cursor.setPosition(cursor.block().position() + anchorInBlock);
                        cursor.setPosition(firstLine + posInBlock, QTextCursor::KeepAnchor);
                    }
                    else {
                        cursor.movePosition(QTextCursor::StartOfBlock);
                        const int lastLine = cursor.position();
                        cursor.setPosition(firstLine + anchorInBlock);
                        cursor.setPosition(lastLine + posInBlock, QTextCursor::KeepAnchor);
                    }
                    cursor.endEditBlock();
                    setTextCursor(cursor);
                    ensureCursorVisible();
                    event->accept();
                    return;
                }
                else {
                    cursor.endEditBlock();
                }
            }
            else {
                highlightThisSelection_ = false;
                cursor.beginEditBlock();
                cursor.setPosition(std::max(anch, pos));
                cursor.movePosition(QTextCursor::EndOfBlock);
                cursor.setPosition(std::min(anch, pos), QTextCursor::KeepAnchor);
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
                const QString txt = cursor.selectedText();
                if (cursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::KeepAnchor)) {
                    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                    cursor.deleteChar();
                    cursor.movePosition(QTextCursor::StartOfBlock);
                    const int firstLine = cursor.position();
                    cursor.insertText(txt);
                    cursor.insertText(QString(QChar::ParagraphSeparator));
                    cursor.movePosition(QTextCursor::PreviousBlock);
                    if (anch >= pos) {
                        cursor.setPosition(cursor.block().position() + anchorInBlock);
                        cursor.setPosition(firstLine + posInBlock, QTextCursor::KeepAnchor);
                    }
                    else {
                        const int lastLine = cursor.position();
                        cursor.setPosition(firstLine + anchorInBlock);
                        cursor.setPosition(lastLine + posInBlock, QTextCursor::KeepAnchor);
                    }
                    cursor.endEditBlock();
                    setTextCursor(cursor);
                    ensureCursorVisible();
                    event->accept();
                    return;
                }
                else {
                    cursor.endEditBlock();
                }
            }
        }
        else if (event->modifiers() == Qt::NoModifier ||
                 (!(event->modifiers() & Qt::AltModifier) &&
                  ((event->modifiers() & Qt::ShiftModifier) || (event->modifiers() & Qt::MetaModifier) ||
                   (event->modifiers() & Qt::KeypadModifier)))) {
            // Down/Up after Backspace/Enter keep pixel horizontal position
            keepTxtCurHPos_ = true;
            QTextCursor cursor = textCursor();
            int hPos;
            if (txtCurHPos_ >= 0) {
                hPos = txtCurHPos_;
            }
            else {
                QTextCursor startCur = cursor;
                startCur.movePosition(QTextCursor::StartOfLine);
                hPos = std::abs(cursorRect().left() - cursorRect(startCur).left());  // negative for RTL
                txtCurHPos_ = hPos;
            }
            const QTextCursor::MoveMode mode =
                ((event->modifiers() & Qt::ShiftModifier) ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
            if (event->modifiers() & Qt::MetaModifier) {  // restore cursor pixel position between blocks
                cursor.movePosition(event->key() == Qt::Key_Down ? QTextCursor::EndOfBlock : QTextCursor::StartOfBlock,
                                    mode);
                if (cursor.movePosition(
                        event->key() == Qt::Key_Down ? QTextCursor::NextBlock : QTextCursor::PreviousBlock, mode)) {
                    setTextCursor(cursor);  // needed due to a Qt bug
                    const bool rtl = cursor.block().textDirection() == Qt::RightToLeft;
                    const QPoint cc = cursorRect(cursor).center();
                    cursor.setPosition(cursorForPosition(QPoint(cc.x() + (rtl ? -1 : 1) * hPos, cc.y())).position(),
                                       mode);
                }
            }
            else {  // restore cursor pixel position between lines
                cursor.movePosition(event->key() == Qt::Key_Down ? QTextCursor::EndOfLine : QTextCursor::StartOfLine,
                                    mode);
                if (cursor.movePosition(
                        event->key() == Qt::Key_Down ? QTextCursor::NextCharacter : QTextCursor::PreviousCharacter,
                        mode)) {  // next/prev line or block
                    cursor.movePosition(QTextCursor::StartOfLine, mode);
                    setTextCursor(cursor);  // needed due to a Qt bug
                    const bool rtl = cursor.block().textDirection() == Qt::RightToLeft;
                    const QPoint cc = cursorRect(cursor).center();
                    cursor.setPosition(cursorForPosition(QPoint(cc.x() + (rtl ? -1 : 1) * hPos, cc.y())).position(),
                                       mode);
                }
            }
            setTextCursor(cursor);
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_PageDown || event->key() == Qt::Key_PageUp) {
        mousePressed_ = false;  // remove column highlighting
        if (event->modifiers() == Qt::ControlModifier) {
            if (QScrollBar* vbar = verticalScrollBar()) {  // scroll without changing the cursor position
                vbar->setValue(vbar->value() + (event->key() == Qt::Key_PageDown ? 1 : -1) * vbar->pageStep());
                event->accept();
                return;
            }
        }
    }
    else if (event->key() == Qt::Key_Tab) {
        QTextCursor cursor = textCursor();
        const int newLines = cursor.selectedText().count(QChar(QChar::ParagraphSeparator));
        if (newLines > 0) {
            highlightThisSelection_ = false;
            cursor.beginEditBlock();
            cursor.setPosition(std::min(cursor.anchor(), cursor.position()));  // go to the first block
            cursor.movePosition(QTextCursor::StartOfBlock);
            for (int i = 0; i <= newLines; ++i) {
                // skip leading spaces to align the real text
                int indx = 0;
                QRegularExpressionMatch match;
                if (cursor.block().text().indexOf(QRegularExpression("^\\s+"), 0, &match) > -1)
                    indx = match.capturedLength();
                cursor.setPosition(cursor.block().position() + indx);
                if (event->modifiers() & Qt::ControlModifier) {
                    cursor.insertText(remainingSpaces(event->modifiers() & Qt::MetaModifier ? "  " : textTab_, cursor));
                }
                else {
                    cursor.insertText("\t");
                }
                if (!cursor.movePosition(QTextCursor::NextBlock))
                    break;  // not needed
            }
            cursor.endEditBlock();
            ensureCursorVisible();
            event->accept();
            return;
        }
        else if (event->modifiers() & Qt::ControlModifier) {
            if (!colSel_.isEmpty()) {
                if (colSel_.count() > 1000) {
                    QTimer::singleShot(0, this, [this]() { emit hugeColumn(); });
                }
                else {
                    const bool savedPressed = mousePressed_;
                    mousePressed_ = true;
                    const QString spaceTab(event->modifiers() & Qt::MetaModifier ? "  " : textTab_);
                    cursor.beginEditBlock();
                    for (const auto& extra : std::as_const(colSel_)) {
                        if (!extra.cursor.hasSelection())
                            continue;
                        cursor = extra.cursor;
                        cursor.setPosition(std::min(cursor.anchor(), cursor.position()));
                        cursor.insertText(remainingSpaces(spaceTab, cursor));
                    }
                    cursor.endEditBlock();
                    mousePressed_ = savedPressed;
                }
                event->accept();
                return;
            }
            QTextCursor tmp(cursor);
            tmp.setPosition(std::min(tmp.anchor(), tmp.position()));
            cursor.insertText(remainingSpaces(event->modifiers() & Qt::MetaModifier ? "  " : textTab_, tmp));
            ensureCursorVisible();
            event->accept();
            return;
        }
        else if (!colSel_.isEmpty()) {
            prependToColumn(event);
            return;
        }
    }
    else if (event->key() == Qt::Key_Backtab) {
        removeColumnHighlight();
        QTextCursor cursor = textCursor();
        const int newLines = cursor.selectedText().count(QChar(QChar::ParagraphSeparator));
        cursor.setPosition(std::min(cursor.anchor(), cursor.position()));
        highlightThisSelection_ = false;
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::StartOfBlock);
        for (int i = 0; i <= newLines; ++i) {
            if (cursor.atBlockEnd()) {
                if (!cursor.movePosition(QTextCursor::NextBlock))
                    break;  // not needed
                continue;
            }
            cursor = backTabCursor(cursor, event->modifiers() & Qt::MetaModifier ? true : false);
            cursor.removeSelectedText();
            if (!cursor.movePosition(QTextCursor::NextBlock))
                break;  // not needed
        }
        cursor.endEditBlock();
        ensureCursorVisible();

        // do nothing else with Shift+Tab
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_Insert) {
        if (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::KeypadModifier) {
            setOverwriteMode(!overwriteMode());
            if (!overwriteMode())
                update();  // otherwise a part of the thick cursor might remain
            event->accept();
            return;
        }
    }
    // due to a Qt bug, ZWNJ may not be inserted with Shift+Space
    else if (event->key() == 0x200c) {
        removeColumnHighlight();
        insertPlainText(QChar(0x200C));
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_Space) {
        if (!colSel_.isEmpty()) {
            prependToColumn(event);
            return;
        }
        if (autoReplace_ && event->modifiers() == Qt::NoModifier) {
            QTextCursor cur = textCursor();
            if (!cur.hasSelection()) {
                const int p = cur.positionInBlock();
                if (p > 1) {
                    bool replaceStr = true;
                    cur.beginEditBlock();
                    if (prog_ == "url" || prog_ == "changelog" || lang_ == "url" ||
                        lang_ == "changelog") {  // not for code
                        cur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 2);
                        const QString selTxt = cur.selectedText();
                        replaceStr = selTxt.endsWith('.');
                        if (!replaceStr) {
                            if (selTxt == "--") {
                                QTextCursor prevCur = cur;
                                prevCur.setPosition(cur.position());
                                prevCur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                                if (prevCur.selectedText() != "-")
                                    cur.insertText("—");
                            }
                            else if (selTxt == "->") {
                                cur.insertText("→");
                            }
                            else if (selTxt == "<-") {
                                cur.insertText("←");
                            }
                            else if (selTxt == ">=") {
                                cur.insertText("≥");
                            }
                            else if (selTxt == "<=") {
                                cur.insertText("≤");
                            }
                        }
                    }
                    if (replaceStr && p > 2) {
                        cur = textCursor();
                        cur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 3);
                        const QString sel3 = cur.selectedText();
                        if (sel3 == "...") {
                            QTextCursor prevCur = cur;
                            prevCur.setPosition(cur.position());
                            if (p > 3) {
                                prevCur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                                if (prevCur.selectedText() != ".")
                                    cur.insertText("…");
                            }
                            else {
                                cur.insertText("…");
                            }
                        }
                    }
                    cur.endEditBlock();
                }
            }
        }
    }
    else if (event->key() == Qt::Key_Home) {
        mousePressed_ = false;                              // remove column highlighting
        if (!(event->modifiers() & Qt::ControlModifier)) {  // override Qt default behavior
            QTextCursor cur = textCursor();
            int p = cur.positionInBlock();
            int indx = 0;
            QRegularExpressionMatch match;
            if (cur.block().text().indexOf(QRegularExpression("^\\s+"), 0, &match) > -1)
                indx = match.capturedLength();
            if (p > 0) {
                p = (p <= indx) ? 0 : indx;
            }
            else {
                p = indx;
            }
            cur.setPosition(p + cur.block().position(),
                            event->modifiers() & Qt::ShiftModifier ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
            setTextCursor(cur);
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_End) {
        mousePressed_ = false;  // remove column highlighting
    }
    else if (!colSel_.isEmpty() && !event->text().isEmpty()) {
        prependToColumn(event);
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

/*************************/
void TextEdit::keyReleaseEvent(QKeyEvent* event) {
    // reset hyperlink cursor on Ctrl release
    if (highlighter_ && event->key() == Qt::Key_Control && viewport()->cursor().shape() != Qt::IBeamCursor)
        viewport()->setCursor(Qt::IBeamCursor);
    QPlainTextEdit::keyReleaseEvent(event);
}

/*************************/
void TextEdit::wheelEvent(QWheelEvent* event) {
    const QPoint anglePoint = event->angleDelta();
    if (event->modifiers() == Qt::ControlModifier) {
        const float delta = anglePoint.y() / 120.f;
        zooming(delta);
        return;
    }

    const bool horizontal = std::abs(anglePoint.x()) > std::abs(anglePoint.y());

    if ((event->modifiers() & Qt::ShiftModifier) &&
        QApplication::wheelScrollLines() > 1) {  // line-by-line scrolling with Shift
        QScrollBar* sbar = nullptr;
        if (horizontal || (event->modifiers() & Qt::AltModifier)) {  // horizontal when Alt is also pressed
            sbar = horizontalScrollBar();
            if (!horizontal && !(sbar && sbar->isVisible()))
                sbar = verticalScrollBar();
        }
        else {
            sbar = verticalScrollBar();
        }
        if (sbar && sbar->isVisible()) {
            const int delta = horizontal ? anglePoint.x() : anglePoint.y();
            if (std::abs(delta) >= QApplication::wheelScrollLines()) {
                QWheelEvent e(event->position(), event->globalPosition(), event->pixelDelta(),
                              QPoint(0, delta / QApplication::wheelScrollLines()), event->buttons(), Qt::NoModifier,
                              event->phase(), false, event->source());
                QCoreApplication::sendEvent(sbar, &e);
            }
            return;
        }
    }

    if ((event->modifiers() & Qt::AltModifier) && !horizontal) {  // horizontal scrolling with Alt
        if (QScrollBar* hbar = horizontalScrollBar(); hbar && hbar->isVisible()) {
            QWheelEvent e(event->position(), event->globalPosition(), event->pixelDelta(), QPoint(0, anglePoint.y()),
                          event->buttons(), Qt::NoModifier, event->phase(), false, event->source());
            QCoreApplication::sendEvent(hbar, &e);
            return;
        }
    }

    // inertial scrolling
    if (inertialScrolling_ && QApplication::wheelScrollLines() > 0 && event->spontaneous() && !horizontal &&
        event->source() == Qt::MouseEventNotSynthesized) {
        if (QScrollBar* vbar = verticalScrollBar(); vbar && vbar->isVisible()) {
            int delta = anglePoint.y();
            // with mouse, set initial speed to 3 lines per wheel turn
            // with more sensitive devices, set it to one line
            if (std::abs(delta) >= 120) {
                if (std::abs(delta * 3) >= QApplication::wheelScrollLines())
                    delta = delta * 3 / QApplication::wheelScrollLines();
            }
            else if (std::abs(delta) >= QApplication::wheelScrollLines()) {
                delta = delta / QApplication::wheelScrollLines();
            }

            if ((delta > 0 && vbar->value() == vbar->minimum()) || (delta < 0 && vbar->value() == vbar->maximum()))
                return;  // scrollbar can't move

            // count wheel events in 500 ms to set frames per second
            static QList<qint64> wheelEvents;
            wheelEvents << QDateTime::currentMSecsSinceEpoch();
            while (wheelEvents.last() - wheelEvents.first() > 500)
                wheelEvents.removeFirst();
            const int steps =
                std::max(kScrollFramesPerSec / static_cast<int>(wheelEvents.size()), 5) * kScrollDurationMs / 1000;

            // accumulate until angle delta reaches an acceptable value
            static int accDelta = 0;
            accDelta += delta;
            if (std::abs(accDelta) < steps)
                return;

            // enqueue inertial scroll data
            scrollData data;
            data.delta = accDelta;
            accDelta = 0;
            data.totalSteps = data.leftSteps = steps;
            queuedScrollSteps_.append(data);

            if (!scrollTimer_) {
                scrollTimer_ = new QTimer();
                scrollTimer_->setTimerType(Qt::PreciseTimer);
                connect(scrollTimer_, &QTimer::timeout, this, &TextEdit::scrollWithInertia);
            }
            if (!scrollTimer_->isActive())
                scrollTimer_->start(1000 / kScrollFramesPerSec);
            return;
        }
    }

    // default behavior
    QAbstractScrollArea::wheelEvent(event);
    updateMicroFocus();
}

/*************************/
void TextEdit::scrollWithInertia() {
    QScrollBar* vbar = verticalScrollBar();
    if (!vbar || !vbar->isVisible()) {
        queuedScrollSteps_.clear();
        scrollTimer_->stop();
        return;
    }

    int totalDelta = 0;
    for (auto it = queuedScrollSteps_.begin(); it != queuedScrollSteps_.end(); ++it) {
        totalDelta += std::round(static_cast<double>(it->delta) / it->totalSteps);
        --it->leftSteps;
    }

    // only remove the first queue to simulate inertia
    while (!queuedScrollSteps_.empty()) {
        const int t = queuedScrollSteps_.begin()->totalSteps;  // 18 for one wheel turn
        const int l = queuedScrollSteps_.begin()->leftSteps;
        if ((t > 10 && l <= 0) || (t > 5 && t <= 10 && l <= -3) || (t <= 5 && l <= -6))
            queuedScrollSteps_.removeFirst();
        else
            break;
    }

    if (totalDelta != 0) {
        QWheelEvent e(QPointF(), QPointF(), QPoint(), QPoint(0, totalDelta), Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(vbar, &e);
    }

    if (queuedScrollSteps_.empty())
        scrollTimer_->stop();
}

}  // namespace Texxy
