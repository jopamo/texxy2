

#include "highlighter.h"

namespace Texxy {

static inline int rustRawLiteral(const QString& text, int index) {
    /* The start should be b?r#+\" and the number of occurrences
       of "#" determines what the end should be. */
    int N = 0;
    while (index > 0 && text.at(index - 1) == '#') {
        ++N;
        --index;
    }
    if (N > 0 && index > 0 && text.at(index - 1) == 'r') {
        --index;
        if (index == 0)
            return N;
        --index;
        QChar c = text.at(index);
        if (c == 'b') {
            if (index == 0)
                return N;
            --index;
            c = text.at(index);
        }
        if (c == '\'') {
            // -> highlighter.cpp -> rust single quotes
            static const QRegularExpression rustSingleQuote(
                "'([^'\\\\]{0,1}|\\\\(r|t|n|'|\\\"|\\\\|x[0-9a-fA-F]{2}))\\K'");
            if (text.lastIndexOf(rustSingleQuote, index) == index)
                return N;
        }
        else if (c.isNumber() || !c.isLetterOrNumber())
            return N;
    }
    return 0;
}

static inline bool endsRawLiteral(const QString& text, int endIndex, int N) {
    if (endIndex > text.length() - 1 - N)
        return false;
    for (int i = 1; i <= N; ++i) {
        if (text.at(endIndex + i) != '#')
            return false;
    }
    return true;
}

void Highlighter::multiLineRustQuote(const QString& text) {
    int index = 0;

    /* for Rust's raw string literals */
    int N = 0;
    TextBlockData* bData = nullptr;
    bData = static_cast<TextBlockData*>(currentBlock().userData());

    int prevState = previousBlockState();
    if (prevState != doubleQuoteState) {  // find the start quote
        index = text.indexOf(quoteMark, index);
        /* skip escaped start quotes and all comments */
        while (isEscapedQuote(text, index, true) || isMLCommented(text, index, commentState)) {
            index = text.indexOf(quoteMark, index + 1);
        }
        if (format(index) == commentFormat || format(index) == urlFormat)  // single-line comment
            return;
        N = rustRawLiteral(text, index);
    }
    else {
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid()) {
            if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData()))
                N = prevData->openNests();
        }
    }

    while (index >= 0) {
        int endIndex;
        /* if there's no start quote ... */
        if (index == 0 && prevState == doubleQuoteState) {
            /* ... search for the end quote from the line start */
            endIndex = text.indexOf(quoteMark, 0);
        }
        else  // otherwise, search from the start quote
            endIndex = text.indexOf(quoteMark, index + 1);

        if (N > 0) {  // check if the quote is inside a rust raw string literal
            while (endIndex > -1 && !endsRawLiteral(text, endIndex, N))
                endIndex = text.indexOf(quoteMark, endIndex + 1);
        }
        else {  // check if the quote is escaped
            while (isEscapedQuote(text, endIndex, false))
                endIndex = text.indexOf(quoteMark, endIndex + 1);
        }

        int quoteLength;
        if (endIndex == -1) {
            setCurrentBlockState(doubleQuoteState);
            quoteLength = text.length() - index;
            if (bData && N > 0) {
                bData->insertNestInfo(N);
                setCurrentBlockUserData(bData);
                /* NOTE: The next block will be rehighlighted at highlightBlock()
                         if "openNests()" is changed. */
            }
        }
        else
            quoteLength = endIndex - index + 1;
        setFormat(index, quoteLength, quoteFormat);
        if (N > 0) {
            if (index > N) {  // start quote was found
                if (index > N + 1 && text.at(index - N - 2) == 'b')
                    setFormat(index - N - 2, N + 3, rawLiteralFormat);  // for br#+\"
                else
                    setFormat(index - N - 1, N + 2, rawLiteralFormat);  // for r#+\"
            }
            if (endIndex > -1)                                                // end quote was found
                setFormat(index + quoteLength - 1, N + 1, rawLiteralFormat);  // for \"#+
        }

        QString str = text.sliced(index, quoteLength);
        int urlIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((urlIndex = str.indexOf(urlPattern, urlIndex, &urlMatch)) > -1) {
            setFormat(urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
            urlIndex += urlMatch.capturedLength();
        }

        index = text.indexOf(quoteMark, index + quoteLength);

        while (isEscapedQuote(text, index, true) || isMLCommented(text, index, commentState, endIndex + 1)) {
            index = text.indexOf(quoteMark, index + 1);
        }
        while (format(index) == commentFormat || format(index) == urlFormat)
            index = text.indexOf(quoteMark, index + 1);
        N = rustRawLiteral(text, index);
    }
}
/*************************/
bool Highlighter::isRustQuoted(const QString& text, const int index, const int start) {
    if (index < 0 || start < 0 || index < start)
        return false;

    int N = 0;  // for Rust's raw string literals

    int pos = start - 1;
    bool res = false;
    int n;
    if (pos == -1) {
        if (previousBlockState() != doubleQuoteState)
            n = 0;
        else {
            n = 1;
            res = true;
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid()) {
                if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData()))
                    N = prevData->openNests();
            }
        }
    }
    else
        n = 0;  // a new search from the last position

    int nxtPos;
    while ((nxtPos = text.indexOf(quoteMark, pos + 1)) >= 0) {
        /* skip formatted comments */
        if (format(nxtPos) == commentFormat || format(nxtPos) == urlFormat) {
            pos = nxtPos;
            continue;
        }

        ++n;
        if ((n % 2 == 0  // an escaped end quote...
             && (N > 0 ? !endsRawLiteral(text, nxtPos, N) : isEscapedQuote(text, nxtPos, false)))
            // ... or an escaped start quote
            || (n % 2 != 0 && isEscapedQuote(text, nxtPos, true))) {
            --n;
            pos = nxtPos;
            continue;
        }

        if (index < nxtPos) {
            if (n % 2 == 0)
                res = true;
            else
                res = false;
            break;
        }

        if (n % 2 != 0)
            N = rustRawLiteral(text, nxtPos);

        /* "pos" might be negative next time */
        if (n % 2 == 0)
            res = false;
        else
            res = true;

        pos = nxtPos;
    }

    return res;
}

}  // namespace Texxy
