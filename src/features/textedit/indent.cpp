#include "textedit/textedit_prelude.h"

#include <algorithm>

namespace Texxy {

// Compute leading indentation (spaces or tabs) up to the caret or selection start.
QString TextEdit::computeIndentation(const QTextCursor& cur) const {
    QTextCursor c = cur;
    if (c.hasSelection()) {
        c.setPosition(std::min(c.anchor(), c.position()));
    }

    const QString blockText = c.block().text();
    if (blockText.isEmpty()) {
        return QString{};
    }

    const int posInBlock = c.positionInBlock();
    const int n = blockText.size();

    // Count leading whitespace characters
    int lead = 0;
    while (lead < n) {
        QChar ch = blockText.at(lead);
        if (ch != QChar::Space && ch != QChar::Tabulation) {
            break;
        }
        ++lead;
    }

    // Limit to where the caret is, if inside indent
    const int upto = std::min(lead, posInBlock);
    return blockText.left(upto);
}

// Compute the number of spaces needed to reach the next "soft tab" stop.
QString TextEdit::remainingSpaces(const QString& spaceTab, const QTextCursor& cursor) const {
    const int tabw = std::max<int>(1, static_cast<int>(spaceTab.size()));

    const QString left = cursor.block().text().left(cursor.positionInBlock());
    int col = 0;
    for (QChar ch : left) {
        if (ch == QChar::Tabulation) {
            int add = tabw - (col % tabw);
            col += add;
        }
        else {
            ++col;
        }
    }

    int spaces = tabw - (col % tabw);
    if (spaces == 0) {
        spaces = tabw;
    }

    return QString(spaces, QChar::Space);
}

// Return a QTextCursor selecting the indent span to remove for a back-tab operation.
QTextCursor TextEdit::backTabCursor(const QTextCursor& cursor, bool twoSpace) const {
    QTextCursor tmp = cursor;

    const QString blockText = cursor.block().text();
    int indentLen = 0;
    const int btSize = blockText.size();
    while (indentLen < btSize) {
        QChar ch = blockText.at(indentLen);
        if (ch != QChar::Space && ch != QChar::Tabulation) {
            break;
        }
        ++indentLen;
    }
    if (indentLen == 0) {
        return tmp;
    }

    const int textStart = cursor.block().position() + indentLen;
    const QString indent = blockText.left(indentLen);

    const int tabw = std::max<int>(1, static_cast<int>(textTab_.size()));
    int col = 0;
    for (QChar ch : indent) {
        if (ch == QChar::Tabulation) {
            int add = tabw - (col % tabw);
            col += add;
        }
        else {
            ++col;
        }
    }

    int dropCols;
    if (twoSpace) {
        const int rem = (col % tabw == 0) ? tabw : (col % tabw);
        dropCols = std::min(2, rem);
    }
    else {
        dropCols = (col % tabw == 0) ? tabw : (col % tabw);
    }

    int charsToRemove = 0;
    int leftCols = dropCols;
    for (int i = indent.size() - 1; i >= 0 && leftCols > 0; --i) {
        QChar ch = indent.at(i);
        if (ch == QChar::Tabulation) {
            const int rem = (col % tabw == 0) ? tabw : (col % tabw);
            const int take = std::min(rem, leftCols);
            leftCols -= take;
            col -= rem;
        }
        else {
            --leftCols;
            --col;
        }
        ++charsToRemove;
    }

    tmp.setPosition(textStart);
    tmp.setPosition(textStart - charsToRemove, QTextCursor::KeepAnchor);
    return tmp;
}

// Insert typed text at the start of each selected column-block line.
void TextEdit::prependToColumn(QKeyEvent* event) {
    if (colSel_.count() > 1000) {
        QTimer::singleShot(0, this, [this]() { emit hugeColumn(); });
        event->accept();
        return;
    }

    const bool origMousePressed = mousePressed_;
    mousePressed_ = true;

    QTextCursor cur = textCursor();
    cur.beginEditBlock();
    for (const auto& extra : std::as_const(colSel_)) {
        if (!extra.cursor.hasSelection())
            continue;
        QTextCursor c = extra.cursor;
        c.setPosition(std::min(c.anchor(), c.position()));
        c.insertText(event->text());
    }
    cur.endEditBlock();

    mousePressed_ = origMousePressed;
    event->accept();
}

// Return true if the given string consists only of spaces or tabs.
bool TextEdit::isOnlySpaces(const QString& str) {
    return std::all_of(str.begin(), str.end(),
                       [](QChar ch) { return (ch == QChar::Space || ch == QChar::Tabulation); });
}

}  // namespace Texxy
