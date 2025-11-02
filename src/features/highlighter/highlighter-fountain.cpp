// src/features/highlighter/highlighter-fountain.cpp

#include "highlighter.h"

namespace Texxy {

void Highlighter::fountainFonts(const QString& text) {
    QTextCharFormat boldFormat = neutralFormat;
    boldFormat.setFontWeight(QFont::Bold);

    QTextCharFormat italicFormat = neutralFormat;
    italicFormat.setFontItalic(true);

    QTextCharFormat boldItalicFormat = italicFormat;
    boldItalicFormat.setFontWeight(QFont::Bold);

    QRegularExpressionMatch italicMatch, boldcMatch, expMatch;
    static const QRegularExpression italicExp("(?<!\\\\)\\*([^*]|(?:(?<=\\\\)\\*))+(?<!\\\\|\\s)\\*");
    static const QRegularExpression boldExp("(?<!\\\\)\\*\\*([^*]|(?:(?<=\\\\)\\*))+(?<!\\\\|\\s)\\*\\*");
    static const QRegularExpression boldItalicExp("(?<!\\\\)\\*{3}([^*]|(?:(?<=\\\\)\\*))+(?<!\\\\|\\s)\\*{3}");

    const QRegularExpression exp(boldExp.pattern() + "|" + italicExp.pattern() + "|" + boldItalicExp.pattern());

    int index = 0;
    while ((index = text.indexOf(exp, index, &expMatch)) > -1) {
        if (format(index) == mainFormat && format(index + expMatch.capturedLength() - 1) == mainFormat) {
            if (index == text.indexOf(boldItalicExp, index)) {
                setFormatWithoutOverwrite(index, expMatch.capturedLength(), boldItalicFormat, whiteSpaceFormat);
            }
            else if (index == text.indexOf(boldExp, index, &boldcMatch) &&
                     boldcMatch.capturedLength() == expMatch.capturedLength()) {
                setFormatWithoutOverwrite(index, expMatch.capturedLength(), boldFormat, whiteSpaceFormat);
                /* also format italic bold strings */
                QString str = text.mid(index + 2, expMatch.capturedLength() - 4);
                int indx = 0;
                while ((indx = str.indexOf(italicExp, indx, &italicMatch)) > -1) {
                    setFormatWithoutOverwrite(index + 2 + indx, italicMatch.capturedLength(), boldItalicFormat,
                                              whiteSpaceFormat);
                    indx += italicMatch.capturedLength();
                }
            }
            else {
                setFormatWithoutOverwrite(index, expMatch.capturedLength(), italicFormat, whiteSpaceFormat);
                /* also format bold italic strings */
                QString str = text.mid(index + 1, expMatch.capturedLength() - 2);
                int indx = 0;
                while ((indx = str.indexOf(boldExp, indx, &boldcMatch)) > -1) {
                    setFormatWithoutOverwrite(index + 1 + indx, boldcMatch.capturedLength(), boldItalicFormat,
                                              whiteSpaceFormat);
                    indx += boldcMatch.capturedLength();
                }
            }
            index += expMatch.capturedLength();
        }
        else
            ++index;
    }

    /* format underlines */
    static const QRegularExpression under("(?<!\\\\)_([^_]|(?:(?<=\\\\)_))+(?<!\\\\|\\s)_");
    index = 0;
    while ((index = text.indexOf(under, index, &expMatch)) > -1) {
        QTextCharFormat fi = format(index);
        if (fi == commentFormat || fi == altQuoteFormat)
            ++index;
        else {
            int count = expMatch.capturedLength();
            fi = format(index + count - 1);
            if (fi != commentFormat && fi != altQuoteFormat) {
                int start = index;
                int indx;
                while (start < index + count) {
                    fi = format(start);
                    while (start < index + count &&
                           (fi == whiteSpaceFormat || fi == commentFormat || fi == altQuoteFormat)) {
                        ++start;
                        fi = format(start);
                    }
                    if (start < index + count) {
                        indx = start;
                        fi = format(indx);
                        while (indx < index + count && fi != whiteSpaceFormat && fi != commentFormat &&
                               fi != altQuoteFormat) {
                            fi.setFontUnderline(true);
                            setFormat(indx, 1, fi);
                            ++indx;
                            fi = format(indx);
                        }
                        start = indx;
                    }
                }
            }
            index += count;
        }
    }
}
/*************************/
// Completely commented lines are considered blank.
// It's also supposed that spaces don't affect blankness.
bool Highlighter::isFountainLineBlank(const QTextBlock& block) {
    if (!block.isValid())
        return false;
    QString text = block.text();
    if (block.previous().isValid()) {
        if (block.previous().userState() == markdownBlockQuoteState)
            return false;
        if (block.previous().userState() == commentState) {
            int indx = text.indexOf(commentEndExpression);
            if (indx == -1 || indx == text.length() - 2)
                return true;
            text = text.right(text.length() - indx - 2);
        }
    }
    int index = 0;
    while (index < text.length()) {
        while (index < text.length() && text.at(index).isSpace())
            ++index;
        if (index == text.length())
            break;  // only spaces
        text = text.right(text.length() - index);
        if (!text.startsWith("/*"))
            return false;
        index = 2;

        index = text.indexOf(commentEndExpression, index);
        if (index == -1)
            break;
        index += 2;
    }
    return true;
}
/*************************/
static inline bool isUpperCase(const QString& text) {  // this isn't the same as QSting::isUpper()
    bool res = true;
    for (int i = 0; i < text.length(); ++i) {
        const QChar thisChar = text.at(i);
        if (thisChar.isLetter() && !thisChar.isUpper()) {
            res = false;
            break;
        }
    }
    return res;
}

void Highlighter::highlightFountainBlock(const QString& text) {
    bool rehighlightPrevBlock = false;

    TextBlockData* data = new TextBlockData;
    setCurrentBlockUserData(data);
    setCurrentBlockState(0);

    QTextCharFormat fi;

    static const QRegularExpression leftNoteBracket("\\[\\[");
    static const QRegularExpression rightNoteBracket("\\]\\]");
    static const QRegularExpression heading(
        "^\\s*(INT|EXT|EST|INT./EXT|INT/EXT|I/E|int|ext|est|int./ext|int/ext|i/e|\\.\\w)");
    static const QRegularExpression charRegex("^\\s*@");
    static const QRegularExpression parenRegex("^\\s*\\(.*\\)$");
    static const QRegularExpression lyricRegex("^\\s*~");

    /* notes */
    multiLineComment(text, 0, leftNoteBracket, QRegularExpression("^ ?$|\\]\\]"), markdownBlockQuoteState,
                     altQuoteFormat);
    /* boneyards (like a multi-line comment -- skips altQuoteFormat in notes with commentStartExpression) */
    multiLineComment(text, 0, commentStartExpression, commentEndExpression, commentState, commentFormat);

    QTextBlock prevBlock = currentBlock().previous();
    QTextBlock nxtBlock = currentBlock().next();
    if (isFountainLineBlank(currentBlock())) {
        if (currentBlockState() != commentState)
            setCurrentBlockState(updateState);  // to distinguish it
        if (prevBlock.isValid() && previousBlockState() != commentState &&
            previousBlockState() != markdownBlockQuoteState &&
            (isUpperCase(prevBlock.text()) || prevBlock.text().indexOf(charRegex) == 0 || text.indexOf(heading) == 0)) {
            rehighlightPrevBlock = true;
        }
    }
    else {
        if (prevBlock.isValid() && previousBlockState() != codeBlockState &&
            (isUpperCase(prevBlock.text()) || prevBlock.text().indexOf(charRegex) == 0 || text.indexOf(heading) == 0)) {
            rehighlightPrevBlock = true;
        }

        QTextCharFormat fFormat;
        /* scene headings (between blank lines) */
        if (previousBlockState() == updateState && isFountainLineBlank(nxtBlock) && text.indexOf(heading) == 0) {
            fFormat.setFontWeight(QFont::Bold);
            fFormat.setForeground(Blue);
            setFormatWithoutOverwrite(0, text.length(), fFormat, commentFormat);
        }
        /* characters (following a blank line and not preceding one) */
        else if (previousBlockState() == updateState && nxtBlock.isValid() && !isFountainLineBlank(nxtBlock) &&
                 (text.indexOf(charRegex) == 0 || isUpperCase(text))) {
            fFormat.setFontWeight(QFont::Bold);
            fFormat.setForeground(DarkBlue);
            setFormatWithoutOverwrite(0, text.length(), fFormat, commentFormat);
            if (currentBlockState() != commentState && currentBlockState() != markdownBlockQuoteState)
                setCurrentBlockState(codeBlockState);  // to distinguish it
        }
        /* transitions (between blank lines) */
        else if (previousBlockState() == updateState && isFountainLineBlank(nxtBlock) &&
                 ((text.indexOf(QRegularExpression("^\\s*>")) == 0 &&
                   text.indexOf(QRegularExpression("<$")) == -1)  // not centered
                  || (isUpperCase(text) && text.endsWith("TO:")))) {
            fFormat.setFontWeight(QFont::Bold);
            fFormat.setForeground(DarkMagenta);
            fFormat.setFontItalic(true);
            setFormatWithoutOverwrite(0, text.length(), fFormat, commentFormat);
        }
        else {
            if (previousBlockState() == codeBlockState && currentBlockState() != commentState &&
                currentBlockState() != markdownBlockQuoteState) {
                setCurrentBlockState(codeBlockState);  // to distinguish it for parentheticals
            }
            /* parentheticals (following characters or dialogs) */
            if (text.indexOf(parenRegex) == 0 && previousBlockState() == codeBlockState) {
                fFormat.setFontWeight(QFont::Bold);
                fFormat.setForeground(DarkGreen);
                setFormatWithoutOverwrite(0, text.length(), fFormat, commentFormat);
            }
            /* lyrics */
            else if (text.indexOf(lyricRegex) == 0) {
                fFormat.setFontItalic(true);
                fFormat.setForeground(DarkMagenta);
                setFormatWithoutOverwrite(0, text.length(), fFormat, commentFormat);
            }
        }
    }

    int index;

    /* main formatting */
    int bn = currentBlock().blockNumber();
    if (bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber()) {
        data->setHighlighted();
        QRegularExpressionMatch match;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
        for (const HighlightingRule& rule : std::as_const(highlightingRules))
#else
        for (const HighlightingRule& rule : qAsConst(highlightingRules))
#endif
        {
            if (rule.format == commentFormat)
                continue;
            index = text.indexOf(rule.pattern, 0, &match);
            if (rule.format != whiteSpaceFormat) {
                fi = format(index);
                while (index >= 0 && fi != mainFormat) {
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
                    while (index >= 0 && fi != mainFormat) {
                        index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                        fi = format(index);
                    }
                }
            }
        }
        fountainFonts(text);
    }

    /* left parenthesis */
    index = text.indexOf('(');
    fi = format(index);
    while (index >= 0 && (fi == altQuoteFormat || fi == commentFormat)) {
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
        while (index >= 0 && (fi == altQuoteFormat || fi == commentFormat)) {
            index = text.indexOf('(', index + 1);
            fi = format(index);
        }
    }

    /* right parenthesis */
    index = text.indexOf(')');
    fi = format(index);
    while (index >= 0 && (fi == altQuoteFormat || fi == commentFormat)) {
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
        while (index >= 0 && (fi == altQuoteFormat || fi == commentFormat)) {
            index = text.indexOf(')', index + 1);
            fi = format(index);
        }
    }

    /* left bracket */
    index = text.indexOf(leftNoteBracket);
    fi = format(index);
    while (index >= 0 && fi != altQuoteFormat) {
        index = text.indexOf(leftNoteBracket, index + 2);
        fi = format(index);
    }
    while (index >= 0) {
        BracketInfo* info = new BracketInfo;
        info->character = '[';
        info->position = index;
        data->insertInfo(info);

        info = new BracketInfo;
        info->character = '[';
        info->position = index + 1;
        data->insertInfo(info);

        index = text.indexOf(leftNoteBracket, index + 2);
        fi = format(index);
        while (index >= 0 && fi != altQuoteFormat) {
            index = text.indexOf(leftNoteBracket, index + 2);
            fi = format(index);
        }
    }

    /* right bracket */
    index = text.indexOf(rightNoteBracket);
    fi = format(index);
    while (index >= 0 && fi != altQuoteFormat) {
        index = text.indexOf(rightNoteBracket, index + 2);
        fi = format(index);
    }
    while (index >= 0) {
        BracketInfo* info = new BracketInfo;
        info->character = ']';
        info->position = index;
        data->insertInfo(info);

        info = new BracketInfo;
        info->character = ']';
        info->position = index + 1;
        data->insertInfo(info);

        index = text.indexOf(rightNoteBracket, index + 2);
        fi = format(index);
        while (index >= 0 && fi != altQuoteFormat) {
            index = text.indexOf(rightNoteBracket, index + 2);
            fi = format(index);
        }
    }

    setCurrentBlockUserData(data);

    if (rehighlightPrevBlock) {
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid())
            QMetaObject::invokeMethod(this, "rehighlightBlock", Qt::QueuedConnection, Q_ARG(QTextBlock, prevBlock));
    }
}

}  // namespace Texxy
