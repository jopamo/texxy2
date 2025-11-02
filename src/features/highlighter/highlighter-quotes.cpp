// src/features/highlighter/highlighter-quotes.cpp
/*
 * texxy/highlighter/highlighter-quotes.cpp
 */

#include "highlighter.h"

#include <QTextBlock>

#include <algorithm>

namespace Texxy {

bool Highlighter::isEscapedChar(const QString& text, const int pos) const {
    if (pos < 1)
        return false;
    int i = 0;
    while (pos - i - 1 >= 0 && text.at(pos - i - 1) == '\\')
        ++i;
    if (i % 2 != 0)
        return true;
    return false;
}
/*************************/
// Checks if a start quote is inside a Yaml key (as in ab""c).
bool Highlighter::isYamlKeyQuote(const QString& key, const int pos) {
    static int lastKeyQuote = -1;
    int indx = pos;
    if (indx > 0 && indx < key.length()) {
        while (indx > 0 && key.at(indx - 1).isSpace())
            --indx;
        if (indx > 0) {
            QChar c = key.at(indx - 1);
            if (c != '\"' && c != '\'') {
                lastKeyQuote = pos;
                return true;
            }
            if (lastKeyQuote == indx - 1) {
                lastKeyQuote = pos;
                return true;
            }
        }
    }
    lastKeyQuote = -1;
    return false;
}
/*************************/
// Check if a start or end quotation mark (positioned at "pos") is escaped.
// If "skipCommandSign" is true (only for SH), start double quotes are escaped before "$(".
bool Highlighter::isEscapedQuote(const QString& text, const int pos, bool isStartQuote, bool skipCommandSign) {
    if (pos < 0)
        return false;

    if (progLan == "html" /* || progLan == "xml"*/)  // xml is formatted separately
        return false;

    if (progLan == "yaml") {
        if (isStartQuote) {
            if (format(pos) == codeBlockFormat)  // inside a literal block
                return true;
            QRegularExpressionMatch match;
            if (text.indexOf(QRegularExpression("^(\\s*-\\s)+\\s*"), 0, &match) == 0) {
                if (match.capturedLength() == pos)
                    return false;  // a start quote isn't escaped at the beginning of a list
            }
            /* Skip the start quote if it's inside a key or value.
               NOTE: In keys, "(?:(?!(\\{|\\[|,|:\\s|\\s#)).)*" is used instead of "[^{\\[:,#]*"
                     because ":" should be followed by a space to make a key-value. */
            if (format(pos) == neutralFormat) {  // inside preformatted braces, when multiLineQuote() is called (not
                                                 // needed; repeated below)
                int index = text.lastIndexOf(
                    QRegularExpression("(^|{|,|\\[)\\s*\\K(?:(?!(\\{|\\[|,|:\\s|\\s#)).)*(:\\s+)?"), pos, &match);
                if (index > -1 && index <= pos && index + match.capturedLength() > pos &&
                    isYamlKeyQuote(match.captured(), pos - index)) {
                    return true;
                }
                index =
                    text.lastIndexOf(QRegularExpression("(^|{|,|\\[)[^:#]*:\\s+\\K[^{\\[,#\\s][^,#]*"), pos, &match);
                if (index > -1 && index < pos && index + match.capturedLength() > pos)
                    return true;
            }
            else {
                /* inside braces before preformatting (indirectly used by yamlOpenBraces()) */
                int index = text.lastIndexOf(
                    QRegularExpression("(^|{|,|\\[)\\s*\\K(?:(?!(\\{|\\[|,|:\\s|\\s#)).)*(:\\s+)?"), pos, &match);
                if (index > -1 && index <= pos && index + match.capturedLength() > pos &&
                    isYamlKeyQuote(match.captured(), pos - index)) {
                    return true;
                }
                index =
                    text.lastIndexOf(QRegularExpression("(^|{|,|\\[)[^:#]*:\\s+\\K[^{\\[,#\\s][^,#]*"), pos, &match);
                if (index > -1 && index < pos && index + match.capturedLength() > pos)
                    return true;
                /* outside braces */
                index = text.lastIndexOf(QRegularExpression("^\\s*\\K(?:(?!(\\{|\\[|,|:\\s|\\s#)).)*(:\\s+)?"), pos,
                                         &match);
                if (index > -1 && index < pos && index + match.capturedLength() > pos &&
                    isYamlKeyQuote(match.captured(), pos - index)) {
                    return true;
                }
                index = text.lastIndexOf(QRegularExpression("^[^:#]*:\\s+\\K[^\\[\\s#].*"), pos, &match);
                if (index > -1 && index < pos && index + match.capturedLength() > pos)
                    return true;
            }
        }
        else if (text.length() > pos && text.at(pos) == '\'') {  // a pair of single quotes means escaping them
            static int lastEscapedQuote = -1;
            if (lastEscapedQuote == pos && pos > 0 && text.at(pos - 1) == '\'')  // the second quote
                return true;
            if ((pos == 0 || text.at(pos - 1) != '\'' || lastEscapedQuote == pos - 1) && text.length() > pos + 1 &&
                text.at(pos + 1) == '\'') {  // the first quote
                lastEscapedQuote = pos + 1;
                return true;
            }
            lastEscapedQuote = -1;
        }
    }
    else if (progLan == "go") {
        if (text.at(pos) == '`')
            return false;
        if (isStartQuote) {
            if (text.at(pos) == '\"' && pos > 0 && pos < text.length() - 1 && text.at(pos + 1) == '\'' &&
                (text.at(pos - 1) == '\'' || (pos > 1 && text.at(pos - 2) == '\'' && text.at(pos - 1) == '\\'))) {
                return true;  // '"' or '\"'
            }
            else
                return false;
        }
        return isEscapedChar(text, pos);
    }
    else if (progLan == "rust") {
        if (isStartQuote) {
            if (pos < text.length() - 1 && text.at(pos + 1) == '\'' &&
                (pos > 0 &&
                 (text.at(pos - 1) == '\'' || (pos > 1 && text.at(pos - 2) == '\'' && text.at(pos - 1) == '\\')))) {
                return true;
            }
        }
    }

    /* there's no need to check for quote marks because this function is used only with them */
    /*if (progLan == "perl"
        && pos != text.indexOf (quoteMark, pos)
        && pos != text.indexOf ("\'", pos)
        && pos != text.indexOf ("`", pos))
    {
        return false;
    }
    if (pos != text.indexOf (quoteMark, pos)
        && pos != text.indexOf (QRegularExpression ("\'"), pos))
    {
        return false;
    }*/

    /* check if the quote surrounds a here-doc delimiter */
    if ((currentBlockState() >= endState || currentBlockState() < -1) && currentBlockState() % 2 == 0) {
        QRegularExpressionMatch match;
        QRegularExpression delimPart(progLan == "ruby" ? "<<(-|~){0,1}" : progLan == "perl" ? "<<~?\\s*" : "<<\\s*");
        if (text.lastIndexOf(delimPart, pos, &match) == pos - match.capturedLength())
            return true;        // escaped start quote
        if (progLan == "perl")  // space is allowed
            delimPart.setPattern(
                "<<~?(?:\\s*)(\'[A-Za-z0-9_\\s]+)|<<~?(?:\\s*)(\"[A-Za-z0-9_\\s]+)|<<~?(?:\\s*)(`[A-Za-z0-9_\\s]+)");
        else if (progLan == "ruby")
            delimPart.setPattern("<<(?:-|~){0,1}(\'[A-Za-z0-9]+)|<<(?:-|~){0,1}(\"[A-Za-z0-9]+)");
        else
            delimPart.setPattern("<<(?:\\s*)(\'[A-Za-z0-9_]+)|<<(?:\\s*)(\"[A-Za-z0-9_]+)");
        if (text.lastIndexOf(delimPart, pos, &match) == pos - match.capturedLength())
            return true;  // escaped end quote
    }

    /* escaped start quotes are just for Bash, Perl, markdown and yaml
       (and tcl, for which this function is never called) */
    if (isStartQuote) {
        if (progLan == "perl") {
            if (pos >= 1) {
                if (text.at(pos - 1) == '$')  // in Perl, $' has a (deprecated?) meaning
                    return true;
                if (text.at(pos) == '\'') {
                    if (text.at(pos - 1) == '&')
                        return true;
                    int index = pos - 1;
                    while (index >= 0 && (text.at(index).isLetterOrNumber() || text.at(index) == '_'))
                        --index;
                    if (index >= 0 && (text.at(index) == '$' || text.at(index) == '@' || text.at(index) == '%' ||
                                       text.at(index) == '*'
                                       /*|| text.at (index) == '!'*/)) {
                        return true;
                    }
                }
            }
            return false;  // no other case of escaping at the start
        }
        else if (progLan == "c" || progLan == "cpp") {
            /*if (text.at (pos) == '\''
                && pos > 0 && text.at (pos - 1).isLetterOrNumber())
            {
                return true;
            }*/
            return false;
        }
        else if (progLan != "sh" && progLan != "makefile" && progLan != "cmake" && progLan != "yaml") {
            return false;
        }

        if (skipCommandSign && text.at(pos) == quoteMark.pattern().at(0) &&
            text.indexOf(QRegularExpression("[^\"]*\\$\\("), pos) == pos + 1) {
            return true;
        }
    }

    int i = 0;
    while (pos - i > 0 && text.at(pos - i - 1) == '\\')
        ++i;
    /* only an odd number of backslashes means that the quote is escaped */
    if (i % 2 != 0 && (((progLan == "yaml" || progLan == "toml") && text.at(pos) == quoteMark.pattern().at(0))
                       /* for these languages, both single and double quotes can be escaped (also for perl?) */
                       || progLan == "c" || progLan == "cpp" || progLan == "javascript" || progLan == "qml" ||
                       progLan == "python" || progLan == "perl" || progLan == "dart" || progLan == "php" ||
                       progLan == "ruby"
                       /* rust only has double quotes */
                       || progLan == "rust"
                       /* however, in Bash, single quote can be escaped only at start */
                       || ((progLan == "sh" || progLan == "makefile" || progLan == "cmake") &&
                           (isStartQuote || text.at(pos) == quoteMark.pattern().at(0))))) {
        return true;
    }

    if (progLan == "ruby" &&
        text.at(pos) == quoteMark.pattern().at(0)) {  // a minimal support for command substitution "#{...}"
        QRegularExpressionMatch match;
        int index = text.lastIndexOf(QRegularExpression("#\\{[^\\}]*"), pos, &match);
        if (index > -1 && index < pos && index + match.capturedLength() > pos)
            return true;
    }

    return false;
}
/*************************/
// Checks if a character is inside quotation marks, considering the language
// (should be used with care because it gives correct results only in special places).
// If "skipCommandSign" is true (only for SH), start double quotes are escaped before "$(".
bool Highlighter::isQuoted(const QString& text, const int index, bool skipCommandSign, const int start) {
    if (!hasQuotes_)
        return false;

    if (progLan == "perl" || progLan == "ruby")
        return isPerlQuoted(text, index);
    if (progLan == "javascript" || progLan == "qml")
        return isJSQuoted(text, index);
    if (progLan == "tcl")
        return isTclQuoted(text, index, start);
    if (progLan == "rust")
        return isRustQuoted(text, index, start);

    if (index < 0 || start < 0 || index < start)
        return false;

    int pos = start - 1;

    bool res = false;
    int N;
    QRegularExpression quoteExpression;
    if (mixedQuotes_)
        quoteExpression = mixedQuoteMark;
    else
        quoteExpression = quoteMark;
    if (pos == -1) {
        int prevState = previousBlockState();
        if ((prevState < doubleQuoteState || prevState > SH_MixedSingleQuoteState) &&
            prevState != htmlStyleSingleQuoteState && prevState != htmlStyleDoubleQuoteState) {
            N = 0;
        }
        else {
            N = 1;
            res = true;
            if (mixedQuotes_) {
                if (prevState == doubleQuoteState || prevState == SH_DoubleQuoteState ||
                    prevState == SH_MixedDoubleQuoteState || prevState == htmlStyleDoubleQuoteState) {
                    quoteExpression = quoteMark;
                    if (skipCommandSign) {
                        if (text.indexOf(QRegularExpression("[^\"]*\\$\\("), 0) == 0) {
                            N = 0;
                            res = false;
                        }
                        else {
                            QTextBlock prevBlock = currentBlock().previous();
                            if (prevBlock.isValid()) {
                                if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData())) {
                                    int N = prevData->openNests();
                                    if (N > 0 &&
                                        (prevState == doubleQuoteState || !prevData->openQuotes().contains(N))) {
                                        N = 0;
                                        res = false;
                                    }
                                }
                            }
                        }
                    }
                }
                else
                    quoteExpression = singleQuoteMark;
            }
        }
    }
    else
        N = 0;  // a new search from the last position

    int nxtPos;
    while ((nxtPos = text.indexOf(quoteExpression, pos + 1)) >= 0) {
        /* skip formatted comments */
        if (format(nxtPos) == commentFormat || format(nxtPos) == urlFormat) {
            pos = nxtPos;
            continue;
        }

        ++N;
        if ((N % 2 == 0  // an escaped end quote...
             && isEscapedQuote(text, nxtPos, false)) ||
            (N % 2 != 0  // ... or an escaped start quote
             && (isEscapedQuote(text, nxtPos, true, skipCommandSign)
                 /*|| isInsideRegex (text, nxtPos)*/)))  // ... or a start quote inside regex
        {
            --N;
            pos = nxtPos;
            continue;
        }

        if (index < nxtPos) {
            if (N % 2 == 0)
                res = true;
            else
                res = false;
            break;
        }

        /* "pos" might be negative next time */
        if (N % 2 == 0)
            res = false;
        else
            res = true;

        if (mixedQuotes_) {
            if (N % 2 != 0) {  // each quote neutralizes the other until it's closed
                if (text.at(nxtPos) == quoteMark.pattern().at(0))
                    quoteExpression = quoteMark;
                else
                    quoteExpression = singleQuoteMark;
            }
            else
                quoteExpression = mixedQuoteMark;
        }
        pos = nxtPos;
    }

    return res;
}
/*************************/
// Perl has a separate method to support backquotes.
// Also see multiLinePerlQuote().
bool Highlighter::isPerlQuoted(const QString& text, const int index) {
    if (index < 0)
        return false;

    int pos = -1;

    if (format(index) == quoteFormat || format(index) == altQuoteFormat)
        return true;
    if (TextBlockData* data = static_cast<TextBlockData*>(currentBlock().userData())) {
        pos = data->lastFormattedQuote() - 1;
        if (index <= pos)
            return false;
    }

    bool res = false;
    int N;
    QRegularExpression quoteExpression = mixedQuoteBackquote;
    if (pos == -1) {
        int prevState = previousBlockState();
        if (prevState != doubleQuoteState && prevState != singleQuoteState)
            N = 0;
        else {
            N = 1;
            res = true;
            if (prevState == doubleQuoteState)
                quoteExpression = quoteMark;
            else {
                bool backquoted(false);
                QTextBlock prevBlock = currentBlock().previous();
                if (prevBlock.isValid()) {
                    TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData());
                    if (prevData && prevData->getProperty())
                        backquoted = true;
                }
                if (backquoted)
                    quoteExpression = backQuote;
                else
                    quoteExpression = singleQuoteMark;
            }
        }
    }
    else
        N = 0;  // a new search from the last position

    int nxtPos;
    while ((nxtPos = text.indexOf(quoteExpression, pos + 1)) >= 0) {
        /* skip formatted comments */
        if (format(nxtPos) == commentFormat || format(nxtPos) == urlFormat ||
            (N % 2 == 0 && isMLCommented(text, nxtPos, commentState))) {
            pos = nxtPos;
            continue;
        }

        ++N;
        if ((N % 2 == 0  // an escaped end quote...
             && isEscapedQuote(text, nxtPos, false)) ||
            (N % 2 != 0  // ... or an escaped start quote
             && (isEscapedQuote(text, nxtPos, true, false) || isInsideRegex(text, nxtPos)))) {
            if (res) {  // -> isEscapedRegex()
                pos = std::max(pos, 0);
                if (text.at(nxtPos) == quoteMark.pattern().at(0))
                    setFormat(pos, nxtPos - pos + 1, quoteFormat);
                else
                    setFormat(pos, nxtPos - pos + 1, altQuoteFormat);
            }
            --N;
            pos = nxtPos;
            continue;
        }

        if (N % 2 == 0) {  // -> isEscapedRegex()
            if (TextBlockData* data = static_cast<TextBlockData*>(currentBlock().userData()))
                data->insertLastFormattedQuote(nxtPos + 1);
            pos = std::max(pos, 0);
            if (text.at(nxtPos) == quoteMark.pattern().at(0))
                setFormat(pos, nxtPos - pos + 1, quoteFormat);
            else
                setFormat(pos, nxtPos - pos + 1, altQuoteFormat);
        }

        if (index < nxtPos) {
            if (N % 2 == 0)
                res = true;
            else
                res = false;
            break;
        }

        /* "pos" might be negative next time */
        if (N % 2 == 0)
            res = false;
        else
            res = true;

        if (N % 2 != 0) {  // each quote neutralizes the other until it's closed
            if (text.at(nxtPos) == quoteMark.pattern().at(0))
                quoteExpression = quoteMark;
            else if (text.at(nxtPos) == '\'')
                quoteExpression = singleQuoteMark;
            else
                quoteExpression = backQuote;
        }
        else
            quoteExpression = mixedQuoteBackquote;
        pos = nxtPos;
    }

    return res;
}
/*************************/
// JS has a separate method to support backquotes (template literals).
// Also see multiLineJSQuote().
bool Highlighter::isJSQuoted(const QString& text, const int index) {
    if (index < 0)
        return false;

    int pos = -1;

    /* with regex, the text will be formatted below to know whether
       the regex start sign is quoted (-> isEscapedRegex) */
    if (format(index) == quoteFormat || format(index) == altQuoteFormat)
        return true;
    if (TextBlockData* data = static_cast<TextBlockData*>(currentBlock().userData())) {
        pos = data->lastFormattedQuote() - 1;
        if (index <= pos)
            return false;
    }

    bool res = false;
    int N;
    QRegularExpression quoteExpression = mixedQuoteBackquote;
    if (pos == -1) {
        int prevState = previousBlockState();
        if (prevState != doubleQuoteState && prevState != singleQuoteState && prevState != JS_templateLiteralState) {
            N = 0;
        }
        else {
            N = 1;
            res = true;
            if (prevState == doubleQuoteState)
                quoteExpression = quoteMark;
            else if (prevState == singleQuoteState)
                quoteExpression = singleQuoteMark;
            else
                quoteExpression = backQuote;
        }
    }
    else
        N = 0;  // a new search from the last position

    int nxtPos;
    while ((nxtPos = text.indexOf(quoteExpression, pos + 1)) >= 0) {
        /* skip formatted comments */
        if (format(nxtPos) == commentFormat || format(nxtPos) == urlFormat ||
            (N % 2 == 0 &&
             (isMLCommented(text, nxtPos, commentState) || isMLCommented(text, nxtPos, htmlJavaCommentState)))) {
            pos = nxtPos;
            continue;
        }

        ++N;
        if ((N % 2 == 0  // an escaped end quote...
             && isEscapedQuote(text, nxtPos, false)) ||
            (N % 2 != 0  // ... or a start quote inside regex (JS has no escaped start quote)
             && isInsideRegex(text, nxtPos))) {
            if (res) {  // -> isEscapedRegex()
                pos = std::max(pos, 0);
                if (text.at(nxtPos) == quoteMark.pattern().at(0))
                    setFormat(pos, nxtPos - pos + 1, quoteFormat);
                else
                    setFormat(pos, nxtPos - pos + 1, altQuoteFormat);
            }
            --N;
            pos = nxtPos;
            continue;
        }

        if (N % 2 == 0) {  // -> isEscapedRegex()
            if (TextBlockData* data = static_cast<TextBlockData*>(currentBlock().userData()))
                data->insertLastFormattedQuote(nxtPos + 1);
            pos = std::max(pos, 0);
            if (text.at(nxtPos) == quoteMark.pattern().at(0))
                setFormat(pos, nxtPos - pos + 1, quoteFormat);
            else
                setFormat(pos, nxtPos - pos + 1, altQuoteFormat);
        }

        if (index < nxtPos) {
            if (N % 2 == 0)
                res = true;
            else
                res = false;
            break;
        }

        /* "pos" might be negative next time */
        if (N % 2 == 0)
            res = false;
        else
            res = true;

        if (N % 2 != 0) {  // each quote neutralizes the other until it's closed
            if (text.at(nxtPos) == quoteMark.pattern().at(0))
                quoteExpression = quoteMark;
            else if (text.at(nxtPos) == '\'')
                quoteExpression = singleQuoteMark;
            else
                quoteExpression = backQuote;
        }
        else
            quoteExpression = mixedQuoteBackquote;
        pos = nxtPos;
    }

    return res;
}
/*************************/
// Checks if a start quote or a start single-line comment sign is inside a multiline comment.
// If "start" > 0, it will be supposed that "start" is not inside a previous comment.
// (May give an incorrect result with other characters and works only with real comments
// whose state is "comState".)

/*************************/
// This handles multiline python comments separately because they aren't normal.
// It comes after singleLineComment() and before multiLineQuote().
void Highlighter::multiLineJSQuote(const QString& text, const int start, int comState) {
    int index = start;
    QRegularExpressionMatch quoteMatch;
    QRegularExpression quoteExpression = mixedQuoteBackquote;
    int quote = doubleQuoteState;

    /* find the start quote */
    int prevState = previousBlockState();
    if ((prevState != doubleQuoteState && prevState != singleQuoteState && prevState != JS_templateLiteralState) ||
        index > 0) {
        index = text.indexOf(quoteExpression, index);
        /* skip escaped start quotes and all comments */
        while (isEscapedQuote(text, index, true) || isInsideRegex(text, index) ||
               isMLCommented(text, index, comState)) {
            index = text.indexOf(quoteExpression, index + 1);
        }
        while (format(index) == commentFormat || format(index) == urlFormat)  // single-line
            index = text.indexOf(quoteExpression, index + 1);

        /* if the start quote is found... */
        if (index >= 0) {
            /* ... distinguish between double and single quotes */
            if (text.at(index) == quoteMark.pattern().at(0)) {
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else if (text.at(index) == '\'') {
                quoteExpression = singleQuoteMark;
                quote = singleQuoteState;
            }
            else {
                quoteExpression = backQuote;
                quote = JS_templateLiteralState;
            }
        }
    }
    else  // but if we're inside a quotation...
    {
        /* ... distinguish between the two quote kinds
           by checking the previous line */
        quote = prevState;
        if (quote == doubleQuoteState)
            quoteExpression = quoteMark;
        else if (quote == singleQuoteState)
            quoteExpression = singleQuoteMark;
        else
            quoteExpression = backQuote;
    }

    while (index >= 0) {
        /* if the search is continued... */
        if (quoteExpression == mixedQuoteBackquote) {
            /* ... distinguish between double and single quotes
               again because the quote mark may have changed */
            if (text.at(index) == quoteMark.pattern().at(0)) {
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else if (text.at(index) == '\'') {
                quoteExpression = singleQuoteMark;
                quote = singleQuoteState;
            }
            else {
                quoteExpression = backQuote;
                quote = JS_templateLiteralState;
            }
        }

        int endIndex;
        /* if there's no start quote ... */
        if (index == 0 &&
            (prevState == doubleQuoteState || prevState == singleQuoteState || prevState == JS_templateLiteralState)) {
            /* ... search for the end quote from the line start */
            endIndex = text.indexOf(quoteExpression, 0, &quoteMatch);
        }
        else  // otherwise, search from the start quote
            endIndex = text.indexOf(quoteExpression, index + 1, &quoteMatch);

        /* check if the quote is escaped */
        while (isEscapedQuote(text, endIndex, false))
            endIndex = text.indexOf(quoteExpression, endIndex + 1, &quoteMatch);

        int quoteLength;
        if (endIndex == -1) {
            /* In JS, multiline double and single quotes need backslash. */
            if ((quoteExpression == singleQuoteMark || (quoteExpression == quoteMark && progLan != "qml")) &&
                !textEndsWithBackSlash(text)) {  // see NOTE of isEscapedRegex() and also the end of multiLineRegex()
                setCurrentBlockState(regexExtraState);
            }
            else
                setCurrentBlockState(quote);
            quoteLength = text.length() - index;
        }
        else
            quoteLength = endIndex - index + quoteMatch.capturedLength();  // 1

        setFormat(index, quoteLength, quoteExpression == quoteMark ? quoteFormat : altQuoteFormat);

        QString str = text.sliced(index, quoteLength);
        int urlIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((urlIndex = str.indexOf(urlPattern, urlIndex, &urlMatch)) > -1) {
            setFormat(urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
            urlIndex += urlMatch.capturedLength();
        }

        /* the next quote may be different */
        quoteExpression = mixedQuoteBackquote;
        index = text.indexOf(quoteExpression, index + quoteLength);

        /* skip escaped start quotes and all comments */
        while (isEscapedQuote(text, index, true) || isInsideRegex(text, index) ||
               isMLCommented(text, index, comState, endIndex + 1)) {
            index = text.indexOf(quoteExpression, index + 1);
        }
        while (format(index) == commentFormat || format(index) == urlFormat)
            index = text.indexOf(quoteExpression, index + 1);
    }
}
/*************************/
// Handles escaped backslashes too.
bool Highlighter::textEndsWithBackSlash(const QString& text) const {
    QString str = text;
    int n = 0;
    while (!str.isEmpty() && str.endsWith("\\")) {
        str.truncate(str.size() - 1);
        ++n;
    }
    return (n % 2 != 0);
}
/*************************/
// This also covers single-line quotes. It comes after single-line comments
// and before multi-line ones are formatted. "comState" is the comment state,
// whose default is "commentState" but may be different for some languages.
// Sometimes (with multi-language docs), formatting should be started from "start".
bool Highlighter::multiLineQuote(const QString& text, const int start, int comState) {
    if (progLan == "perl" || progLan == "ruby") {
        multiLinePerlQuote(text);
        return false;
    }
    if (progLan == "javascript" || progLan == "qml") {
        multiLineJSQuote(text, start, comState);
        return false;
    }
    if (progLan == "rust") {
        multiLineRustQuote(text);
        return false;
    }
    /* For Tcl, this function is never called. */

    //--------------------
    /* these are only for C++11 raw string literals */
    bool rehighlightNextBlock = false;
    QString delimStr;
    TextBlockData* cppData = nullptr;
    if (progLan == "cpp") {
        cppData = static_cast<TextBlockData*>(currentBlock().userData());
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid()) {
            if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData()))
                delimStr = prevData->labelInfo();
        }
    }
    //--------------------

    int index = start;
    QRegularExpressionMatch quoteMatch;
    QRegularExpression quoteExpression;
    if (mixedQuotes_)
        quoteExpression = mixedQuoteMark;
    else
        quoteExpression = quoteMark;
    int quote = doubleQuoteState;

    /* find the start quote */
    int prevState = previousBlockState();
    if ((prevState != doubleQuoteState && prevState != singleQuoteState) || index > 0) {
        index = text.indexOf(quoteExpression, index);
        /* skip escaped start quotes and all comments */
        while (isEscapedQuote(text, index, true) || isInsideRegex(text, index) ||
               isMLCommented(text, index, comState)) {
            index = text.indexOf(quoteExpression, index + 1);
        }
        while (format(index) == commentFormat || format(index) == urlFormat)  // single-line and Python
            index = text.indexOf(quoteExpression, index + 1);

        /* if the start quote is found... */
        if (index >= 0) {
            if (mixedQuotes_) {
                /* ... distinguish between double and single quotes */
                if (text.at(index) == quoteMark.pattern().at(0)) {
                    if (progLan == "cpp" && index > start) {
                        QRegularExpressionMatch cppMatch;
                        if (text.at(index - 1) == 'R' &&
                            index - 1 == text.indexOf(cppLiteralStart, index - 1, &cppMatch)) {
                            delimStr = ")" + cppMatch.captured(1);
                            setFormat(index - 1, 1, rawLiteralFormat);
                        }
                    }
                    quoteExpression = quoteMark;
                    quote = doubleQuoteState;
                }
                else {
                    quoteExpression = singleQuoteMark;
                    quote = singleQuoteState;
                }
            }
        }
    }
    else  // but if we're inside a quotation...
    {
        /* ... distinguish between the two quote kinds
           by checking the previous line */
        if (mixedQuotes_) {
            quote = prevState;
            if (quote == doubleQuoteState)
                quoteExpression = quoteMark;
            else
                quoteExpression = singleQuoteMark;
        }
    }

    while (index >= 0) {
        /* if the search is continued... */
        if (quoteExpression == mixedQuoteMark) {
            /* ... distinguish between double and single quotes
               again because the quote mark may have changed */
            if (text.at(index) == quoteMark.pattern().at(0)) {
                if (progLan == "cpp" && index > start) {
                    QRegularExpressionMatch cppMatch;
                    if (text.at(index - 1) == 'R' && index - 1 == text.indexOf(cppLiteralStart, index - 1, &cppMatch)) {
                        delimStr = ")" + cppMatch.captured(1);
                        setFormat(index - 1, 1, rawLiteralFormat);
                    }
                }
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else {
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

        if (delimStr.isEmpty()) {  // check if the quote is escaped
            while (isEscapedQuote(text, endIndex, false))
                endIndex = text.indexOf(quoteExpression, endIndex + 1, &quoteMatch);
        }
        else {  // check if the quote is inside a C++11 raw string literal
            while (endIndex > -1 && (endIndex - delimStr.length() < start ||
                                     text.mid(endIndex - delimStr.length(), delimStr.length()) != delimStr)) {
                endIndex = text.indexOf(quoteExpression, endIndex + 1, &quoteMatch);
            }
        }

        if (endIndex == -1) {
            if (progLan == "c" || progLan == "cpp") {
                /* In c and cpp, multiline double quotes need backslash and
                   there's no multiline single quote. Moreover, In C++11,
                   there can be multiline raw string literals. */
                if (quoteExpression == singleQuoteMark ||
                    (quoteExpression == quoteMark && delimStr.isEmpty() && !textEndsWithBackSlash(text))) {
                    endIndex = text.length();
                }
            }
            else if (progLan == "go") {
                if (quoteExpression == quoteMark)  // no multiline double quote
                    endIndex = text.length();
            }
        }

        int quoteLength;
        if (endIndex == -1) {
            setCurrentBlockState(quote);
            quoteLength = text.length() - index;

            /* set the delimiter string for C++11 */
            if (cppData && !delimStr.isEmpty()) {
                /* with a multiline C++11 raw string literal, if the delimiter is changed
                   but the state of the current block isn't changed, the next block won't
                   be highlighted automatically, so it should be rehighlighted forcefully */
                if (cppData->lastState() == quote) {
                    QTextBlock nextBlock = currentBlock().next();
                    if (nextBlock.isValid()) {
                        if (TextBlockData* nextData = static_cast<TextBlockData*>(nextBlock.userData())) {
                            if (nextData->labelInfo() != delimStr)
                                rehighlightNextBlock = true;
                        }
                    }
                }
                cppData->insertInfo(delimStr);
                setCurrentBlockUserData(cppData);
            }
        }
        else
            quoteLength =
                endIndex - index + quoteMatch.capturedLength();  // 1 or 0 (open quotation without ending backslash)
        setFormat(index, quoteLength, quoteExpression == quoteMark ? quoteFormat : altQuoteFormat);
        /* URLs should be formatted in a different way inside quotes because,
           otherwise, there would be no difference between URLs inside quotes and
           those inside comments and so, they couldn't be escaped correctly when needed. */
        QString str = text.sliced(index, quoteLength);
        int urlIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((urlIndex = str.indexOf(urlPattern, urlIndex, &urlMatch)) > -1) {
            setFormat(urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
            urlIndex += urlMatch.capturedLength();
        }

        /* the next quote may be different */
        if (mixedQuotes_)
            quoteExpression = mixedQuoteMark;
        index = text.indexOf(quoteExpression, index + quoteLength);

        /* skip escaped start quotes and all comments */
        while (isEscapedQuote(text, index, true) || isInsideRegex(text, index) ||
               isMLCommented(text, index, comState, endIndex + 1)) {
            index = text.indexOf(quoteExpression, index + 1);
        }
        while (format(index) == commentFormat || format(index) == urlFormat)
            index = text.indexOf(quoteExpression, index + 1);
        delimStr.clear();
    }
    return rehighlightNextBlock;
}
/*************************/
// Perl's (and Ruby's) multiline quote highlighting comes here to support backquotes.
// Also see isPerlQuoted().
void Highlighter::multiLinePerlQuote(const QString& text) {
    int index = 0;
    QRegularExpressionMatch quoteMatch;
    QRegularExpression quoteExpression = mixedQuoteBackquote;
    int quote = doubleQuoteState;

    /* find the start quote */
    int prevState = previousBlockState();
    if (prevState != doubleQuoteState && prevState != singleQuoteState) {
        index = text.indexOf(quoteExpression, index);
        /* skip escaped start quotes and all comments */
        while (isEscapedQuote(text, index, true) || isInsideRegex(text, index))
            index = text.indexOf(quoteExpression, index + 1);
        while (format(index) == commentFormat || format(index) == urlFormat)
            index = text.indexOf(quoteExpression, index + 1);

        /* if the start quote is found... */
        if (index >= 0) {
            /* ... distinguish between the three kinds of quote */
            if (text.at(index) == quoteMark.pattern().at(0)) {
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else {
                if (text.at(index) == '\'')
                    quoteExpression = singleQuoteMark;
                else
                    quoteExpression = backQuote;
                quote = singleQuoteState;
            }
        }
    }
    else  // but if we're inside a quotation...
    {
        /* ... distinguish between the three kinds of quote */
        quote = prevState;
        if (quote == doubleQuoteState)
            quoteExpression = quoteMark;
        else {
            bool backquoted(false);
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid()) {
                TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData());
                if (prevData && prevData->getProperty())
                    backquoted = true;
            }
            if (backquoted)
                quoteExpression = backQuote;
            else
                quoteExpression = singleQuoteMark;
        }
    }

    while (index >= 0) {
        /* if the search is continued... */
        if (quoteExpression == mixedQuoteBackquote) {
            /* ... distinguish between the three kinds of quote
               again because the quote mark may have changed */
            if (text.at(index) == quoteMark.pattern().at(0)) {
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else {
                if (text.at(index) == '\'')
                    quoteExpression = singleQuoteMark;
                else
                    quoteExpression = backQuote;
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

        // check if the quote is escaped
        while (isEscapedQuote(text, endIndex, false))
            endIndex = text.indexOf(quoteExpression, endIndex + 1, &quoteMatch);

        int quoteLength;
        if (endIndex == -1) {
            setCurrentBlockState(quote);
            if (quoteExpression == backQuote) {
                if (TextBlockData* data = static_cast<TextBlockData*>(currentBlock().userData()))
                    data->setProperty(true);
                /* NOTE: The next block will be rehighlighted at highlightBlock()
                         (-> multiLineRegex (text, 0);) if the property is changed. */
            }
            quoteLength = text.length() - index;
        }
        else
            quoteLength = endIndex - index + quoteMatch.capturedLength();  // 1
        setFormat(index, quoteLength, quoteExpression == quoteMark ? quoteFormat : altQuoteFormat);
        QString str = text.sliced(index, quoteLength);
        int urlIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((urlIndex = str.indexOf(urlPattern, urlIndex, &urlMatch)) > -1) {
            setFormat(urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
            urlIndex += urlMatch.capturedLength();
        }

        /* the next quote may be different */
        quoteExpression = mixedQuoteBackquote;
        index = text.indexOf(quoteExpression, index + quoteLength);

        /* skip escaped start quotes and all comments */
        while (isEscapedQuote(text, index, true) || isInsideRegex(text, index))
            index = text.indexOf(quoteExpression, index + 1);
        while (format(index) == commentFormat || format(index) == urlFormat)
            index = text.indexOf(quoteExpression, index + 1);
    }
}

}  // namespace Texxy
