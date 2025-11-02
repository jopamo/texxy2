// src/features/highlighter/highlighter-block.cpp
/*
 * texxy/highlighter/highlighter-block.cpp
 */

#include "highlighter.h"

#include <algorithm>
#include <QMetaObject>

namespace Texxy {

/*************************/
// Start syntax highlighting!
void Highlighter::highlightBlock(const QString& text) {
    if (progLan.isEmpty())
        return;

    if (progLan == "json") {  // Json's huge lines are also handled separately because of its special syntax
        highlightJsonBlock(text);
        return;
    }
    if (progLan == "xml") {  // Optimized SVG files can have huge lines with more than 10000 characters
        highlightXmlBlock(text);
        return;
    }

    int bn = currentBlock().blockNumber();
    bool mainFormatting(bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber());

    int txtL = text.length();
    if (txtL <= maxBlockSize_) {
        /* If the paragraph separators are shown, the unformatted text
           will be grayed out. So, we should restore its real color here.
           This is also safe when the paragraph separators are hidden. */
        if (mainFormatting)
            setFormat(0, txtL, mainFormat);

        if (progLan == "fountain") {
            highlightFountainBlock(text);
            return;
        }
        if (progLan == "yaml") {
            highlightYamlBlock(text);
            return;
        }
        if (progLan == "markdown") {
            highlightMarkdownBlock(text);
            return;
        }
        if (progLan == "reST") {
            highlightReSTBlock(text);
            return;
        }
        if (progLan == "tcl") {
            highlightTclBlock(text);
            return;
        }
        if (progLan == "lua") {
            highlightLuaBlock(text);
            return;
        }
    }

    bool rehighlightNextBlock = false;
    int oldOpenNests = 0;
    QSet<int> oldOpenQuotes;   // to be used in SH_CmndSubstVar() (and perl, ruby, css, rust and cmake)
    bool oldProperty = false;  // to be used with perl, ruby, pascal, java and cmake
    QString oldLabel;          // to be used with perl, ruby and LaTeX
    if (TextBlockData* oldData = static_cast<TextBlockData*>(currentBlockUserData())) {
        oldOpenNests = oldData->openNests();
        oldOpenQuotes = oldData->openQuotes();
        oldProperty = oldData->getProperty();
        oldLabel = oldData->labelInfo();
    }

    int index;
    TextBlockData* data = new TextBlockData;
    data->setLastState(currentBlockState());  // remember the last state (which may not be -1)
    setCurrentBlockUserData(data);            // to be fed in later
    setCurrentBlockState(0);                  // start highlightng, with 0 as the neutral state

    /* set a limit on line length */
    if (txtL > maxBlockSize_) {
        setFormat(0, txtL, translucentFormat);
        data->setHighlighted();  // completely highlighted
        return;
    }

    /* Java is formatted separately, partially because of "Javadoc"
       but also because its single quotes are for literal characters */
    if (progLan == "java") {
        singleLineJavaComment(text);
        JavaQuote(text);
        multiLineJavaComment(text);

        if (mainFormatting)
            javaMainFormatting(text);

        javaBraces(text);

        setCurrentBlockUserData(data);
        if (currentBlockState() == data->lastState() && data->getProperty() != oldProperty) {
            QTextBlock nextBlock = currentBlock().next();
            if (nextBlock.isValid())
                QMetaObject::invokeMethod(this, "rehighlightBlock", Qt::QueuedConnection, Q_ARG(QTextBlock, nextBlock));
        }
        return;
    }

    /********************
     * "Here" Documents *
     ********************/

    if (progLan == "sh" || progLan == "perl" || progLan == "ruby") {
        /* first, handle "__DATA__" in perl */
        if (progLan == "perl") {
            static const QRegularExpression perlData("^\\s*__(DATA|END)__");
            QRegularExpressionMatch match;
            if (previousBlockState() == updateState  // only used below to distinguish "__DATA__"
                || (previousBlockState() <= 0 && text.indexOf(perlData, 0, &match) == 0)) {
                /* ensure that the main format is applied */
                if (!mainFormatting)
                    setFormat(0, txtL, mainFormat);

                if (match.capturedLength() > 0) {
                    QTextCharFormat dataFormat = neutralFormat;
                    dataFormat.setFontWeight(QFont::Bold);
                    setFormat(0, match.capturedLength(), dataFormat);
                }
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
                for (const HighlightingRule& rule : std::as_const(highlightingRules))
#else
                for (const HighlightingRule& rule : qAsConst(highlightingRules))
#endif
                {
                    if (rule.format == whiteSpaceFormat) {
                        index = text.indexOf(rule.pattern, 0, &match);
                        while (index >= 0) {
                            setFormat(index, match.capturedLength(), rule.format);
                            index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                        }
                        break;
                    }
                }
                setCurrentBlockState(updateState);  // completely highlighted
                data->setHighlighted();
                return;
            }
        }
        if (isHereDocument(text)) {
            data->setHighlighted();  // completely highlighted
            /* transfer the info on open quotes inside code blocks downwards */
            if (data->openNests() > 0) {
                QTextBlock nextBlock = currentBlock().next();
                if (nextBlock.isValid()) {
                    if (TextBlockData* nextData = static_cast<TextBlockData*>(nextBlock.userData())) {
                        if (nextData->openQuotes() != data->openQuotes() ||
                            (nextBlock.userState() >= 0 && nextBlock.userState() < endState))  // end delimiter
                        {
                            QMetaObject::invokeMethod(this, "rehighlightBlock", Qt::QueuedConnection,
                                                      Q_ARG(QTextBlock, nextBlock));
                        }
                    }
                }
            }
            return;
        }
    }
    /* just for debian control file */
    else if (progLan == "deb")
        debControlFormatting(text);

    /************************
     * Single-Line Comments *
     ************************/

    if (progLan != "html")
        singleLineComment(text, 0);

    /* this is only for setting the format of
       command substitution variables in bash */
    rehighlightNextBlock |= SH_CmndSubstVar(text, data, oldOpenNests, oldOpenQuotes);

    /*******************
     * Python Comments *
     *******************/

    pythonMLComment(text, 0);

    /**********************************
     * Pascal Quotations and Comments *
     **********************************/
    if (progLan == "pascal") {
        singleLinePascalComment(text);
        pascalQuote(text);
        multiLinePascalComment(text);
        if (currentBlockState() == data->lastState())
            rehighlightNextBlock |= (data->getProperty() != oldProperty);
    }
    /******************
     * LaTeX Formulae *
     ******************/
    else if (progLan == "LaTeX") {
        latexFormula(text);
        if (data->labelInfo() != oldLabel)
            rehighlightNextBlock = true;
    }
    /*****************************************
     * (Multiline) Quotations as well as CSS *
     *****************************************/
    else if (progLan == "sh")  // bash has its own method
        SH_MultiLineQuote(text);
    else if (progLan == "toml")  // Toml has its own method
        tomlQuote(text);
    else if (progLan == "css") {  // quotes and urls are highlighted by cssHighlighter() inside CSS values
        cssHighlighter(text, mainFormatting);
        rehighlightNextBlock |= (data->openNests() != oldOpenNests);
    }
    else if (multilineQuote_)
        rehighlightNextBlock |= multiLineQuote(text);

    /**********************
     * Multiline Comments *
     **********************/

    if (progLan == "cmake")
        rehighlightNextBlock |= cmakeDoubleBrackets(text, oldOpenNests, oldProperty);
    else if (!commentStartExpression.pattern().isEmpty() && progLan != "python")
        rehighlightNextBlock |=
            multiLineComment(text, 0, commentStartExpression, commentEndExpression, commentState, commentFormat);

    /* only javascript, qml, perl and ruby */
    multiLineRegex(text, 0);

    /* "Property" is used for knowing about Perl's backquotes,
        "label" is used for delimiter strings, and "OpenNests" for
        paired delimiters as well as Rust's raw string literals. */
    if ((progLan == "perl" || progLan == "ruby" || progLan == "rust") && currentBlockState() == data->lastState()) {
        rehighlightNextBlock |=
            (data->labelInfo() != oldLabel || data->getProperty() != oldProperty || data->openNests() != oldOpenNests);
    }

    QTextCharFormat fi;

    /*************
     * HTML Only *
     *************/

    if (progLan == "html") {
        htmlBrackets(text);
        htmlCSSHighlighter(text);
        htmlJavascript(text);
        /* also consider quotes and URLs inside CSS values */
        rehighlightNextBlock |= (data->openNests() != oldOpenNests);
        /* go to braces matching */
    }

    /*******************
     * Main Formatting *
     *******************/

    // we format html embedded javascript in htmlJavascript()
    else if (mainFormatting) {
        data->setHighlighted();  // completely highlighted
        QRegularExpressionMatch match;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
        for (const HighlightingRule& rule : std::as_const(highlightingRules))
#else
        for (const HighlightingRule& rule : qAsConst(highlightingRules))
#endif
        {
            /* single-line comments are already formatted */
            if (rule.format == commentFormat)
                continue;

            index = text.indexOf(rule.pattern, 0, &match);
            /* skip quotes and all comments */
            if (rule.format != whiteSpaceFormat) {
                fi = format(index);
                while (index >= 0 && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat ||
                                      fi == commentFormat || fi == urlFormat || fi == regexFormat)) {
                    index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                    fi = format(index);
                }
            }

            while (index >= 0) {
                int length = match.capturedLength();
                int l = length;
                /* In c/c++, the neutral pattern after "#define" may contain
                   a (double-)slash but it's always good to check whether a
                   part of the match is inside an already formatted region. */
                if (rule.format != whiteSpaceFormat) {
                    while (format (index + l - 1) == commentFormat
                           /*|| format (index + l - 1) == urlFormat
                           || format (index + l - 1) == quoteFormat
                           || format (index + l - 1) == altQuoteFormat
                           || format (index + l - 1) == urlInsideQuoteFormat
                           || format (index + l - 1) == regexFormat*/)
                    {
                        --l;
                    }
                }
                setFormat(index, l, rule.format);
                index = text.indexOf(rule.pattern, index + length, &match);

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

    /*********************************************
     * Parentheses, Braces and Brackets Matching *
     *********************************************/

    auto shouldSkipBracket = [&](int pos, bool checkEscaped) {
        if (pos < 0)
            return false;
        QTextCharFormat formatAtPos = format(pos);
        if (formatAtPos == quoteFormat || formatAtPos == altQuoteFormat || formatAtPos == urlInsideQuoteFormat ||
            formatAtPos == commentFormat || formatAtPos == urlFormat || formatAtPos == regexFormat) {
            return true;
        }
        return checkEscaped && progLan == "sh" && isEscapedChar(text, pos);
    };

    auto collectBracketPositions = [&](auto newInfo, QChar symbol, bool checkEscaped) {
        int pos = text.indexOf(symbol);
        while (pos >= 0 && shouldSkipBracket(pos, checkEscaped)) {
            pos = text.indexOf(symbol, pos + 1);
        }
        while (pos >= 0) {
            auto* info = newInfo();
            info->character = symbol.toLatin1();
            info->position = pos;
            data->insertInfo(info);

            pos = text.indexOf(symbol, pos + 1);
            while (pos >= 0 && shouldSkipBracket(pos, checkEscaped)) {
                pos = text.indexOf(symbol, pos + 1);
            }
        }
    };

    collectBracketPositions([]() { return new ParenthesisInfo; }, '(', true);
    collectBracketPositions([]() { return new ParenthesisInfo; }, ')', true);
    collectBracketPositions([]() { return new BraceInfo; }, '{', false);
    collectBracketPositions([]() { return new BraceInfo; }, '}', false);
    collectBracketPositions([]() { return new BracketInfo; }, '[', true);
    collectBracketPositions([]() { return new BracketInfo; }, ']', true);

    setCurrentBlockUserData(data);

    if (rehighlightNextBlock) {
        QTextBlock nextBlock = currentBlock().next();
        if (nextBlock.isValid())
            QMetaObject::invokeMethod(this, "rehighlightBlock", Qt::QueuedConnection, Q_ARG(QTextBlock, nextBlock));
    }
}

}  // namespace Texxy
