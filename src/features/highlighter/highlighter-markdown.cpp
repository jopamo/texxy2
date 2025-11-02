

#include "highlighter.h"

namespace Texxy {

static const QRegularExpression listRegex("((\\*\\s+){1,}|(\\+\\s+){1,}|(\\-\\s+){1,}|\\d+\\.\\s+|\\d+\\)\\s+)");
static const QRegularExpression nonSpace("\\S");

void Highlighter::markdownSingleLineCode(const QString& text) {
    /* "(?:(?!\1).)+" means "contaning anything other than \1" */
    static const QRegularExpression markdowBackQuote(R"((?<!\\)(?:\\{2})*\K(`+)(?!`)(?:(?!\1).)+(\1)(?!`))");

    QRegularExpressionMatch match;
    int index = text.indexOf(markdowBackQuote, 0, &match);
    while (isMLCommented(text, index, commentState))
        index = text.indexOf(markdowBackQuote, index + match.capturedLength(1), &match);
    while (index >= 0) {
        setFormat(index, match.capturedLength(), codeBlockFormat);
        int end = index + match.capturedLength();
        index = text.indexOf(markdowBackQuote, end, &match);
        while (isMLCommented(text, index, commentState, end))
            index = text.indexOf(markdowBackQuote, index + match.capturedLength(1), &match);
    }
}
/*************************/
bool Highlighter::isIndentedCodeBlock(const QString& text, int& index, QRegularExpressionMatch& match) const {
    static const QRegularExpression codeRegex("^(\\s+).*");

    int prevState = previousBlockState();
    QTextBlock prevBlock = currentBlock().previous();
    bool prevProperty = false;
    QString prevLabel;
    if (prevBlock.isValid()) {
        if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData())) {
            prevProperty = prevData->getProperty();
            prevLabel = prevData->labelInfo();
        }
    }
    int extraBlockIndentation = 0;
    if (!prevLabel.isEmpty() && prevState != codeBlockState)  // the label info is about indentation
    {
        extraBlockIndentation = prevLabel.length();
    }

    index = text.indexOf(codeRegex, 0, &match);
    QTextCharFormat fi = format(index);
    if (index >= 0 && fi != blockQuoteFormat && fi != codeBlockFormat && fi != commentFormat && fi != urlFormat) {
        /* when this is about a code block with indentation
           but the current line can't be a code block */
        bool isCodeBlock(prevState != markdownBlockQuoteState && prevState != codeBlockState);
        if (isCodeBlock) {
            if (match.capturedLength(1) <= extraBlockIndentation)
                isCodeBlock = false;
            else if (match.capturedLength(1) < extraBlockIndentation + 4) {  // a tab can be used instead of four spaces
                if (!(match.captured(1).mid(extraBlockIndentation)).contains(QChar(QChar::Tabulation)))
                    isCodeBlock = false;
            }
            if (isCodeBlock) {
                const QString prevTxt = prevBlock.text();
                if (prevTxt.indexOf(nonSpace) > -1) {
                    /*QRegularExpressionMatch matchPrev;
                    int indx = prevTxt.indexOf (codeRegex, 0, &matchPrev);
                    if (indx < 0)
                        isCodeBlock = false;
                    else
                    {
                        if (matchPrev.capturedLength (1) <= extraBlockIndentation)
                            isCodeBlock = false;
                        else if (matchPrev.capturedLength (1) < extraBlockIndentation + 4)
                        {
                            if (!(matchPrev.captured (1).mid (extraBlockIndentation)).contains (QChar
                    (QChar::Tabulation))) isCodeBlock = false;
                        }
                    }*/
                    if (/*isCodeBlock && */ !prevProperty)
                        isCodeBlock = false;
                }
            }
        }
        return isCodeBlock;
    }
    return false;
}
/*************************/
void Highlighter::markdownComment(const QString& text) {
    int prevState = previousBlockState();
    int startIndex = 0;

    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;

    if (prevState != commentState) {
        startIndex = text.indexOf(commentStartExpression, startIndex, &startMatch);
        while (format(startIndex) == codeBlockFormat)
            startIndex = text.indexOf(commentStartExpression, startIndex + 1, &startMatch);
        if (startIndex > 0) {
            if (text.indexOf(QRegularExpression("^#+\\s+.*"), 0) == 0)
                return;  // no comment start sign inside headings
            QRegularExpressionMatch match;
            int indx;
            if (isIndentedCodeBlock(text, indx, match))
                return;  // no comment start sign inside indented code blocks
            /* no comment start sign inside footnotes, images or links */
            static const QRegularExpression mExp(
                "\\[\\^[^\\]]+\\]"
                "|"
                "\\!\\[[^\\]\\^]*\\]\\s*"
                "(\\(\\s*[^\\)\\(\\s]+(\\s+\\\".*\\\")*\\s*\\)|\\s*\\[[^\\]]*\\])"
                "|"
                "\\[[^\\]\\^]*\\]\\s*\\[[^\\]\\s]*\\]"
                "|"
                "\\[[^\\]\\^]*\\]\\s*\\(\\s*[^\\)\\(\\s]+(\\s+\\\".*\\\")*\\s*\\)"
                "|"
                "\\[[^\\]\\^]*\\]:\\s+\\s*[^\\)\\(\\s]+(\\s+\\\".*\\\")*");
            indx = text.indexOf(mExp, 0, &match);
            while (indx >= 0 && indx < startIndex) {
                int mEnd = indx + match.capturedLength();
                if (startIndex < mEnd) {
                    startIndex = text.indexOf(commentStartExpression, mEnd, &startMatch);
                    if (startIndex == -1)
                        return;
                }
                indx = text.indexOf(mExp, mEnd, &match);
            }
        }
    }

    while (startIndex >= 0) {
        int endIndex;
        if (startIndex == 0 && prevState == commentState)
            endIndex = text.indexOf(commentEndExpression, 0, &endMatch);
        else
            endIndex = text.indexOf(commentEndExpression, startIndex + startMatch.capturedLength(), &endMatch);

        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(commentState);
            commentLength = text.length() - startIndex;
        }
        else
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        setFormat(startIndex, commentLength, commentFormat);

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

        startIndex = text.indexOf(commentStartExpression, startIndex + commentLength, &startMatch);

        while (format(startIndex) == codeBlockFormat)
            startIndex = text.indexOf(commentStartExpression, startIndex + 1, &startMatch);
    }
}
/*************************/
// This function formats Markdown's block quotes and code blocks.
// The start expressions always includes the line start.
// The value of "indentation" should be positive only with block quotes.
bool Highlighter::markdownMultiLine(const QString& text,
                                    const QString& oldStartPattern,
                                    const int indentation,
                                    const int state,
                                    const QTextCharFormat& txtFormat) {
    static const QRegularExpression blockQuoteStartExp("^\\s*(?=>)");
    static const QRegularExpression blockQuoteEndExp("^\\s*$");
    static const QRegularExpression codeblockStartExp("^ {0,3}\\K(`{3,}(?=[^`]*$)|~{3,}(?=[^~]*$))");

    QRegularExpression endRegex;
    if (indentation > 0)  // used only with block quotes
        endRegex = blockQuoteEndExp;

    int prevState = previousBlockState();

    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;

    if (prevState != state) {
        int startIndex = text.indexOf(indentation > 0 ? blockQuoteStartExp : codeblockStartExp, 0, &startMatch);
        if (startIndex == -1)
            return false;  // nothing to format
        if (format(startIndex) == commentFormat || format(startIndex) == urlFormat ||
            format(startIndex) == codeBlockFormat) {
            return false;  // this is a comment or quote
        }
        if (indentation > 0 && startMatch.capturedLength() > indentation)
            return false;
    }

    bool res = false;

    if (indentation > 0) {
        /* don't continue the previous block quote if this line is a list */
        if (prevState == state) {
            int spaces = 0;
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid()) {  // the label info is about indentation in this case
                if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData()))
                    spaces = prevData->labelInfo().length();
            }
            spaces += 3;
            int i = 0;
            for (i = 0; i < spaces; ++i) {
                if (i == text.length() || text.at(i) != ' ')
                    break;
            }
            if (text.indexOf(listRegex, i) == i)
                return false;
        }
    }
    else {
        if (prevState == state) {
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid()) {  // the label info is about end regex in this case
                if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData()))
                    endRegex.setPattern(prevData->labelInfo());
            }
        }
        else {                                    // get the end regex from the start regex
            QString str = startMatch.captured();  // is never empty
            str += QString(str.at(0));
            endRegex.setPattern(QStringLiteral("^\\s*\\K") + str + QStringLiteral("*(?!\\s*\\S)"));
        }
    }

    int endIndex = indentation <= 0 && prevState != state ?  // the start of a code block can be ```
                       -1
                                                          : text.indexOf(endRegex, 0, &endMatch);
    int L;
    if (endIndex == -1) {
        L = text.length();
        setCurrentBlockState(state);
        if (indentation <= 0) {
            if (TextBlockData* data = static_cast<TextBlockData*>(currentBlock().userData())) {
                data->insertInfo(endRegex.pattern());
                if (data->lastState() == state && oldStartPattern != endRegex.pattern())
                    res = true;
            }
        }
    }
    else
        L = endIndex + endMatch.capturedLength();
    setFormat(0, L, txtFormat);

    /* format urls and email addresses inside block quotes and code blocks */
    QString str = text.mid(0, L);
    int pIndex = 0;
    QRegularExpressionMatch urlMatch;
    while ((pIndex = str.indexOf(urlPattern, pIndex, &urlMatch)) > -1) {
        setFormat(pIndex, urlMatch.capturedLength(), urlFormat);
        pIndex += urlMatch.capturedLength();
    }
    /* format note patterns too */
    pIndex = 0;
    while ((pIndex = str.indexOf(notePattern, pIndex, &urlMatch)) > -1) {
        if (format(pIndex) != urlFormat)
            setFormat(pIndex, urlMatch.capturedLength(), noteFormat);
        pIndex += urlMatch.capturedLength();
    }

    return res;
}
/*************************/
void Highlighter::markdownFonts(const QString& text) {
    QTextCharFormat boldFormat = neutralFormat;
    boldFormat.setFontWeight(QFont::Bold);

    QTextCharFormat italicFormat = neutralFormat;
    italicFormat.setFontItalic(true);

    QTextCharFormat boldItalicFormat = italicFormat;
    boldItalicFormat.setFontWeight(QFont::Bold);

    /* NOTE: Apparently, all browsers use expressions similar to the following ones.
             However, these patterns aren't logical. For example, escaping an asterisk
             isn't always equivalent to its removal with regard to boldness/italicity.
             It also seems that five successive asterisks are ignored at start. */

    QRegularExpressionMatch italicMatch;
    static const QRegularExpression italicExp(
        "(?<!\\\\|\\*{4})\\*([^*]|(?:(?<!\\*)\\*\\*))+\\*|(?<!\\\\|_{4})_([^_]|(?:(?<!_)__))+_");  // allow double
                                                                                                   // asterisks inside

    QRegularExpressionMatch boldcMatch;
    // const QRegularExpression boldExp ("\\*\\*(?!\\*)(?:(?!\\*\\*).)+\\*\\*|__(?:(?!__).)+__}");
    static const QRegularExpression boldExp(
        "(?<!\\\\|\\*{3})\\*\\*([^*]|(?:(?<!\\*)\\*))+\\*\\*|(?<!\\\\|_{3})__([^_]|(?:(?<!_)_))+__");  // allow single
                                                                                                       // asterisks
                                                                                                       // inside

    static const QRegularExpression boldItalicExp(
        "(?<!\\\\|\\*{2})\\*{3}([^*]|(?:(?<!\\*)\\*))+\\*{3}|(?<!\\\\|_{2})_{3}([^_]|(?:(?<!_)_))+_{3}");

    QRegularExpressionMatch expMatch;
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
}
/*************************/
void Highlighter::highlightMarkdownBlock(const QString& text) {
    bool rehighlightNextBlock = false;
    bool oldProperty = false;
    QString oldLabel;
    if (TextBlockData* oldData = static_cast<TextBlockData*>(currentBlockUserData())) {
        oldProperty = oldData->getProperty();
        oldLabel = oldData->labelInfo();
    }

    int index;
    TextBlockData* data = new TextBlockData;
    data->setLastState(currentBlockState());
    setCurrentBlockUserData(data);
    setCurrentBlockState(0);
    QTextCharFormat fi;

    markdownSingleLineCode(text);
    markdownComment(text);

    int prevState = previousBlockState();
    QTextBlock prevBlock = currentBlock().previous();
    QString prevLabel;
    if (prevBlock.isValid()) {
        if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData()))
            prevLabel = prevData->labelInfo();
    }
    int extraBlockIndentation = 0;
    static const QRegularExpression startSpace("^\\s+");

    /* determine whether this line is inside a list */
    if (!prevLabel.isEmpty() && prevState != codeBlockState)  // the label info is about indentation
    {
        extraBlockIndentation = prevLabel.length();
        if (prevBlock.text().indexOf(nonSpace) > -1)
            data->insertInfo(prevLabel);
        else {
            QRegularExpressionMatch spacesMatch;
            (void)text.indexOf(startSpace, 0, &spacesMatch);
            if (spacesMatch.capturedLength() == text.length() ||
                spacesMatch.capturedLength() >= extraBlockIndentation) {
                data->insertInfo(prevLabel);
            }
        }
    }

    /* a block quote shouldn't be formatted inside a comment or fenced code block */
    if (prevState != commentState && prevState != codeBlockState) {
        markdownMultiLine(text,
                          QString(),  // not used
                          3 + extraBlockIndentation, markdownBlockQuoteState, blockQuoteFormat);
    }
    /* the fenced code block shouldn't be formatted inside a comment or block quote */
    if (prevState != commentState && prevState != markdownBlockQuoteState) {
        rehighlightNextBlock |= markdownMultiLine(text, oldLabel,
                                                  0,  // not used
                                                  codeBlockState, codeBlockFormat);
    }

    if (currentBlockState() != markdownBlockQuoteState && currentBlockState() != codeBlockState) {
        QRegularExpressionMatch match;

        /* list start */
        QTextCharFormat markdownFormat;
        markdownFormat.setFontWeight(QFont::Bold);
        markdownFormat.setForeground(DarkBlue);
        int i = 0;
        for (i = 0; i < 3 + extraBlockIndentation; ++i) {
            if (i == text.length() || text.at(i) != ' ')
                break;
        }
        index = text.indexOf(listRegex, i, &match);
        fi = format(index);
        if (index == i && fi != blockQuoteFormat && fi != codeBlockFormat && fi != commentFormat && fi != urlFormat) {
            QString spaces;
            for (int j = 0; j < index + match.capturedLength(); ++j)
                spaces += " ";
            data->insertInfo(spaces);
            setFormat(index, match.capturedLength(), markdownFormat);
        }

        /* code block with indentation */
        if (isIndentedCodeBlock(text, index, match)) {
            setFormat(index, match.capturedLength(), codeBlockFormat);
            data->setProperty(true);
            if (!oldProperty)
                rehighlightNextBlock = true;
        }
        else if (oldProperty)
            rehighlightNextBlock = true;
    }

    int bn = currentBlock().blockNumber();
    if (bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber()) {
        data->setHighlighted();  // completely highlighted
        QRegularExpressionMatch match;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
        for (const HighlightingRule& rule : std::as_const(highlightingRules))
#else
        for (const HighlightingRule& rule : qAsConst(highlightingRules))
#endif
        {
            index = text.indexOf(rule.pattern, 0, &match);
            if (rule.format != whiteSpaceFormat) {
                if (currentBlockState() == markdownBlockQuoteState || currentBlockState() == codeBlockState)
                    continue;
                fi = format(index);
                while (index >= 0 &&
                       (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
                    index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                    fi = format(index);
                }
            }

            while (index >= 0) {
                setFormat(index, match.capturedLength(), rule.format);
                index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                if (rule.format != whiteSpaceFormat) {
                    fi = format(index);
                    while (index >= 0 && (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat ||
                                          fi == urlFormat)) {
                        index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                        fi = format(index);
                    }
                }
            }
        }

        markdownFonts(text);
    }

    /* if this line isn't a code block with indentation
        but the next line was, rehighlight the next line */
    if (!rehighlightNextBlock && !data->getProperty()) {
        QTextBlock nextBlock = currentBlock().next();
        if (nextBlock.isValid()) {
            if (TextBlockData* nextData = static_cast<TextBlockData*>(nextBlock.userData())) {
                if (nextData->getProperty())
                    rehighlightNextBlock = true;
            }
        }
    }
    if (!rehighlightNextBlock) {
        if (data->labelInfo().length() != oldLabel.length())
            rehighlightNextBlock = true;
        else if (!data->labelInfo().isEmpty()) {
            QTextBlock nextBlock = currentBlock().next();
            if (nextBlock.isValid()) {
                if (TextBlockData* nextData = static_cast<TextBlockData*>(nextBlock.userData())) {
                    if (nextData->labelInfo() != data->labelInfo())
                        rehighlightNextBlock = true;
                }
            }
        }
    }

    /* left parenthesis */
    index = text.indexOf('(');
    fi = format(index);
    while (index >= 0 && (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
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
        while (index >= 0 &&
               (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
            index = text.indexOf('(', index + 1);
            fi = format(index);
        }
    }

    /* right parenthesis */
    index = text.indexOf(')');
    fi = format(index);
    while (index >= 0 && (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
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
        while (index >= 0 &&
               (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
            index = text.indexOf(')', index + 1);
            fi = format(index);
        }
    }

    /* left brace */
    index = text.indexOf('{');
    fi = format(index);
    while (index >= 0 && (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
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
        while (index >= 0 &&
               (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
            index = text.indexOf('{', index + 1);
            fi = format(index);
        }
    }

    /* right brace */
    index = text.indexOf('}');
    fi = format(index);
    while (index >= 0 && (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
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
        while (index >= 0 &&
               (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
            index = text.indexOf('}', index + 1);
            fi = format(index);
        }
    }

    /* left bracket */
    index = text.indexOf('[');
    fi = format(index);
    while (index >= 0 && (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
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
        while (index >= 0 &&
               (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
            index = text.indexOf('[', index + 1);
            fi = format(index);
        }
    }

    /* right bracket */
    index = text.indexOf(']');
    fi = format(index);
    while (index >= 0 && (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
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
        while (index >= 0 &&
               (fi == blockQuoteFormat || fi == codeBlockFormat || fi == commentFormat || fi == urlFormat)) {
            index = text.indexOf(']', index + 1);
            fi = format(index);
        }
    }

    setCurrentBlockUserData(data);

    if (rehighlightNextBlock) {
        QTextBlock nextBlock = currentBlock().next();
        if (nextBlock.isValid())
            QMetaObject::invokeMethod(this, "rehighlightBlock", Qt::QueuedConnection, Q_ARG(QTextBlock, nextBlock));
    }
}

}  // namespace Texxy
