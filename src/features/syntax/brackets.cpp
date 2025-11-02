/*
 * texxy/brackets.cpp
 */

#include "texxywindow.h"
#include "ui_texxywindow.h"
#include <algorithm>  // std::min, std::max

namespace Texxy {

// generic forward scan for matching pairs across QTextBlocks
// calls onMatch with absolute doc position of the matching token
template <typename InfoT, typename ListGetter, typename MatchFn>
static inline bool matchForwardGeneric(QTextBlock block,
                                       int startIndex,
                                       int depth,
                                       char openCh,
                                       char closeCh,
                                       const ListGetter& getList,
                                       const MatchFn& onMatch) {
    while (block.isValid()) {
        auto* data = static_cast<TextBlockData*>(block.userData());
        if (!data)
            return false;

        // getList returns by value to avoid dangling references
        const auto infos = getList(data);
        const int docPos = block.position();
        const int n = infos.size();
        int i = startIndex;

        for (; i < n; ++i) {
            const InfoT* info = infos.at(i);
            const char ch = info->character;
            if (ch == openCh) {
                ++depth;
                continue;
            }
            if (ch == closeCh) {
                if (depth == 0) {
                    onMatch(docPos + info->position);
                    return true;
                }
                --depth;
            }
        }

        block = block.next();
        startIndex = 0;
    }
    return false;
}

// generic backward scan for matching pairs across QTextBlocks
template <typename InfoT, typename ListGetter, typename MatchFn>
static inline bool matchBackwardGeneric(QTextBlock block,
                                        int startIndexFromEnd,
                                        int depth,
                                        char openCh,
                                        char closeCh,
                                        const ListGetter& getList,
                                        const MatchFn& onMatch) {
    while (block.isValid()) {
        auto* data = static_cast<TextBlockData*>(block.userData());
        if (!data)
            return false;

        // getList returns by value to avoid dangling references
        const auto infos = getList(data);
        const int docPos = block.position();
        const int n = infos.size();
        int i = startIndexFromEnd;

        for (; i < n; ++i) {
            const InfoT* info = infos.at(n - 1 - i);
            const char ch = info->character;
            if (ch == closeCh) {
                ++depth;
                continue;
            }
            if (ch == openCh) {
                if (depth == 0) {
                    onMatch(docPos + info->position);
                    return true;
                }
                --depth;
            }
        }

        block = block.previous();
        startIndexFromEnd = 0;
    }
    return false;
}

void TexxyWindow::matchBrackets() {
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    QTextCursor cur = textEdit->textCursor();

    auto* data = static_cast<TextBlockData*>(cur.block().userData());
    if (!data)
        return;

    // update TextEdit local highlights first
    textEdit->matchedBrackets();

    // drop any trailing red selections from previous match in O(1) where possible
    QList<QTextEdit::ExtraSelection> es = textEdit->extraSelections();
    const int redCount = textEdit->getRedSel().size();
    if (redCount > 0 && redCount <= es.size())
        es.resize(es.size() - redCount);
    textEdit->setRedSel(QList<QTextEdit::ExtraSelection>());
    textEdit->setExtraSelections(es);

    QTextDocument* doc = textEdit->document();
    const int curPos = cur.position();
    const int blockPos = cur.block().position();  // position of block's first character
    const int curBlockPos = curPos - blockPos;    // position of cursor in block

    const QChar chHere = doc->characterAt(curPos);
    const QChar chPrev = (curPos > 0) ? doc->characterAt(curPos - 1) : QChar(QChar::Null);

    auto onMatch = [this](int pos) { createSelection(pos); };

    // parentheses
    bool isAtLeft = (chHere == QChar('('));
    bool isAtRight = (chPrev == QChar(')'));
    bool findNextBrace = !isAtLeft || !isAtRight;
    if (isAtLeft || isAtRight) {
        // fetch by value to avoid binding a reference to a temporary
        const auto infos = data->parentheses();

        if (isAtLeft) {
            const int n = infos.size();
            for (int i = 0; i < n; ++i) {
                const ParenthesisInfo* info = infos.at(i);
                if (info->position == curBlockPos && info->character == '(') {
                    if (matchLeftParenthesis(cur.block(), i + 1, 0)) {
                        onMatch(blockPos + info->position);
                        if (!isAtRight)
                            break;
                        isAtLeft = false;
                    }
                }
            }
        }
        if (isAtRight) {
            const int n = infos.size();
            for (int i = 0; i < n; ++i) {
                const ParenthesisInfo* info = infos.at(i);
                if (info->position == curBlockPos - 1 && info->character == ')') {
                    if (matchRightParenthesis(cur.block(), n - i, 0)) {
                        onMatch(blockPos + info->position);
                        if (!isAtLeft)
                            break;
                        isAtRight = false;
                    }
                }
            }
        }
    }
    if (!findNextBrace)
        return;

    // braces
    isAtLeft = (chHere == QChar('{'));
    isAtRight = (chPrev == QChar('}'));
    findNextBrace = !isAtLeft || !isAtRight;
    if (isAtLeft || isAtRight) {
        const auto infos = data->braces();

        if (isAtLeft) {
            const int n = infos.size();
            for (int i = 0; i < n; ++i) {
                const BraceInfo* info = infos.at(i);
                if (info->position == curBlockPos && info->character == '{') {
                    if (matchLeftBrace(cur.block(), i + 1, 0)) {
                        onMatch(blockPos + info->position);
                        if (!isAtRight)
                            break;
                        isAtLeft = false;
                    }
                }
            }
        }
        if (isAtRight) {
            const int n = infos.size();
            for (int i = 0; i < n; ++i) {
                const BraceInfo* info = infos.at(i);
                if (info->position == curBlockPos - 1 && info->character == '}') {
                    if (matchRightBrace(cur.block(), n - i, 0)) {
                        onMatch(blockPos + info->position);
                        if (!isAtLeft)
                            break;
                        isAtRight = false;
                    }
                }
            }
        }
    }
    if (!findNextBrace)
        return;

    // brackets
    isAtLeft = (chHere == QChar('['));
    isAtRight = (chPrev == QChar(']'));
    if (isAtLeft || isAtRight) {
        const auto infos = data->brackets();

        if (isAtLeft) {
            const int n = infos.size();
            for (int i = 0; i < n; ++i) {
                const BracketInfo* info = infos.at(i);
                if (info->position == curBlockPos && info->character == '[') {
                    if (matchLeftBracket(cur.block(), i + 1, 0)) {
                        onMatch(blockPos + info->position);
                        if (!isAtRight)
                            break;
                        isAtLeft = false;
                    }
                }
            }
        }
        if (isAtRight) {
            const int n = infos.size();
            for (int i = 0; i < n; ++i) {
                const BracketInfo* info = infos.at(i);
                if (info->position == curBlockPos - 1 && info->character == ']') {
                    if (matchRightBracket(cur.block(), n - i, 0)) {
                        onMatch(blockPos + info->position);
                        if (!isAtLeft)
                            break;
                        isAtRight = false;
                    }
                }
            }
        }
    }
}

/*******************************************************************************
 loops instead of recursion to avoid stack growth on huge files
 the generic helpers above consolidate the core scanning logic
*******************************************************************************/

bool TexxyWindow::matchLeftParenthesis(QTextBlock currentBlock, int i, int numLeftParentheses) {
    return matchForwardGeneric<ParenthesisInfo>(
        currentBlock, i, numLeftParentheses, '(', ')', [](TextBlockData* d) { return d->parentheses(); },
        [this](int pos) { createSelection(pos); });
}

bool TexxyWindow::matchRightParenthesis(QTextBlock currentBlock, int i, int numRightParentheses) {
    return matchBackwardGeneric<ParenthesisInfo>(
        currentBlock, i, numRightParentheses, '(', ')', [](TextBlockData* d) { return d->parentheses(); },
        [this](int pos) { createSelection(pos); });
}

bool TexxyWindow::matchLeftBrace(QTextBlock currentBlock, int i, int numRightBraces) {
    return matchForwardGeneric<BraceInfo>(
        currentBlock, i, numRightBraces, '{', '}', [](TextBlockData* d) { return d->braces(); },
        [this](int pos) { createSelection(pos); });
}

bool TexxyWindow::matchRightBrace(QTextBlock currentBlock, int i, int numLeftBraces) {
    return matchBackwardGeneric<BraceInfo>(
        currentBlock, i, numLeftBraces, '{', '}', [](TextBlockData* d) { return d->braces(); },
        [this](int pos) { createSelection(pos); });
}

bool TexxyWindow::matchLeftBracket(QTextBlock currentBlock, int i, int numRightBrackets) {
    return matchForwardGeneric<BracketInfo>(
        currentBlock, i, numRightBrackets, '[', ']', [](TextBlockData* d) { return d->brackets(); },
        [this](int pos) { createSelection(pos); });
}

bool TexxyWindow::matchRightBracket(QTextBlock currentBlock, int i, int numLeftBrackets) {
    return matchBackwardGeneric<BracketInfo>(
        currentBlock, i, numLeftBrackets, '[', ']', [](TextBlockData* d) { return d->brackets(); },
        [this](int pos) { createSelection(pos); });
}

void TexxyWindow::createSelection(int pos) {
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();

    QList<QTextEdit::ExtraSelection> es = textEdit->extraSelections();

    QTextCursor cursor = textEdit->textCursor();
    cursor.setPosition(pos);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);

    QTextEdit::ExtraSelection extra;
    extra.format.setBackground(textEdit->hasDarkScheme() ? QColor(190, 0, 3) : QColor(255, 150, 150));
    extra.cursor = cursor;

    QList<QTextEdit::ExtraSelection> rsel = textEdit->getRedSel();
    rsel.append(extra);
    textEdit->setRedSel(rsel);
    es.append(extra);

    textEdit->setExtraSelections(es);
}

}  // namespace Texxy
