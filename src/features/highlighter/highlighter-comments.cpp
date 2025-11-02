// src/features/highlighter/highlighter-comments.cpp
/*
 * texxy/highlighter/highlighter-comments.cpp
 */

#include "highlighter.h"

#include <QTextBlock>

#include <algorithm>
#include <utility>

namespace Texxy {

bool Highlighter::isMLCommented(const QString& text, const int index, int comState, const int start) {
    if (progLan == "cmake")
        return isCmakeDoubleBracketed(text, index, start);

    if (index < 0 || start < 0 ||
        index < start
        // commentEndExpression is always set if commentStartExpression is
        || commentStartExpression.pattern().isEmpty()) {
        return false;
    }

    /* not for Python */
    if (progLan == "python")
        return false;

    int prevState = previousBlockState();
    if (prevState == nextLineCommentState)
        return true;  // see singleLineComment()

    bool res = false;
    int pos = start - 1;
    int N;
    QRegularExpressionMatch commentMatch;
    QRegularExpression commentExpression;
    if (pos >= 0 || prevState != comState) {
        N = 0;
        commentExpression = commentStartExpression;
    }
    else {
        N = 1;
        res = true;
        commentExpression = commentEndExpression;
    }

    while ((pos = text.indexOf(commentExpression, pos + 1, &commentMatch)) >= 0) {
        /* skip formatted quotations and regex */
        QTextCharFormat fi = format(pos);
        if (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat ||
            fi == regexFormat)  // see multiLineRegex() for the reason
        {
            continue;
        }

        ++N;

        /* All (or most) multiline comments have more than one character
           and this trick is needed for knowing if a double slash follows
           an asterisk without using "lookbehind", for example. */
        if (index < pos + (N % 2 == 0 ? commentMatch.capturedLength() : 0)) {
            if (N % 2 == 0)
                res = true;
            else
                res = false;
            break;
        }

        if (N % 2 != 0) {
            commentExpression = commentEndExpression;
            res = true;
        }
        else {
            commentExpression = commentStartExpression;
            res = false;
        }
    }

    return res;
}
/*************************/
// This handles multiline python comments separately because they aren't normal.
// It comes after singleLineComment() and before multiLineQuote().
void Highlighter::pythonMLComment(const QString& text, const int indx) {
    if (progLan != "python")
        return;

    /* we reset the block state because this method is also called
       during the multiline quotation formatting after clearing formats */
    setCurrentBlockState(-1);

    int index = indx;
    int quote;

    /* find the start quote */
    int prevState = previousBlockState();
    if (prevState != pyDoubleQuoteState && prevState != pySingleQuoteState) {
        index = text.indexOf(commentStartExpression, indx);

        QTextCharFormat fi = format(index);
        while ((index > 0 && isQuoted(text, index - 1))  // because two quotes may follow an end quote
               || (index == 0 && (prevState == doubleQuoteState || prevState == singleQuoteState)) ||
               fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat)  // not needed
        {
            index = text.indexOf(commentStartExpression, index + 3);
            fi = format(index);
        }
        if (format(index) == commentFormat || format(index) == urlFormat)
            return;  // inside a single-line comment

        /* if the comment start is found... */
        if (index >= indx) {
            /* ... distinguish between double and single quotes */
            if (index == text.indexOf(QRegularExpression("\"\"\""), index)) {
                commentStartExpression.setPattern("\"\"\"");
                quote = pyDoubleQuoteState;
            }
            else {
                commentStartExpression.setPattern("\'\'\'");
                quote = pySingleQuoteState;
            }
        }
    }
    else  // but if we're inside a triple quotation...
    {
        /* ... distinguish between the two quote kinds
           by checking the previous line */
        quote = prevState;
        if (quote == pyDoubleQuoteState)
            commentStartExpression.setPattern("\"\"\"");
        else
            commentStartExpression.setPattern("\'\'\'");
    }

    while (index >= indx) {
        /* if the search is continued... */
        if (commentStartExpression.pattern() == "\"\"\"|\'\'\'") {
            /* ... distinguish between double and single quotes
               again because the quote mark may have changed... */
            if (text.at(index) == quoteMark.pattern().at(0)) {
                commentStartExpression.setPattern("\"\"\"");
                quote = pyDoubleQuoteState;
            }
            else {
                commentStartExpression.setPattern("\'\'\'");
                quote = pySingleQuoteState;
            }
        }

        QRegularExpressionMatch startMatch;
        int endIndex;
        /* if there's no start quote ... */
        if (index == indx && (prevState == pyDoubleQuoteState || prevState == pySingleQuoteState)) {
            /* ... search for the end quote from the line start */
            endIndex = text.indexOf(commentStartExpression, indx, &startMatch);
        }
        else  // otherwise, search for the end quote from the start quote
            endIndex = text.indexOf(commentStartExpression, index + 3, &startMatch);

        /* check if the quote is escaped */
        while ((endIndex > 0 &&
                text.at(endIndex - 1) == '\\'
                /* backslash shouldn't be escaped itself */
                && (endIndex < 2 || text.at(endIndex - 2) != '\\'))
               /* also consider ^' and ^" */
               || ((endIndex > 0 && text.at(endIndex - 1) == '^') && (endIndex < 2 || text.at(endIndex - 2) != '\\'))) {
            endIndex = text.indexOf(commentStartExpression, endIndex + 3, &startMatch);
        }

        /* if there's a comment end ... */
        if (endIndex >= 0) {
            /* ... clear the comment format from there to reformat
               because a single-line comment may have changed now */
            int badIndex = endIndex + startMatch.capturedLength();
            if (format(badIndex) == commentFormat || format(badIndex) == urlFormat)
                setFormat(badIndex, text.length() - badIndex, mainFormat);
            singleLineComment(text, badIndex);
        }

        int quoteLength;
        if (endIndex == -1) {
            setCurrentBlockState(quote);
            quoteLength = text.length() - index;
        }
        else
            quoteLength = endIndex - index + startMatch.capturedLength();  // 3
        setFormat(index, quoteLength, commentFormat);

        /* format urls and email addresses inside the comment */
        QString str = text.sliced(index, quoteLength);
        int pIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((pIndex = str.indexOf(urlPattern, pIndex, &urlMatch)) > -1) {
            setFormat(pIndex + index, urlMatch.capturedLength(), urlFormat);
            pIndex += urlMatch.capturedLength();
        }
        /* format note patterns too */
        pIndex = 0;
        while ((pIndex = str.indexOf(notePattern, pIndex, &urlMatch)) > -1) {
            if (format(pIndex + index) != urlFormat)
                setFormat(pIndex + index, urlMatch.capturedLength(), noteFormat);
            pIndex += urlMatch.capturedLength();
        }

        /* the next quote may be different */
        commentStartExpression.setPattern("\"\"\"|\'\'\'");
        index = text.indexOf(commentStartExpression, index + quoteLength);
        QTextCharFormat fi = format(index);
        while ((index > 0 && isQuoted(text, index - 1)) ||
               (index == 0 && (prevState == doubleQuoteState || prevState == singleQuoteState)) || fi == quoteFormat ||
               fi == altQuoteFormat || fi == urlInsideQuoteFormat) {
            index = text.indexOf(commentStartExpression, index + 3);
            fi = format(index);
        }
        if (format(index) == commentFormat || format(index) == urlFormat)
            return;
    }
}
/*************************/
void Highlighter::singleLineComment(const QString& text, const int start) {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
    for (const HighlightingRule& rule : std::as_const(highlightingRules))
#else
    for (const HighlightingRule& rule : qAsConst(highlightingRules))
#endif
    {
        if (rule.format == commentFormat) {
            int startIndex = std::max(start, 0);
            if (previousBlockState() == nextLineCommentState)
                startIndex = 0;
            else {
                startIndex = text.indexOf(rule.pattern, startIndex);
                /* skip quoted comments (and, automatically, those inside multiline python comments) */
                while (startIndex > -1
                       // check quote formats (only for multiLineComment())
                       && (format(startIndex) == quoteFormat || format(startIndex) == altQuoteFormat ||
                           format(startIndex) == urlInsideQuoteFormat
                           // check whether the comment sign is quoted or inside regex
                           || isQuoted(text, startIndex, false, std::max(start, 0)) ||
                           isInsideRegex(text, startIndex)
                           // with troff and LaTeX, the comment sign may be escaped
                           || ((progLan == "troff" || progLan == "LaTeX") && isEscapedChar(text, startIndex)) ||
                           (progLan == "tcl" && text.at(startIndex) == ';' &&
                            insideTclBracedVariable(text, startIndex, std::max(start, 0))))) {
                    startIndex = text.indexOf(rule.pattern, startIndex + 1);
                }
            }
            if (startIndex > -1) {
                int l = text.length();
                setFormat(startIndex, l - startIndex, commentFormat);

                /* also format urls and email addresses inside the comment */
                QString str = text.sliced(startIndex, l - startIndex);
                int pIndex = 0;
                QRegularExpressionMatch urlMatch;
                while ((pIndex = str.indexOf(urlPattern, pIndex, &urlMatch)) > -1) {
                    setFormat(pIndex + startIndex, urlMatch.capturedLength(), urlFormat);
                    pIndex += urlMatch.capturedLength();
                }
                /* format note patterns too */
                pIndex = 0;
                while ((pIndex = str.indexOf(notePattern, pIndex, &urlMatch)) > -1) {
                    if (format(pIndex + startIndex) != urlFormat)
                        setFormat(pIndex + startIndex, urlMatch.capturedLength(), noteFormat);
                    pIndex += urlMatch.capturedLength();
                }

                if (progLan == "javascript" || progLan == "qml") {
                    /* see NOTE of isEscapedRegex() and also the end of multiLineRegex() */
                    setCurrentBlockState(regexExtraState);
                }
                else if ((progLan == "c" || progLan == "cpp") && text.endsWith(QLatin1Char('\\'))) {
                    /* Take care of next-line comments with languages, for which
                       no highlighting function is called after singleLineComment()
                       and before the main formatting in highlightBlock()
                       (only c and c++ for now). */
                    setCurrentBlockState(nextLineCommentState);
                }
            }
            break;
        }
    }
}
/*************************/
bool Highlighter::multiLineComment(const QString& text,
                                   const int index,
                                   const QRegularExpression& commentStartExp,
                                   const QRegularExpression& commentEndExp,
                                   const int commState,
                                   const QTextCharFormat& comFormat) {
    if (index < 0)
        return false;
    int prevState = previousBlockState();
    if (prevState == nextLineCommentState)
        return false;  // was processed by singleLineComment()

    bool rehighlightNextBlock = false;
    int startIndex = index;

    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;

    if (startIndex > 0 ||
        (prevState != commState && prevState != commentInCssBlockState && prevState != commentInCssValueState)) {
        startIndex = text.indexOf(commentStartExp, startIndex, &startMatch);
        /* skip quotations (usually all formatted to this point) and regexes */
        QTextCharFormat fi = format(startIndex);
        while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat ||
               isInsideRegex(text, startIndex)) {
            startIndex = text.indexOf(commentStartExp, startIndex + 1, &startMatch);
            fi = format(startIndex);
        }
        /* skip single-line comments */
        if (format(startIndex) == commentFormat || format(startIndex) == urlFormat)
            startIndex = -1;
    }

    while (startIndex >= 0) {
        int endIndex;
        /* when the comment start is in the prvious line
           and the search for the comment end has just begun... */
        if (startIndex == 0 &&
            (prevState == commState || prevState == commentInCssBlockState || prevState == commentInCssValueState))
            /* ... search for the comment end from the line start */
            endIndex = text.indexOf(commentEndExp, 0, &endMatch);
        else
            endIndex = text.indexOf(commentEndExp, startIndex + startMatch.capturedLength(), &endMatch);

        /* skip quotations */
        QTextCharFormat fi = format(endIndex);
        if (progLan != "fountain")  // in Fountain, altQuoteFormat is used for notes
        {  // FIXME: Is this really needed? Commented quotes are skipped in formatting multi-line quotes.
            while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat) {
                endIndex = text.indexOf(commentEndExp, endIndex + 1, &endMatch);
                fi = format(endIndex);
            }
        }

        if (endIndex >= 0 && /*progLan != "xml" && */ progLan != "html")  // xml is formatted separately
        {
            /* because multiline commnets weren't taken into account in
               singleLineComment(), that method should be used here again */
            int badIndex = endIndex + endMatch.capturedLength();
            bool hadSingleLineComment = false;
            int i = 0;
            for (i = badIndex; i < text.length(); ++i) {
                if (format(i) == commentFormat || format(i) == urlFormat) {
                    setFormat(i, text.length() - i, mainFormat);
                    hadSingleLineComment = true;
                    break;
                }
            }
            singleLineComment(text, badIndex);
            /* because the previous single-line comment may have been
               removed now, quotes should be checked again from its start */
            if (hadSingleLineComment && multilineQuote_)
                rehighlightNextBlock = multiLineQuote(text, i, commState);
        }

        int commentLength;
        if (endIndex == -1) {
            if (currentBlockState() != cssBlockState && currentBlockState() != cssValueState) {
                setCurrentBlockState(commState);
            }
            else {  // CSS
                if (currentBlockState() == cssValueState)
                    setCurrentBlockState(commentInCssValueState);
                else
                    setCurrentBlockState(commentInCssBlockState);
            }
            commentLength = text.length() - startIndex;
        }
        else
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        setFormat(startIndex, commentLength, comFormat);

        /* format urls and email addresses inside the comment */
        QString str = text.sliced(startIndex, commentLength);
        int pIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((pIndex = str.indexOf(urlPattern, pIndex, &urlMatch)) > -1) {
            setFormat(pIndex + startIndex, urlMatch.capturedLength(), urlFormat);
            pIndex += urlMatch.capturedLength();
        }
        /* format note patterns too */
        pIndex = 0;
        while ((pIndex = str.indexOf(notePattern, pIndex, &urlMatch)) > -1) {
            if (format(pIndex + startIndex) != urlFormat)
                setFormat(pIndex + startIndex, urlMatch.capturedLength(), noteFormat);
            pIndex += urlMatch.capturedLength();
        }

        startIndex = text.indexOf(commentStartExp, startIndex + commentLength, &startMatch);

        /* skip single-line comments and quotations again */
        fi = format(startIndex);
        while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat ||
               isInsideRegex(text, startIndex)) {
            startIndex = text.indexOf(commentStartExp, startIndex + 1, &startMatch);
            fi = format(startIndex);
        }
        if (format(startIndex) == commentFormat || format(startIndex) == urlFormat)
            startIndex = -1;
    }

    /* reset the block state if this line created a next-line comment
       whose starting single-line comment sign is commented out now */
    if (currentBlockState() == nextLineCommentState && format(text.size() - 1) != commentFormat &&
        format(text.size() - 1) != urlFormat) {
        setCurrentBlockState(0);
    }

    return rehighlightNextBlock;
}
}  // namespace Texxy
