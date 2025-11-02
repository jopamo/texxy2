/*
 * texxy/highlighter/highlighter-utils.cpp
 */

#include "highlighter.h"

#include <QTextDocument>

#include <algorithm>
#include <utility>

namespace Texxy {

/*************************/
Highlighter::~Highlighter() {
    if (QTextDocument* doc = document()) {
        QTextOption opt = doc->defaultTextOption();
        opt.setFlags(opt.flags() & ~QTextOption::ShowTabsAndSpaces & ~QTextOption::ShowLineAndParagraphSeparators &
                     ~QTextOption::AddSpaceForLineAndParagraphSeparators & ~QTextOption::ShowDocumentTerminator);
        doc->setDefaultTextOption(opt);
    }
}
/*************************/
// Apply a format without overwriting existing comments or quotes.
void Highlighter::setFormatWithoutOverwrite(int start,
                                            int count,
                                            const QTextCharFormat& newFormat,
                                            const QTextCharFormat& oldFormat) {
    int index = start;  // always >= 0
    int indx;
    while (index < start + count) {
        QTextCharFormat fi = format(index);
        while (index < start + count && (fi == oldFormat
                                         // skip comments and quotes
                                         || fi == commentFormat || fi == urlFormat || fi == quoteFormat ||
                                         fi == altQuoteFormat || fi == urlInsideQuoteFormat)) {
            ++index;
            fi = format(index);
        }
        if (index < start + count) {
            indx = index;
            fi = format(indx);
            while (indx < start + count && fi != oldFormat && fi != commentFormat && fi != urlFormat &&
                   fi != quoteFormat && fi != altQuoteFormat && fi != urlInsideQuoteFormat) {
                ++indx;
                fi = format(indx);
            }
            setFormat(index, indx - index, newFormat);
            index = indx;
        }
    }
}
/*************************/
// Check if the current block is inside a "here document" and format it accordingly.
// (Open quotes aren't taken into account when they happen after the start delimiter.)
bool Highlighter::isHereDocument(const QString& text) {
    /*if (progLan != "sh" && progLan != "makefile" && progLan != "cmake"
        && progLan != "perl" && progLan != "ruby")
    {
        return false;
        // "<<([A-Za-z0-9_]+)|<<(\'[A-Za-z0-9_]+\')|<<(\"[A-Za-z0-9_]+\")"
    }*/

    QTextBlock prevBlock = currentBlock().previous();
    int prevState = previousBlockState();

    QTextCharFormat blockFormat;
    blockFormat.setForeground(Violet);
    QTextCharFormat delimFormat = blockFormat;
    delimFormat.setFontWeight(QFont::Bold);
    QString delimStr;

    /* format the start delimiter */
    if (!prevBlock.isValid() || (prevState >= 0 && prevState < endState)) {
        int pos = 0;
        QRegularExpressionMatch match;
        while ((pos = text.indexOf(hereDocDelimiter, pos, &match)) >= 0 &&
               (isQuoted(text, pos, progLan == "sh")  // escaping start double quote before "$("
                || (progLan == "perl" && isInsideRegex(text, pos))))

        {
            pos += match.capturedLength();
        }
        if (pos >= 0) {
            int insideCommentPos;
            if (progLan == "sh") {
                static const QRegularExpression commentSH("^#.*|\\s+#.*");
                insideCommentPos = text.indexOf(commentSH);
            }
            else {
                static const QRegularExpression commentOthers("#.*");
                insideCommentPos = text.indexOf(commentOthers);
            }
            if (insideCommentPos == -1 || pos < insideCommentPos || isQuoted(text, insideCommentPos, progLan == "sh") ||
                (progLan == "perl" &&
                 isInsideRegex(text, insideCommentPos))) {  // the delimiter isn't (single-)commented out
                int i = 1;
                while ((delimStr = match.captured(i)).isEmpty() && i <= 3) {
                    ++i;
                    delimStr = match.captured(i);
                }

                if (progLan == "perl") {
                    if (delimStr.contains('`'))  // Perl's delimiter can have backquotes
                        delimStr = delimStr.split('`').at(1);
                }

                if (!delimStr.isEmpty()) {
                    /* remove quotes */
                    if (delimStr.contains('\''))
                        delimStr = delimStr.split('\'').at(1);
                    if (delimStr.contains('\"'))
                        delimStr = delimStr.split('\"').at(1);
                    /* remove the start backslash (with bash) if it exists */
                    if (delimStr.startsWith("\\"))
                        delimStr = delimStr.remove(0, 1);
                }

                if (!delimStr.isEmpty()) {
                    setFormat(text.indexOf(delimStr, pos), delimStr.length(), delimFormat);

                    if (progLan == "sh") {
                        /* skip double-parenthesis constructs */
                        static const QRegularExpression dpc("(^\\s*|\\$|[\\);&`\\|]\\s*)\\(\\(.+\\)\\)");
                        int index = text.lastIndexOf(dpc, pos, &match);
                        if (index > -1 && index < pos && index + match.capturedLength() > pos) {
                            return false;
                        }

                        int pos1 = pos;
                        while (pos1 > 0 && text.at(pos1 - 1) == '<')
                            --pos1;
                        if ((pos - pos1) % 3 != 0)
                            return false;  // a here-string, not a here-doc

                        if (text.length() > pos + 2 && text.at(pos + 2) == '-') {
                            /* "<<-" causes all leading tab characters to be ignored at
                               the end of the here-doc. So, it should be distinguished. */
                            delimStr = "-" + delimStr;
                        }
                    }
                    int n = static_cast<int>(qHash(delimStr));
                    int state = 2 * (n + (n >= 0 ? endState / 2 + 1 : 0));  // always an even number but maybe negative
                    if (progLan == "sh") {
                        if (isQuoted(text, pos,
                                     false)) {  // to know whether a double quote is added/removed before "$(" in the
                                                // current line
                            state > 0 ? state += 2 : state -= 2;
                        }
                        if (prevState == doubleQuoteState ||
                            prevState == SH_DoubleQuoteState) {   // to know whether a double quote is added/removed
                                                                  // before "$(" in a previous line
                            state > 0 ? state += 4 : state -= 4;  // not 2 -> not to be canceled above
                        }
                    }
                    setCurrentBlockState(state);

                    TextBlockData* data = static_cast<TextBlockData*>(currentBlock().userData());
                    if (!data)
                        return false;
                    data->insertInfo(delimStr);
                    setCurrentBlockUserData(data);

                    return false;
                }
            }
        }
    }

    if (prevState >= endState || prevState < -1) {
        TextBlockData* prevData = nullptr;
        if (prevBlock.isValid())
            prevData = static_cast<TextBlockData*>(prevBlock.userData());
        if (!prevData)
            return false;

        delimStr = prevData->labelInfo();
        int l = 0;
        if (progLan == "perl" || progLan == "ruby") {
            QRegularExpressionMatch rMatch;
            /* the terminating string must appear on a line by itself */
            QRegularExpression r("^\\s*" + delimStr + "(?=\\s*$)");
            if (text.indexOf(r, 0, &rMatch) == 0)
                l = rMatch.capturedLength();
        }
        else  // if (progLan == "sh")
        {
            if (!delimStr.startsWith("-")) {
                if (text == delimStr)
                    l = delimStr.length();
            }
            else if (delimStr.length() > 1) {  // the here-doc started with "<<-"
                QString tmp = delimStr.sliced(1);
                QRegularExpression r("^\\t*" + tmp + "$");
                QRegularExpressionMatch rMatch;
                if (text.indexOf(r, 0, &rMatch) == 0)
                    l = rMatch.capturedLength();
            }
        }
        if (l > 0) {
            /* format the end delimiter */
            setFormat(0, l, delimFormat);
            return false;
        }
        else {
            /* format the contents */
            TextBlockData* data = static_cast<TextBlockData*>(currentBlock().userData());
            if (!data)
                return false;
            data->insertInfo(delimStr);
            /* inherit the previous data property and open nests
               (the property shows if the here-doc is double quoted;
               it's set for the delimiter in "SH_MultiLineQuote()") */
            if (bool p = prevData->getProperty())
                data->setProperty(p);
            int N = prevData->openNests();
            if (N > 0) {
                data->insertNestInfo(N);
                QSet<int> Q = prevData->openQuotes();
                if (!Q.isEmpty())
                    data->insertOpenQuotes(Q);
            }
            setCurrentBlockUserData(data);
            if (prevState % 2 == 0)  // the delimiter was in the previous line
                setCurrentBlockState(prevState - 1);
            else
                setCurrentBlockState(prevState);
            setFormat(0, text.length(), blockFormat);

            /* also, format whitespaces */
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
            for (const HighlightingRule& rule : std::as_const(highlightingRules))
#else
            for (const HighlightingRule& rule : qAsConst(highlightingRules))
#endif
            {
                if (rule.format == whiteSpaceFormat) {
                    QRegularExpressionMatch match;
                    int index = text.indexOf(rule.pattern, 0, &match);
                    while (index >= 0) {
                        setFormat(index, match.capturedLength(), rule.format);
                        index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                    }
                    break;
                }
            }

            return true;
        }
    }

    return false;
}
/*************************/
void Highlighter::debControlFormatting(const QString& text) {
    if (text.isEmpty())
        return;
    bool formatFurther(false);
    QRegularExpressionMatch expMatch;
    QRegularExpression exp;
    int indx = 0;
    QTextCharFormat debFormat;
    if (text.indexOf(QRegularExpression("^[^\\s:]+:(?=\\s*)")) == 0) {
        formatFurther = true;
        exp.setPattern("^[^\\s:]+(?=:)");
        if (text.indexOf(exp, 0, &expMatch) == 0) {
            /* before ":" */
            debFormat.setFontWeight(QFont::Bold);
            debFormat.setForeground(DarkBlue);
            setFormat(0, expMatch.capturedLength(), debFormat);

            /* ":" */
            debFormat.setForeground(DarkMagenta);
            indx = text.indexOf(":");
            setFormat(indx, 1, debFormat);
            indx++;

            if (indx < text.size()) {
                /* after ":" */
                debFormat.setFontWeight(QFont::Normal);
                debFormat.setForeground(DarkGreenAlt);
                setFormat(indx, text.size() - indx, debFormat);
            }
        }
    }
    else if (text.indexOf(QRegularExpression("^\\s+")) == 0) {
        formatFurther = true;
        debFormat.setForeground(DarkGreenAlt);
        setFormat(0, text.size(), debFormat);
    }

    if (formatFurther) {
        /* parentheses and brackets */
        exp.setPattern("\\([^\\(\\)\\[\\]]+\\)|\\[[^\\(\\)\\[\\]]+\\]");
        int index = indx;
        debFormat = neutralFormat;
        debFormat.setFontItalic(true);
        while ((index = text.indexOf(exp, index, &expMatch)) > -1) {
            int ml = expMatch.capturedLength();
            setFormat(index, ml, neutralFormat);
            if (ml > 2) {
                setFormat(index + 1, ml - 2, debFormat);

                QRegularExpression rel("<|>|\\=|~");
                int i = index;
                while ((i = text.indexOf(rel, i)) > -1 && i < index + ml - 1) {
                    QTextCharFormat relFormat;
                    relFormat.setForeground(DarkMagenta);
                    setFormat(i, 1, relFormat);
                    ++i;
                }
            }
            index = index + ml;
        }

        /* non-commented URLs */
        debFormat.setForeground(DarkGreenAlt);
        debFormat.setFontUnderline(true);
        QRegularExpressionMatch urlMatch;
        while ((indx = text.indexOf(urlPattern, indx, &urlMatch)) > -1) {
            setFormat(indx, urlMatch.capturedLength(), debFormat);
            indx += urlMatch.capturedLength();
        }
    }
}
/*************************/
void Highlighter::latexFormula(const QString& text) {
    int index = 0;
    int commentStart = text.indexOf('%');
    QString exp;
    TextBlockData* data = static_cast<TextBlockData*>(currentBlock().userData());
    static const QRegularExpression latexFormulaStart(
        "\\${2}|\\$|\\\\\\(|\\\\\\[|\\\\begin\\s*{math}|\\\\begin\\s*{math\\*}|\\\\begin\\s*{displaymath}|\\\\begin\\s*"
        "{"
        "displaymath\\*}|\\\\begin\\s*{multline}|\\\\begin\\s*{multline\\*}|\\\\begin\\s*{gather}|\\\\begin\\s*{"
        "gather\\*"
        "}|\\\\begin\\s*{cases}|\\\\begin\\s*{cases\\*}|\\\\begin\\s*{alignat}|\\\\begin\\s*{alignat\\*}|\\\\begin\\s*{"
        "xalignat}|\\\\begin\\s*{xalignat\\*}|\\\\begin\\s*{xxalignat}|\\\\begin\\s*{xxalignat\\*}|\\\\begin\\s*{"
        "eqnarray}|\\\\begin\\s*{eqnarray\\*}|\\\\begin\\s*{subeqnarray}|\\\\begin\\s*{subeqnarray\\*}|\\\\begin\\s*{"
        "align}|\\\\begin\\s*{align\\*}|\\\\begin\\s*{flalign}|\\\\begin\\s*{flalign\\*}|\\\\begin\\s*{equation}|"
        "\\\\begin\\s*{equation\\*}|\\\\begin\\s*{verbatim}|\\\\begin\\s*{verbatim\\*}");
    QRegularExpressionMatch startMatch;
    QRegularExpression endExp;
    QRegularExpressionMatch endMatch;

    QTextBlock prevBlock = currentBlock().previous();
    if (prevBlock.isValid()) {
        if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData()))
            exp = prevData->labelInfo();
    }

    if (exp.isEmpty()) {
        index = text.indexOf(latexFormulaStart, index, &startMatch);
        while (isEscapedChar(text, index))
            index = text.indexOf(latexFormulaStart, index + 1, &startMatch);
        /* skip (single-line) comments */
        if (commentStart > -1 && index >= commentStart)
            index = -1;
    }

    while (index >= 0) {
        int endIndex;

        if (!exp.isEmpty() && index == 0) {
            endExp.setPattern(exp);
            endIndex = text.indexOf(endExp, 0, &endMatch);
        }
        else {
            if (startMatch.capturedLength() == 1)
                endExp.setPattern("\\$");
            else if (startMatch.capturedLength() == 2) {
                if (text.at(index + 1) == '$')
                    endExp.setPattern("\\${2}");
                else if (text.at(index + 1) == '(')
                    endExp.setPattern("\\\\\\)");
                else  // if (text.at (index + 1) == '[')
                    endExp.setPattern("\\\\\\]");
            }
            else if (startMatch.capturedLength() > 4)  // the smallest is "math"
            {
                if (text.at(index + startMatch.capturedLength() - 2) == '*')  // ending with "*"
                {
                    if (text.at(index + startMatch.capturedLength() - 3) == 'h') {
                        if (startMatch.capturedLength() > 7 &&
                            text.at(index + startMatch.capturedLength() - 7) == 'y') {
                            endExp.setPattern("\\\\end\\s*{displaymath\\*}");
                        }
                        else
                            endExp.setPattern("\\\\end\\s*{math\\*}");
                    }
                    else if (text.at(index + startMatch.capturedLength() - 3) == 'e')
                        endExp.setPattern("\\\\end\\s*{multline\\*}");
                    else if (text.at(index + startMatch.capturedLength() - 3) == 'r')
                        endExp.setPattern("\\\\end\\s*{gather\\*}");
                    else if (text.at(index + startMatch.capturedLength() - 3) == 's')
                        endExp.setPattern("\\\\end\\s*{cases\\*}");
                    else if (text.at(index + startMatch.capturedLength() - 3) == 't') {
                        if (startMatch.capturedLength() > 10 &&
                            text.at(index + startMatch.capturedLength() - 10) == 'x') {
                            if (startMatch.capturedLength() > 11 &&
                                text.at(index + startMatch.capturedLength() - 11) == 'x') {
                                endExp.setPattern("\\\\end\\s*{xxalignat\\*}");
                            }
                            else
                                endExp.setPattern("\\\\end\\s*{xalignat\\*}");
                        }
                        else
                            endExp.setPattern("\\\\end\\s*{alignat\\*}");
                    }
                    else if (text.at(index + startMatch.capturedLength() - 3) == 'y') {
                        if (startMatch.capturedLength() > 11 &&
                            text.at(index + startMatch.capturedLength() - 11) == 'b') {
                            endExp.setPattern("\\\\end\\s*{subeqnarray\\*}");
                        }
                        else
                            endExp.setPattern("\\\\end\\s*{eqnarray\\*}");
                    }
                    else if (text.at(index + startMatch.capturedLength() - 3) == 'n') {
                        if (text.at(index + startMatch.capturedLength() - 4) == 'g') {
                            if (startMatch.capturedLength() > 9 &&
                                text.at(index + startMatch.capturedLength() - 9) == 'f') {
                                endExp.setPattern("\\\\end\\s*{flalign\\*}");
                            }
                            else
                                endExp.setPattern("\\\\end\\s*{align\\*}");
                        }
                        else
                            endExp.setPattern("\\\\end\\s*{equation\\*}");
                    }
                    else  // 'm'
                        endExp.setPattern("\\\\end\\s*{verbatim\\*}");
                }
                else if (text.at(index + startMatch.capturedLength() - 2) == 'h') {
                    if (startMatch.capturedLength() > 6 && text.at(index + startMatch.capturedLength() - 6) == 'y') {
                        endExp.setPattern("\\\\end\\s*{displaymath}");
                    }
                    else
                        endExp.setPattern("\\\\end\\s*{math}");
                }
                else if (text.at(index + startMatch.capturedLength() - 2) == 'e')
                    endExp.setPattern("\\\\end\\s*{multline}");
                else if (text.at(index + startMatch.capturedLength() - 2) == 'r')
                    endExp.setPattern("\\\\end\\s*{gather}");
                else if (text.at(index + startMatch.capturedLength() - 2) == 's')
                    endExp.setPattern("\\\\end\\s*{cases}");
                else if (text.at(index + startMatch.capturedLength() - 2) == 't') {
                    if (startMatch.capturedLength() > 9 && text.at(index + startMatch.capturedLength() - 9) == 'x') {
                        if (startMatch.capturedLength() > 10 &&
                            text.at(index + startMatch.capturedLength() - 10) == 'x') {
                            endExp.setPattern("\\\\end\\s*{xxalignat}");
                        }
                        else
                            endExp.setPattern("\\\\end\\s*{xalignat}");
                    }
                    else
                        endExp.setPattern("\\\\end\\s*{alignat}");
                }
                else if (text.at(index + startMatch.capturedLength() - 2) == 'y') {
                    if (startMatch.capturedLength() > 10 && text.at(index + startMatch.capturedLength() - 10) == 'b') {
                        endExp.setPattern("\\\\end\\s*{subeqnarray}");
                    }
                    else
                        endExp.setPattern("\\\\end\\s*{eqnarray}");
                }
                else if (text.at(index + startMatch.capturedLength() - 2) == 'n') {
                    if (text.at(index + startMatch.capturedLength() - 3) == 'g') {
                        if (startMatch.capturedLength() > 8 &&
                            text.at(index + startMatch.capturedLength() - 8) == 'f') {
                            endExp.setPattern("\\\\end\\s*{flalign}");
                        }
                        else
                            endExp.setPattern("\\\\end\\s*{align}");
                    }
                    else
                        endExp.setPattern("\\\\end\\s*{equation}");
                }
                else  // 'm'
                    endExp.setPattern("\\\\end\\s*{verbatim}");
            }
            endIndex = text.indexOf(endExp, index + startMatch.capturedLength(), &endMatch);
            /* don't format "\begin{math}" or "\begin{equation}" or... */
            if (startMatch.capturedLength() > 2)
                index += startMatch.capturedLength();
        }

        while (isEscapedChar(text, endIndex))
            endIndex = text.indexOf(endExp, endIndex + 1, &endMatch);

        if (commentStart > -1 && endIndex >= commentStart)
            endIndex = -1;  // comments can be inside formulas

        int formulaLength;
        if (endIndex == -1) {
            if (data)
                data->insertInfo(endExp.pattern());
            formulaLength = (commentStart > -1 ? commentStart : text.length()) - index;
        }
        else {
            formulaLength = endIndex - index +
                            (endMatch.capturedLength() > 2 ? 0  // don't format "\end{math}" or "\end{equation}"
                                                           : endMatch.capturedLength());
        }

        setFormat(index, formulaLength, codeBlockFormat);

        index = text.indexOf(latexFormulaStart, index + formulaLength, &startMatch);
        while (isEscapedChar(text, index))
            index = text.indexOf(latexFormulaStart, index + 1, &startMatch);
        if (commentStart > -1 && index >= commentStart)
            index = -1;
    }
}
}  // namespace Texxy
