// src/features/highlighter/highlighter-html.cpp

#include "highlighter.h"

namespace Texxy {

// This should be called before "htmlCSSHighlighter()" and "htmlJavascript()".
void Highlighter::htmlBrackets(const QString& text, const int start) {
    if (progLan != "html")
        return;

    /*****************************
     * (Multiline) HTML Brackets *
     *****************************/

    int braIndex = start;
    int indx = 0;
    QRegularExpressionMatch startMatch;
    static const QRegularExpression braStartExp("<(?!\\!)/{0,1}[A-Za-z0-9_\\-]+");
    QRegularExpressionMatch endMatch;
    static const QRegularExpression braEndExp(">");
    static const QRegularExpression styleExp("<(style|STYLE)$|<(style|STYLE)\\s+[^>]*");
    bool isStyle(false);
    QTextCharFormat htmlBraFormat;
    htmlBraFormat.setFontWeight(QFont::Bold);
    htmlBraFormat.setForeground(Violet);

    int prevState = previousBlockState();
    if (braIndex > 0 || (prevState != singleQuoteState && prevState != doubleQuoteState &&
                         (prevState < htmlBracketState || prevState > htmlStyleSingleQuoteState))) {
        braIndex = text.indexOf(braStartExp, start, &startMatch);
        while (format(braIndex) == commentFormat || format(braIndex) == urlFormat)
            braIndex = text.indexOf(braStartExp, braIndex + 1, &startMatch);
        if (braIndex > -1) {
            indx = text.indexOf(styleExp, start);
            while (format(indx) == commentFormat || format(indx) == urlFormat)
                indx = text.indexOf(styleExp, indx + 1);
            isStyle = indx > -1 && braIndex == indx;
        }
    }
    else if (braIndex == 0 && (prevState == htmlStyleState
                               /* these quote states are set only with styles */
                               || prevState == htmlStyleSingleQuoteState || prevState == htmlStyleDoubleQuoteState)) {
        isStyle = true;
    }

    int bn = currentBlock().blockNumber();
    bool mainFormatting(bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber());
    // bool hugeText (text.length() > 50000);
    int firstBraIndex = braIndex;  // to check progress in the following loop
    while (braIndex >= 0) {
        /*if (hugeText)
        {
            setFormat (braIndex, text.length() - braIndex, translucentFormat);
            break;
        }*/
        int braEndIndex;

        int matched = 0;
        if (braIndex == 0 && (prevState == singleQuoteState || prevState == doubleQuoteState ||
                              (prevState >= htmlBracketState && prevState <= htmlStyleSingleQuoteState))) {
            braEndIndex = text.indexOf(braEndExp, 0, &endMatch);
        }
        else {
            matched = startMatch.capturedLength();
            braEndIndex = text.indexOf(braEndExp, braIndex + matched, &endMatch);
        }

        int len;
        if (braEndIndex == -1) {
            len = text.length() - braIndex;
            if (isStyle)
                setCurrentBlockState(htmlStyleState);
            else
                setCurrentBlockState(htmlBracketState);
        }
        else {
            len = braEndIndex - braIndex + endMatch.capturedLength();
        }

        if (matched > 0)
            setFormat(braIndex, matched, htmlBraFormat);
        if (braEndIndex > -1)
            setFormat(braEndIndex, endMatch.capturedLength(), htmlBraFormat);

        int endLimit;
        if (braEndIndex == -1)
            endLimit = text.length();
        else
            endLimit = braEndIndex;

        /***************************
         * (Multiline) HTML Quotes *
         ***************************/

        int quoteIndex = braIndex;
        QRegularExpressionMatch quoteMatch;
        QRegularExpression quoteExpression = mixedQuoteMark;
        int quote = doubleQuoteState;

        /* find the start quote */
        if (braIndex > firstBraIndex  // when we're in another bracket
            || (prevState != doubleQuoteState && prevState != singleQuoteState &&
                prevState != htmlStyleSingleQuoteState && prevState != htmlStyleDoubleQuoteState)) {
            quoteIndex = text.indexOf(quoteExpression, braIndex);

            /* if the start quote is found... */
            if (quoteIndex >= braIndex && quoteIndex <= endLimit) {
                /* ... distinguish between double and single quotes */
                if (text.at(quoteIndex) == quoteMark.pattern().at(0)) {
                    quoteExpression = quoteMark;
                    quote = currentBlockState() == htmlStyleState ? htmlStyleDoubleQuoteState : doubleQuoteState;
                }
                else {
                    quoteExpression = singleQuoteMark;
                    quote = currentBlockState() == htmlStyleState ? htmlStyleSingleQuoteState : singleQuoteState;
                }
            }
        }
        else  // but if we're inside a quotation...
        {
            /* ... distinguish between the two quote kinds
               by checking the previous line */
            quote = prevState;
            if (quote == doubleQuoteState || quote == htmlStyleDoubleQuoteState)
                quoteExpression = quoteMark;
            else
                quoteExpression = singleQuoteMark;
        }

        while (quoteIndex >= braIndex && quoteIndex <= endLimit) {
            /* if the search is continued... */
            if (quoteExpression == mixedQuoteMark) {
                /* ... distinguish between double and single quotes
                   again because the quote mark may have changed */
                if (text.at(quoteIndex) == quoteMark.pattern().at(0)) {
                    quoteExpression = quoteMark;
                    quote = currentBlockState() == htmlStyleState ? htmlStyleDoubleQuoteState : doubleQuoteState;
                }
                else {
                    quoteExpression = singleQuoteMark;
                    quote = currentBlockState() == htmlStyleState ? htmlStyleSingleQuoteState : singleQuoteState;
                }
            }

            int quoteEndIndex = text.indexOf(quoteExpression, quoteIndex + 1, &quoteMatch);
            if (quoteIndex == braIndex &&
                (prevState == doubleQuoteState || prevState == singleQuoteState ||
                 prevState == htmlStyleSingleQuoteState || prevState == htmlStyleDoubleQuoteState)) {
                quoteEndIndex = text.indexOf(quoteExpression, braIndex, &quoteMatch);
            }

            int Matched = 0;
            if (quoteEndIndex == -1) {
                if (braEndIndex > -1)
                    quoteEndIndex = braEndIndex;
            }
            else {
                if (quoteEndIndex > endLimit)
                    quoteEndIndex = endLimit;
                else
                    Matched = quoteMatch.capturedLength();
            }

            int quoteLength;
            if (quoteEndIndex == -1) {
                if (currentBlockState() == htmlBracketState || currentBlockState() == htmlStyleState) {
                    setCurrentBlockState(quote);
                }
                quoteLength = text.length() - quoteIndex;
            }
            else
                quoteLength = quoteEndIndex - quoteIndex + Matched;
            setFormat(quoteIndex, quoteLength, quoteExpression == quoteMark ? quoteFormat : altQuoteFormat);

            /* the next quote may be different */
            quoteExpression = mixedQuoteMark;
            quoteIndex = text.indexOf(quoteExpression, quoteIndex + quoteLength);
        }

        /*******************************
         * (Multiline) HTML Attributes *
         *******************************/

        if (mainFormatting) {
            QTextCharFormat htmlAttributeFormat;
            htmlAttributeFormat.setFontItalic(true);
            htmlAttributeFormat.setForeground(Brown);
            QRegularExpressionMatch attMatch;
            static const QRegularExpression attExp("[A-Za-z0-9_\\-]+(?=\\s*\\=)");
            int attIndex = text.indexOf(attExp, braIndex, &attMatch);
            QTextCharFormat fi = format(attIndex);
            while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat) {
                attIndex += attMatch.capturedLength();
                fi = format(attIndex);
                while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat) {
                    ++attIndex;
                    fi = format(attIndex);
                }
                attIndex = text.indexOf(attExp, attIndex, &attMatch);
                fi = format(attIndex);
            }
            while (attIndex >= braIndex && attIndex < endLimit) {
                setFormat(attIndex, attMatch.capturedLength(), htmlAttributeFormat);
                attIndex = text.indexOf(attExp, attIndex + attMatch.capturedLength(), &attMatch);
                fi = format(attIndex);
                while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat) {
                    attIndex += attMatch.capturedLength();
                    fi = format(attIndex);
                    while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat) {
                        ++attIndex;
                        fi = format(attIndex);
                    }
                    attIndex = text.indexOf(attExp, attIndex, &attMatch);
                    fi = format(attIndex);
                }
            }
        }

        indx = braIndex + len;
        braIndex = text.indexOf(braStartExp, braIndex + len, &startMatch);
        while (format(braIndex) == commentFormat || format(braIndex) == urlFormat)
            braIndex = text.indexOf(braStartExp, braIndex + 1, &startMatch);
        if (braIndex > -1) {
            indx = text.indexOf(styleExp, indx);
            while (format(indx) == commentFormat || format(indx) == urlFormat)
                indx = text.indexOf(styleExp, indx + 1);
            isStyle = indx > -1 && braIndex == indx;
        }
        else
            isStyle = false;
    }

    /* at last, format whitespaces */
    if (mainFormatting) {
        static_cast<TextBlockData*>(currentBlock().userData())->setHighlighted();  // completely highlighted
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
        for (const HighlightingRule& rule : std::as_const(highlightingRules))
#else
        for (const HighlightingRule& rule : qAsConst(highlightingRules))
#endif
        {
            if (rule.format == whiteSpaceFormat) {
                QRegularExpressionMatch match;
                int index = text.indexOf(rule.pattern, start, &match);
                while (index >= 0) {
                    setFormat(index, match.capturedLength(), rule.format);
                    index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                }

                /* also, mark encoded and unencoded ampersands */
                QRegularExpression ampersand("&");
                QTextCharFormat encodedFormat;
                encodedFormat.setForeground(DarkMagenta);
                encodedFormat.setFontItalic(true);
                QTextCharFormat specialFormat = encodedFormat;
                specialFormat.setFontWeight(QFont::Bold);

                index = text.indexOf(ampersand, start);
                while (index >= 0 && format(index) != mainFormat)
                    index = text.indexOf(ampersand, index + 1);
                while (index >= 0) {
                    QString str = text.mid(index, 6);
                    if (str == "&nbsp;") {
                        setFormat(index, 6, specialFormat);
                        index = text.indexOf(ampersand, index + 6);
                    }
                    else if (str.startsWith("&amp;")) {
                        setFormat(index, 5, specialFormat);
                        index = text.indexOf(ampersand, index + 5);
                    }
                    else if (str.startsWith("&lt;") || str.startsWith("&gt;")) {
                        setFormat(index, 4, specialFormat);
                        index = text.indexOf(ampersand, index + 4);
                    }
                    else {
                        str = text.mid(index);
                        if (str.indexOf(
                                QRegularExpression("^&(#[0-9]+|[a-zA-Z]+[a-zA-Z0-9_:\\.\\-]*|#[xX][0-9a-fA-F]+);"), 0,
                                &match) >
                            -1) {  // accept "&name;", "&number;" and "&hexadecimal;" but format them differently
                            setFormat(index, match.capturedLength(), encodedFormat);
                            index = text.indexOf(ampersand, index + match.capturedLength());
                        }
                        else {
                            setFormat(index, 1, errorFormat);
                            index = text.indexOf(ampersand, index + 1);
                        }
                    }
                    while (index >= 0 && format(index) != mainFormat)
                        index = text.indexOf(ampersand, index + 1);
                }

                break;
            }
        }
    }
}
/*************************/
void Highlighter::htmlCSSHighlighter(const QString& text, const int start) {
    if (progLan != "html")
        return;

    int cssIndex = start;

    QRegularExpressionMatch startMatch;
    static const QRegularExpression cssStartExp("<(style|STYLE)>|<(style|STYLE)\\s+[^>]*>");
    QRegularExpressionMatch endMatch;
    static const QRegularExpression cssEndExp("</(style|STYLE)\\s*>");
    QRegularExpressionMatch braMatch;
    static const QRegularExpression braEndExp(">");

    /* switch to css temporarily */
    commentStartExpression = htmlSubcommetStart;
    commentEndExpression = htmlSubcommetEnd;
    progLan = "css";

    bool wasCSS(false);
    int prevState = previousBlockState();
    bool wasStyle(prevState == htmlStyleState || prevState == htmlStyleSingleQuoteState ||
                  prevState == htmlStyleDoubleQuoteState);
    QTextBlock prevBlock = currentBlock().previous();
    if (prevBlock.isValid()) {
        if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData()))
            wasCSS = prevData->labelInfo() == "CSS";  // it's labeled below
    }

    QTextCharFormat fi;
    int matched = 0;
    if ((!wasCSS || start > 0) && !wasStyle) {
        cssIndex = text.indexOf(cssStartExp, start, &startMatch);
        fi = format(cssIndex);
        while (cssIndex >= 0 && (fi == commentFormat || fi == urlFormat || fi == quoteFormat || fi == altQuoteFormat ||
                                 fi == urlInsideQuoteFormat)) {
            cssIndex = text.indexOf(cssStartExp, cssIndex + startMatch.capturedLength(), &startMatch);
            fi = format(cssIndex);
        }
    }
    else if (wasStyle) {
        cssIndex = text.indexOf(braEndExp, start, &braMatch);
        fi = format(cssIndex);
        while (cssIndex >= 0 && (fi == commentFormat || fi == urlFormat || fi == quoteFormat || fi == altQuoteFormat ||
                                 fi == urlInsideQuoteFormat)) {
            cssIndex = text.indexOf(braEndExp, cssIndex + 1, &braMatch);
            fi = format(cssIndex);
        }
        if (cssIndex > -1)
            matched = braMatch.capturedLength();  // 1
    }
    TextBlockData* curData = static_cast<TextBlockData*>(currentBlock().userData());
    int bn = currentBlock().blockNumber();
    bool mainFormatting(bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber());
    while (cssIndex >= 0) {
        /* single-line style bracket (<style ...>) */
        if (matched == 0 && (!wasCSS || cssIndex > 0))
            matched = startMatch.capturedLength();

        /* starting from here, clear all html formats...
           (NOTE: "mainFormat" is used instead of "neutralFormat"
           for HTML ampersands to be formatted correctly.) */
        setFormat(cssIndex + matched, text.length() - cssIndex - matched, mainFormat);
        setCurrentBlockState(0);

        /* ... and apply the css formats */
        cssHighlighter(text, mainFormatting, cssIndex + matched);
        multiLineComment(text, cssIndex + matched, commentStartExpression, commentEndExpression, htmlCSSCommentState,
                         commentFormat);
        if (mainFormatting) {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
            for (const HighlightingRule& rule : std::as_const(highlightingRules))
#else
            for (const HighlightingRule& rule : qAsConst(highlightingRules))
#endif
            {  // CSS doesn't have any main formatting except for witesapces
                if (rule.format == whiteSpaceFormat) {
                    QRegularExpressionMatch match;
                    int index = text.indexOf(rule.pattern, start, &match);
                    while (index >= 0) {
                        setFormat(index, match.capturedLength(), rule.format);
                        index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                    }
                }
            }
        }

        /* now, search for the end of the css block */
        int cssEndIndex;
        if (cssIndex == 0 && wasCSS)
            cssEndIndex = text.indexOf(cssEndExp, 0, &endMatch);
        else {
            cssEndIndex = text.indexOf(cssEndExp, cssIndex + matched, &endMatch);
        }

        fi = format(cssEndIndex);
        while (cssEndIndex > -1 && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat ||
                                    fi == commentFormat || fi == urlFormat)) {
            cssEndIndex = text.indexOf(cssEndExp, cssEndIndex + endMatch.capturedLength(), &endMatch);
            fi = format(cssEndIndex);
        }

        int len;
        if (cssEndIndex == -1) {
            if (currentBlockState() == 0)
                setCurrentBlockState(htmlCSSState);  // for updating the next line
            /* Since the next line couldn't be processed based on the state of this line,
               we label this line to show that it's written in css and not html. */
            curData->insertInfo("CSS");
            len = text.length() - cssIndex;
        }
        else {
            len = cssEndIndex - cssIndex + endMatch.capturedLength();
            /* if the css block ends at this line, format
               the rest of the line as an html code again */
            setFormat(cssEndIndex, text.length() - cssEndIndex, mainFormat);
            setCurrentBlockState(0);
            progLan = "html";
            commentStartExpression = htmlCommetStart;
            commentEndExpression = htmlCommetEnd;
            htmlBrackets(text, cssEndIndex);
            commentStartExpression = htmlSubcommetStart;
            commentEndExpression = htmlSubcommetEnd;
            progLan = "css";
        }

        cssIndex = text.indexOf(cssStartExp, cssIndex + len, &startMatch);
        fi = format(cssIndex);
        while (cssIndex >= 0 && (fi == commentFormat || fi == urlFormat || fi == quoteFormat || fi == altQuoteFormat ||
                                 fi == urlInsideQuoteFormat)) {
            cssIndex = text.indexOf(cssStartExp, cssIndex + startMatch.capturedLength(), &startMatch);
            fi = format(cssIndex);
        }
        matched = 0;  // single-line style bracket (<style ...>)
    }

    /* revert to html */
    progLan = "html";
    commentStartExpression = htmlCommetStart;
    commentEndExpression = htmlCommetEnd;
}
/*************************/
void Highlighter::htmlJavascript(const QString& text) {
    if (progLan != "html")
        return;

    int javaIndex = 0;

    QRegularExpressionMatch startMatch;
    // QRegularExpression javaStartExp
    // ("<(script|SCRIPT)\\s+[^<>]*(((language|LANGUAGE)\\s*\\=\\s*\"\\s*)|((TYPE|type)\\s*\\=\\s*\"\\s*(TEXT|text)/))(JavaScript|javascript)\\s*\"[^<>]*>");
    static const QRegularExpression javaStartExp("<(script|SCRIPT)\\s*>|<(script|SCRIPT)\\s+[^<>]*>");
    QRegularExpressionMatch endMatch;
    static const QRegularExpression javaEndExp("</(script|SCRIPT)\\s*>");

    /* switch to javascript temporarily */
    commentStartExpression = htmlSubcommetStart;
    commentEndExpression = htmlSubcommetEnd;
    progLan = "javascript";
    multilineQuote_ = true;  // needed alongside progLan

    bool wasJavascript(false);
    QTextBlock prevBlock = currentBlock().previous();
    if (prevBlock.isValid()) {
        if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData()))
            wasJavascript = prevData->labelInfo() == "JS";  // it's labeled below
    }

    QTextCharFormat fi;
    if (!wasJavascript) {
        javaIndex = text.indexOf(javaStartExp, 0, &startMatch);
        fi = format(javaIndex);
        while (javaIndex >= 0 && (fi == commentFormat || fi == urlFormat || fi == quoteFormat || fi == altQuoteFormat ||
                                  fi == urlInsideQuoteFormat)) {
            javaIndex = text.indexOf(javaStartExp, javaIndex + startMatch.capturedLength(), &startMatch);
            fi = format(javaIndex);
        }
    }
    int matched = 0;
    TextBlockData* curData = static_cast<TextBlockData*>(currentBlock().userData());
    int bn = currentBlock().blockNumber();
    bool mainFormatting(bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber());
    while (javaIndex >= 0) {
        if (!wasJavascript || javaIndex > 0) {
            matched = startMatch.capturedLength();
            curData->insertLastFormattedQuote(javaIndex + matched);
            curData->insertLastFormattedRegex(javaIndex + matched);
        }

        /* starting from here, clear all html formats... */
        setFormat(javaIndex + matched, text.length() - javaIndex - matched, mainFormat);
        setCurrentBlockState(0);

        /* ... and apply the javascript formats */
        singleLineComment(text, javaIndex + matched);
        /* javascript single-line comment doesn't comment out
           the end of a javascript block inside HTML */
        int tmpIndx = -1;
        if (currentBlockState() == regexExtraState) {
            tmpIndx = javaIndex + matched;
            while (tmpIndx < text.length() && format(tmpIndx) != commentFormat)
                ++tmpIndx;
            tmpIndx = text.indexOf(javaEndExp, tmpIndx);
        }
        if (tmpIndx > -1) {
            setCurrentBlockState(0);
            setFormat(tmpIndx, text.length() - tmpIndx, mainFormat);
            QString txt = text.left(tmpIndx);
            multiLineQuote(txt, javaIndex + matched, htmlJavaCommentState);
            multiLineComment(txt, javaIndex + matched, commentStartExpression, commentEndExpression,
                             htmlJavaCommentState, commentFormat);
            multiLineRegex(txt, javaIndex + matched);
        }
        else {
            multiLineQuote(text, javaIndex + matched, htmlJavaCommentState);
            multiLineComment(text, javaIndex + matched, commentStartExpression, commentEndExpression,
                             htmlJavaCommentState, commentFormat);
            multiLineRegex(text, javaIndex + matched);
        }
        if (mainFormatting) {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
            for (const HighlightingRule& rule : std::as_const(highlightingRules))
#else
            for (const HighlightingRule& rule : qAsConst(highlightingRules))
#endif
            {
                if (rule.format == commentFormat)
                    continue;

                QRegularExpressionMatch match;
                int index = text.indexOf(rule.pattern, javaIndex + matched, &match);
                if (rule.format != whiteSpaceFormat) {
                    fi = format(index);
                    while (index >= 0 && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat ||
                                          fi == commentFormat || fi == urlFormat || fi == regexFormat)) {
                        index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                        fi = format(index);
                    }
                }

                while (index >= 0) {
                    setFormat(index, match.capturedLength(), rule.format);
                    index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);

                    if (rule.format != whiteSpaceFormat) {
                        fi = format(index);
                        while (index >= 0 && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat ||
                                              fi == commentFormat || fi == urlFormat || fi == regexFormat)) {
                            index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                            fi = format(index);
                        }
                    }
                }
            }
        }

        /* now, search for the end of the javascript block */
        int javaEndIndex;
        if (javaIndex == 0 && wasJavascript)
            javaEndIndex = text.indexOf(javaEndExp, 0, &endMatch);
        else {
            javaEndIndex = text.indexOf(javaEndExp, javaIndex + matched, &endMatch);
        }

        fi = format(javaEndIndex);
        while (javaEndIndex > -1 && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat ||
                                     fi == commentFormat || fi == urlFormat || fi == regexFormat)) {
            javaEndIndex = text.indexOf(javaEndExp, javaEndIndex + endMatch.capturedLength(), &endMatch);
            fi = format(javaEndIndex);
        }

        int len;
        if (javaEndIndex == -1) {
            if (currentBlockState() == 0)
                setCurrentBlockState(htmlJavaState);  // for updating the next line
            /* Since the next line couldn't be processed based on the state of this line,
               we label this line to show that it's written in javascript and not html. */
            curData->insertInfo("JS");
            len = text.length() - javaIndex;
        }
        else {
            len = javaEndIndex - javaIndex + endMatch.capturedLength();
            /* if the javascript block ends at this line,
               format the rest of the line as an html code again */
            setFormat(javaEndIndex, text.length() - javaEndIndex, mainFormat);
            setCurrentBlockState(0);
            progLan = "html";
            multilineQuote_ = false;
            commentStartExpression = htmlCommetStart;
            commentEndExpression = htmlCommetEnd;
            htmlBrackets(text, javaEndIndex);
            htmlCSSHighlighter(text, javaEndIndex);
            commentStartExpression = htmlSubcommetStart;
            commentEndExpression = htmlSubcommetEnd;
            progLan = "javascript";
            multilineQuote_ = true;
        }

        javaIndex = text.indexOf(javaStartExp, javaIndex + len, &startMatch);
        fi = format(javaEndIndex);
        while (javaIndex > -1 && (fi == commentFormat || fi == urlFormat || fi == quoteFormat || fi == altQuoteFormat ||
                                  fi == urlInsideQuoteFormat)) {
            javaIndex = text.indexOf(javaStartExp, javaIndex + startMatch.capturedLength(), &startMatch);
            fi = format(javaEndIndex);
        }
    }

    /* revert to html */
    progLan = "html";
    multilineQuote_ = false;
    commentStartExpression = htmlCommetStart;
    commentEndExpression = htmlCommetEnd;
}

}  // namespace Texxy
