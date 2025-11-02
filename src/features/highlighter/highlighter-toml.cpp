// src/features/highlighter/highlighter-toml.cpp

#include "highlighter.h"

namespace Texxy {

void Highlighter::tomlQuote(const QString& text) {
    TextBlockData* tomlData = static_cast<TextBlockData*>(currentBlock().userData());
    QRegularExpressionMatch quoteMatch;
    QRegularExpression quoteExpression = mixedQuoteMark;
    int quote = doubleQuoteState;
    static const QRegularExpression tripleQuoteExp("\"{3}");
    static const QRegularExpression tripleSingleQuoteExp("'{3}");

    /* find the start quote */
    int index = 0;
    int prevState = previousBlockState();
    if (prevState != doubleQuoteState && prevState != singleQuoteState) {
        index = text.indexOf(quoteExpression);
        if (index >= 0) {
            if (format(index) == commentFormat || format(index) == urlFormat)
                return;  // inside (single-line) comment
            /* distinguish between the quote kinds */
            if (text.at(index) == quoteMark.pattern().at(0)) {
                if (text.length() > index + 2 && text.at(index + 1) == '\"' && text.at(index + 2) == '\"') {
                    quoteExpression = tripleQuoteExp;
                }
                else
                    quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else {
                if (text.length() > index + 2 && text.at(index + 1) == '\'' && text.at(index + 2) == '\'') {
                    quoteExpression = tripleSingleQuoteExp;
                }
                else
                    quoteExpression = singleQuoteMark;
                quote = singleQuoteState;
            }
        }
    }
    else  // if we're inside a quotation from the previous line
    {
        /* ... distinguish between the quote kinds by checking the previous line */
        quote = prevState;
        bool tripleQuote = false;
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid()) {
            if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData()))
                tripleQuote = prevData->getProperty();
        }
        if (quote == doubleQuoteState) {
            quoteExpression = tripleQuote ? tripleQuoteExp : quoteMark;
        }
        else {
            quoteExpression = tripleQuote ? tripleSingleQuoteExp : singleQuoteMark;
        }
    }

    while (index >= 0) {
        /* if the search is continued... */
        if (quoteExpression == mixedQuoteMark) {
            /* ... distinguish between the quote kinds again
               because the quote mark may have changed */
            if (text.at(index) == quoteMark.pattern().at(0)) {
                if (text.length() > index + 2 && text.at(index + 1) == '\"' && text.at(index + 2) == '\"') {
                    quoteExpression = tripleQuoteExp;
                }
                else
                    quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else {
                if (text.length() > index + 2 && text.at(index + 1) == '\'' && text.at(index + 2) == '\'') {
                    quoteExpression = tripleSingleQuoteExp;
                }
                else
                    quoteExpression = singleQuoteMark;
                quote = singleQuoteState;
            }
        }

        int endIndex;
        /* if there's no start quote ... */
        if (index == 0 && (prevState == doubleQuoteState || prevState == singleQuoteState)) {
            /* ... search for the end quote from the line start */
            endIndex = text.indexOf(quoteExpression, 0, &quoteMatch);
        }
        else  // otherwise, search from the start quote
            endIndex = text.indexOf(quoteExpression, index + 1, &quoteMatch);

        /* check if the end quote is escaped */
        while (isEscapedQuote(text, endIndex, false))
            endIndex = text.indexOf(quoteExpression, endIndex + 1, &quoteMatch);

        if (endIndex == -1) {
            if (quoteExpression == singleQuoteMark || (quoteExpression == quoteMark && !textEndsWithBackSlash(text))) {
                /* multiline double quotes need backslash and
                   there's no multiline single quote */
                endIndex = text.length();
            }
            else if (quoteExpression != quoteMark && tomlData)
                tomlData->setProperty(true);
        }

        int quoteLength;
        if (endIndex == -1) {
            setCurrentBlockState(quote);
            quoteLength = text.length() - index;
        }
        else
            quoteLength = endIndex - index + quoteMatch.capturedLength();
        setFormat(index, quoteLength, quote == doubleQuoteState ? quoteFormat : altQuoteFormat);
        /* URLs inside quotes */
        QString str = text.sliced(index, quoteLength);
        int urlIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((urlIndex = str.indexOf(urlPattern, urlIndex, &urlMatch)) > -1) {
            setFormat(urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
            urlIndex += urlMatch.capturedLength();
        }

        /* the next quote may be different */
        quoteExpression = mixedQuoteMark;
        index = text.indexOf(quoteExpression, index + quoteLength);
        if (format(index) == commentFormat || format(index) == urlFormat)
            return;
    }
}

}  // namespace Texxy
