

#include "highlighter.h"

namespace Texxy {

static const QRegularExpression luaSLCommentExp("--(?!\\[\\=*\\[)");
static const QRegularExpression luaQuoteExp(R"((["'])((\\{2})*|(.*?[^\\](\\{2})*))(\1|$))");

bool Highlighter::isLuaQuote(const QString& text, const int index) const {
    if (index < 0)
        return false;
    QRegularExpressionMatch match;
    int i = text.lastIndexOf(luaQuoteExp, index, &match);
    QTextCharFormat fi = format(i);
    return (i > -1 && i <= index && i + match.capturedLength() > index && fi != commentFormat && fi != urlFormat &&
            fi != regexFormat);
}
/*************************/
bool Highlighter::isSingleLineLuaComment(const QString& text, const int index, const int start) const {
    if (start < 0 || index < start)
        return false;
    if (currentBlock().blockNumber() == 0 && text.startsWith("#!"))
        return true;  // the interpreter ignores the first line if it starts with "#"
    int i = start;
    QTextCharFormat fi;
    while ((i = text.indexOf(luaSLCommentExp, i)) > -1) {
        fi = format(i);
        if (fi == commentFormat || fi == urlFormat || fi == regexFormat || isLuaQuote(text, i)) {
            ++i;
        }
        else
            break;
    }
    return (i > -1 && i <= index);
}
/*************************/
// String blocks are also handled here but formatted by "regexFormat".
void Highlighter::multiLineLuaComment(const QString& text) {
    /* Lua comment signs can have equal signs between brackets */
    static const QRegularExpression luaCommentStartExp("\\[\\=*\\[|--\\[\\=*\\[");

    int prevState = previousBlockState();
    int startIndex = 0;

    QString delimStr;
    int openStringBlocks = 0;
    QRegularExpression commentEndExp;
    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;

    if (prevState >= -1 && prevState <= endState) {
        startIndex = text.indexOf(luaCommentStartExp, 0, &startMatch);
        if (isSingleLineLuaComment(text, startIndex, 0))
            startIndex = -1;
        else {
            while (isLuaQuote(text, startIndex))
                startIndex = text.indexOf(luaCommentStartExp, startIndex + 1, &startMatch);
        }
    }
    else {
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid()) {
            if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData())) {
                delimStr = prevData->labelInfo();
                openStringBlocks = prevData->openNests();
            }
        }
    }

    while (startIndex >= 0) {
        if (delimStr.isEmpty()) {
            delimStr = startMatch.captured();
            if (!delimStr.isEmpty()) {
                if (delimStr.at(0) == '-')
                    delimStr.remove('-');
                else
                    ++openStringBlocks;
                delimStr.remove('[');
                delimStr.replace("=", "\\=");
            }
        }
        bool isStringBlock(openStringBlocks > 0);
        commentEndExp.setPattern("\\]" + delimStr + "\\]");

        int index, endIndex;
        if (startIndex == 0 && (prevState < -1 || prevState > endState))
            index = 0;
        else
            index = startIndex + startMatch.capturedLength();
        endIndex = text.indexOf(commentEndExp, index, &endMatch);

        if (isStringBlock) {
            QRegularExpression stringBlockStart;
            QRegularExpressionMatch match;
            stringBlockStart.setPattern("(?<!--)\\[" + delimStr + "\\[");
            while (endIndex >= 0) {
                int i;
                while ((i = text.indexOf(stringBlockStart, index, &match)) > -1 &&
                       i < endIndex) {  // another similar string block is started before endIndex
                    ++openStringBlocks;
                    index = i + match.capturedLength();
                }
                --openStringBlocks;
                if (openStringBlocks > 0)
                    endIndex = text.indexOf(commentEndExp, endIndex + endMatch.capturedLength(), &endMatch);
                else
                    break;
            }
            if (openStringBlocks > 0 && endIndex == -1) {
                int i;
                while ((i = text.indexOf(stringBlockStart, index, &match)) >
                       -1) {  // another similar string block is started
                    ++openStringBlocks;
                    index = i + match.capturedLength();
                }
            }
        }

        int commentLength;
        if (endIndex == -1) {
            int n = static_cast<int>(qHash(delimStr + QString::number(openStringBlocks)));
            int state = n + (n >= 0 ? endState + 1 : -1);
            setCurrentBlockState(state);
            if (openStringBlocks > 0 || !delimStr.isEmpty()) {
                TextBlockData* curData = static_cast<TextBlockData*>(currentBlock().userData());
                if (curData) {
                    curData->insertInfo(delimStr);
                    curData->insertNestInfo(openStringBlocks);
                    setCurrentBlockUserData(curData);
                }
            }
            commentLength = text.length() - startIndex;
        }
        else
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        setFormat(startIndex, commentLength, isStringBlock ? regexFormat : commentFormat);

        QString str = text.sliced(startIndex, commentLength);
        int pIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((pIndex = str.indexOf(urlPattern, pIndex, &urlMatch)) > -1) {
            setFormat(pIndex + startIndex, urlMatch.capturedLength(), urlFormat);
            pIndex += urlMatch.capturedLength();
        }
        pIndex = 0;
        while ((pIndex = str.indexOf(notePattern, pIndex, &urlMatch)) > -1) {
            if (format(pIndex + startIndex) != urlFormat)
                setFormat(pIndex + startIndex, urlMatch.capturedLength(), noteFormat);
            pIndex += urlMatch.capturedLength();
        }

        delimStr.clear();
        startIndex = text.indexOf(luaCommentStartExp, startIndex + commentLength, &startMatch);
        if (isSingleLineLuaComment(text, startIndex, endIndex + 1))
            startIndex = -1;
        else {
            while (isLuaQuote(text, startIndex))
                startIndex = text.indexOf(luaCommentStartExp, startIndex + 1, &startMatch);
        }
    }
}
/*************************/
void Highlighter::highlightLuaBlock(const QString& text) {
    TextBlockData* data = new TextBlockData;
    setCurrentBlockUserData(data);
    setCurrentBlockState(0);

    int index;
    QTextCharFormat fi;

    multiLineLuaComment(text);

    int bn = currentBlock().blockNumber();
    if (bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber()) {
        data->setHighlighted();  // completely highlighted
        QRegularExpressionMatch match;

        /************************
         * Single-line comments *
         ************************/
        index = 0;
        if (bn == 0 && text.startsWith("#!"))
            setFormat(0, text.length(), commentFormat);
        else {
            while ((index = text.indexOf(luaSLCommentExp, index)) > -1) {
                /* skip quotes, multi-line comments and string blocks */
                fi = format(index);
                if (fi == commentFormat || fi == urlFormat || fi == regexFormat || isLuaQuote(text, index)) {
                    ++index;
                }
                else
                    break;
            }
            if (index >= 0) {
                int l = text.length() - index;
                setFormat(index, l, commentFormat);
                /* format urls and email addresses inside the comment */
                QString str = text.sliced(index, l);
                int pIndex = 0;
                while ((pIndex = str.indexOf(urlPattern, pIndex, &match)) > -1) {
                    setFormat(pIndex + index, match.capturedLength(), urlFormat);
                    pIndex += match.capturedLength();
                }
                /* format note patterns too */
                pIndex = 0;
                while ((pIndex = str.indexOf(notePattern, pIndex, &match)) > -1) {
                    if (format(pIndex + index) != urlFormat)
                        setFormat(pIndex + index, match.capturedLength(), noteFormat);
                    pIndex += match.capturedLength();
                }
            }
        }

        /*****************************************
         * Quotes (which are always single-line) *
         *****************************************/
        index = 0;
        while ((index = text.indexOf(luaQuoteExp, index, &match)) >= 0) {
            /* skip all comments and also string blocks */
            fi = format(index);
            if (fi == commentFormat || fi == urlFormat || fi == regexFormat) {
                ++index;
                continue;
            }
            QChar c = text.at(index + match.capturedLength() - 1);
            if (match.capturedLength() == 1 ||
                (c != '\"' && c != '\'')) {  // the quotation isn't closed; show it as an error
                setFormat(index, match.capturedLength(), errorFormat);
                break;
            }
            setFormat(index, match.capturedLength(), text.at(index) == '\"' ? quoteFormat : altQuoteFormat);

            /* format urls and email addresses inside the quotation */
            QString str = text.sliced(index, match.capturedLength());
            int urlIndex = 0;
            QRegularExpressionMatch urlMatch;
            while ((urlIndex = str.indexOf(urlPattern, urlIndex, &urlMatch)) > -1) {
                setFormat(urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
                urlIndex += urlMatch.capturedLength();
            }

            index += match.capturedLength();
        }

        /*****************
         * Other formats *
         *****************/
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
        for (const HighlightingRule& rule : std::as_const(highlightingRules))
#else
        for (const HighlightingRule& rule : qAsConst(highlightingRules))
#endif
        {
            index = text.indexOf(rule.pattern, 0, &match);
            /* skip all quotes and comments */
            if (rule.format != whiteSpaceFormat) {
                fi = format(index);
                while (index >= 0 &&
                       (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat ||
                        fi == commentFormat || fi == urlFormat || fi == regexFormat || fi == errorFormat)) {
                    index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                    fi = format(index);
                }
            }

            while (index >= 0) {
                int length = match.capturedLength();
                setFormat(index, length, rule.format);
                index = text.indexOf(rule.pattern, index + length, &match);

                if (rule.format != whiteSpaceFormat) {
                    fi = format(index);
                    while (index >= 0 &&
                           (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat ||
                            fi == commentFormat || fi == urlFormat || fi == regexFormat || fi == errorFormat)) {
                        index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                        fi = format(index);
                    }
                }
            }
        }
    }

    /*********************************************
     * Parentheses, braces and brackets matching *
     *********************************************/

    /* left parenthesis */
    index = text.indexOf('(');
    fi = format(index);
    while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                          isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
        index = text.indexOf('(', index + 1);
        fi = format(index);
    }
    while (index >= 0) {
        ParenthesisInfo* info = new ParenthesisInfo;
        info->character = '(';
        info->position = index;
        data->insertInfo(info);

        index = text.indexOf('(', index + 1);
        fi = format(index);
        while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                              isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
            index = text.indexOf('(', index + 1);
            fi = format(index);
        }
    }

    /* right parenthesis */
    index = text.indexOf(')');
    fi = format(index);
    while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                          isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
        index = text.indexOf(')', index + 1);
        fi = format(index);
    }
    while (index >= 0) {
        ParenthesisInfo* info = new ParenthesisInfo;
        info->character = ')';
        info->position = index;
        data->insertInfo(info);

        index = text.indexOf(')', index + 1);
        fi = format(index);
        while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                              isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
            index = text.indexOf(')', index + 1);
            fi = format(index);
        }
    }

    /* left brace */
    index = text.indexOf('{');
    fi = format(index);
    while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                          isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
        index = text.indexOf('{', index + 1);
        fi = format(index);
    }
    while (index >= 0) {
        BraceInfo* info = new BraceInfo;
        info->character = '{';
        info->position = index;
        data->insertInfo(info);

        index = text.indexOf('{', index + 1);
        fi = format(index);
        while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                              isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
            index = text.indexOf('{', index + 1);
            fi = format(index);
        }
    }

    /* right brace */
    index = text.indexOf('}');
    fi = format(index);
    while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                          isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
        index = text.indexOf('}', index + 1);
        fi = format(index);
    }
    while (index >= 0) {
        BraceInfo* info = new BraceInfo;
        info->character = '}';
        info->position = index;
        data->insertInfo(info);

        index = text.indexOf('}', index + 1);
        fi = format(index);
        while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                              isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
            index = text.indexOf('}', index + 1);
            fi = format(index);
        }
    }

    /* left bracket */
    index = text.indexOf('[');
    fi = format(index);
    while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                          isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
        index = text.indexOf('[', index + 1);
        fi = format(index);
    }
    while (index >= 0) {
        BracketInfo* info = new BracketInfo;
        info->character = '[';
        info->position = index;
        data->insertInfo(info);

        index = text.indexOf('[', index + 1);
        fi = format(index);
        while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                              isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
            index = text.indexOf('[', index + 1);
            fi = format(index);
        }
    }

    /* right bracket */
    index = text.indexOf(']');
    fi = format(index);
    while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                          isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
        index = text.indexOf(']', index + 1);
        fi = format(index);
    }
    while (index >= 0) {
        BracketInfo* info = new BracketInfo;
        info->character = ']';
        info->position = index;
        data->insertInfo(info);

        index = text.indexOf(']', index + 1);
        fi = format(index);
        while (index >= 0 && (fi == commentFormat || fi == urlFormat || fi == regexFormat ||
                              isSingleLineLuaComment(text, index, 0) || isLuaQuote(text, index))) {
            index = text.indexOf(']', index + 1);
            fi = format(index);
        }
    }

    setCurrentBlockUserData(data);
}

}  // namespace Texxy
