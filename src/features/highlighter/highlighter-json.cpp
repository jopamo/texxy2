// src/features/highlighter/highlighter-json.cpp

#include "highlighter.h"

namespace Texxy {

static const QRegularExpression startBraceOrBracket("\\{|\\[");

void Highlighter::jsonKey(const QString& text,
                          const int start,
                          int& K,
                          int& V,
                          int& B,
                          bool& insideValue,
                          QString& braces) {
    static const QRegularExpression jsonKeyExp("\\{|\\}|:|,|\"");
    int index = text.indexOf(jsonKeyExp, start);
    if (index >= 0) {
        setFormat(start, index - start, errorFormat);
        if (text.at(index) == '{') {
            ++K;
            braces += "{";
            insideValue = false;
            jsonKey(text, index + 1, K, V, B, insideValue, braces);
        }
        else if (text.at(index) == '}') {
            if (K > 0) {
                --K;
                braces.chop(1);
                if (K == 0)
                    V = 0;
                insideValue = V > 0;
            }
            if (K == 0) {  // search for a starting brace or bracket
                ++index;
                int indx = text.indexOf(startBraceOrBracket, index);
                if (indx > -1) {
                    setFormat(index, indx - index, errorFormat);
                    ++K;
                    if (text.at(indx) == '{') {
                        braces += "{";
                        jsonKey(text, indx + 1, K, V, B, insideValue, braces);
                    }
                    else {  // consider a virtual key and serach in the value
                        ++V;
                        insideValue = true;
                        ++B;
                        braces += "v{[";
                        jsonValue(text, indx + 1, K, V, B, insideValue, braces);
                    }
                }
                else
                    setFormat(index, text.length() - index, errorFormat);
            }
            else if (insideValue) {
                jsonValue(text, index + 1, K, V, B, insideValue, braces);
            }
            else {
                jsonKey(text, index + 1, K, V, B, insideValue, braces);
            }
        }
        else if (text.at(index) == ':') {
            ++V;
            insideValue = true;
            jsonValue(text, index + 1, K, V, B, insideValue, braces);
        }
        else if (text.at(index) == ',') {
            jsonKey(text, index + 1, K, V, B, insideValue, braces);
        }
        else  // double quote
        {
            int end = text.indexOf(quoteMark, index + 1);
            while (isEscapedChar(text, end))
                end = text.indexOf(quoteMark, end + 1);
            if (end >= 0) {
                setFormat(index, end + 1 - index, quoteFormat);
                jsonKey(text, end + 1, K, V, B, insideValue, braces);
            }
            else {
                setFormat(index, text.length() - index, quoteFormat);
                setCurrentBlockState(doubleQuoteState);
            }
        }
    }
    else
        setFormat(start, text.length() - start, errorFormat);
}
/*************************/
void Highlighter::jsonValue(const QString& text,
                            const int start,
                            int& K,
                            int& V,
                            int& B,
                            bool& insideValue,
                            QString& braces) {
    static const QRegularExpression jasonNumExp(
        "(?<=^|[^\\w\\d\\.])(\\+|-)?((\\d*\\.?\\d+|\\d+\\.)((e|E)(\\+|-)?\\d+)?)(?=[^\\w\\d\\.]|$)");
    static const QRegularExpression jsonValueExp("\\{|\\}|,|\"|\\[|\\]|\\b(true|false|null)\\b|" +
                                                 jasonNumExp.pattern());
    QRegularExpressionMatch match;
    int index = text.indexOf(jsonValueExp, start, &match);
    if (index >= 0) {
        setFormat(start, index - start, errorFormat);
        if (text.at(index) == '{') {
            ++K;
            braces += "{";
            insideValue = false;
            jsonKey(text, index + 1, K, V, B, insideValue, braces);
        }
        else if (text.at(index) == '}') {
            if (K > 0) {
                --K;
                braces.chop(1);
                if (K == 0)
                    V = 0;
                insideValue = V > 0;
            }
            if (K == 0) {  // search for a starting brace or bracket
                ++index;
                int indx = text.indexOf(startBraceOrBracket, index);
                if (indx > -1) {
                    setFormat(index, indx - index, errorFormat);
                    ++K;
                    if (text.at(indx) == '{') {
                        braces += "{";
                        jsonKey(text, indx + 1, K, V, B, insideValue, braces);
                    }
                    else {  // consider a virtual key and serach in the value
                        ++V;
                        insideValue = true;
                        ++B;
                        braces += "v{[";
                        jsonValue(text, indx + 1, K, V, B, insideValue, braces);
                    }
                }
                else
                    setFormat(index, text.length() - index, errorFormat);
            }
            else if (insideValue) {
                jsonValue(text, index + 1, K, V, B, insideValue, braces);
            }
            else {
                jsonKey(text, index + 1, K, V, B, insideValue, braces);
            }
        }
        else if (text.at(index) == '[') {
            braces += "[";
            ++B;
            jsonValue(text, index + 1, K, V, B, insideValue, braces);
        }
        else if (text.at(index) == ']') {
            if (B > 0) {
                --B;
                braces.chop(1);
            }
            if (B == 0 && braces.size() > 1 && braces.at(braces.size() - 1) == '{' &&
                braces.at(braces.size() - 2) == 'v') {  // we had considered a virtual key
                K = 0;
                V = 0;
                insideValue = false;
                braces.clear();
                ++index;
                int indx = text.indexOf(startBraceOrBracket, index);
                if (indx > -1) {
                    setFormat(index, indx - index, errorFormat);
                    ++K;
                    if (text.at(indx) == '{') {
                        braces += "{";
                        jsonKey(text, indx + 1, K, V, B, insideValue, braces);
                    }
                    else {  // consider a virtual key again and serach in the value
                        ++V;
                        insideValue = true;
                        ++B;
                        braces += "v{[";
                        jsonValue(text, indx + 1, K, V, B, insideValue, braces);
                    }
                }
                else
                    setFormat(index, text.length() - index, errorFormat);
            }
            else {
                jsonValue(text, index + 1, K, V, B, insideValue, braces);
            }
        }
        else if (text.at(index) == ',') {
            if (V > 0)  // inside a value and...
            {
                if (B == 0 || (!braces.isEmpty() &&
                               braces.at(braces.size() - 1) ==
                                   '{')) {  // ... either outside all brackets or immediately inside a pair of braces
                    --V;
                    insideValue = false;
                    jsonKey(text, index + 1, K, V, B, insideValue, braces);
                }
                else {
                    jsonValue(text, index + 1, K, V, B, insideValue, braces);
                }
            }
            else {
                jsonKey(text, index + 1, K, V, B, insideValue, braces);
            }
        }
        else if ((text.at(index) == '.' || text.at(index) == '-' || text.at(index) == '+' ||
                  text.at(index).isNumber()) &&
                 text.indexOf(jasonNumExp, index) == index) {
            QTextCharFormat numFormat;
            numFormat.setForeground(Brown);
            numFormat.setFontItalic(true);
            setFormat(index, match.capturedLength(), numFormat);
            jsonValue(text, index + match.capturedLength(), K, V, B, insideValue, braces);
        }
        else if (match.capturedLength() > 1)  // keywords
        {
            QTextCharFormat keywordFormat;
            keywordFormat.setForeground(DarkBlue);
            keywordFormat.setFontWeight(QFont::Bold);
            setFormat(index, match.capturedLength(), keywordFormat);
            jsonValue(text, index + match.capturedLength(), K, V, B, insideValue, braces);
        }
        else  // double quote
        {
            int end = text.indexOf(quoteMark, index + 1);
            while (isEscapedChar(text, end))
                end = text.indexOf(quoteMark, end + 1);
            if (end >= 0) {
                setFormat(index, end + 1 - index, regexFormat);
                jsonValue(text, end + 1, K, V, B, insideValue, braces);
            }
            else {
                setFormat(index, text.length() - index, regexFormat);
                setCurrentBlockState(regexState);
            }
        }
    }
    else
        setFormat(start, text.length() - start, errorFormat);
}
/*************************/
void Highlighter::highlightJsonBlock(const QString& text) {
    TextBlockData* data = new TextBlockData;

    int txtL = text.length();
    if (txtL > 30000
        /* don't highlight the rest of the document
           (endState isn't used anywhere else) */
        || previousBlockState() == endState) {
        setCurrentBlockState(endState);
        setFormat(0, txtL, translucentFormat);
        data->setHighlighted();
        setCurrentBlockUserData(data);
        return;
    }

    int bn = currentBlock().blockNumber();
    bool mainFormatting(bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber());
    if (mainFormatting)
        setFormat(0, txtL, mainFormat);

    /* NOTE:
       We try to use the available variables of TextBlockData for Json.
       The meanings of the used variables change as follows:

       TextBlockData::OpenNests          -> Nested keys
       TextBlockData::LastFormattedRegex -> Nested values
       TextBlockData::lastFormattedQuote -> Nested brackets (inside values)
       TextBlockData::Property           -> Locally inside a value (not a key inside a value)
       TextBlockData::label              -> Keeps track of the order of open braces and brackets
    */

    int index = 0;
    bool rehighlightNextBlock = false;

    int oldK = 0, oldV = 0, oldB = 0;
    bool oldInsideValue = false;
    QString oldBraces;
    if (TextBlockData* oldData = static_cast<TextBlockData*>(currentBlockUserData())) {
        oldK = oldData->openNests();
        if (oldK > 0) {
            oldV = oldData->lastFormattedRegex();
            if (oldV > 0) {
                oldInsideValue = oldData->getProperty();
                oldB = oldData->lastFormattedQuote();
                oldBraces = oldData->labelInfo();
            }
        }
    }

    data->setLastState(currentBlockState());
    setCurrentBlockState(0);

    int K = 0, V = 0, B = 0;
    bool insideValue = false;
    QString braces;

    QTextBlock prevBlock = currentBlock().previous();
    if (prevBlock.isValid()) {
        if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData())) {
            K = prevData->openNests();
            if (K > 0) {
                V = prevData->lastFormattedRegex();
                if (V > 0) {
                    insideValue = prevData->getProperty();
                    B = prevData->lastFormattedQuote();
                    braces = prevData->labelInfo();
                }
            }
        }
    }

    if (K > 0) {
        if (insideValue)  // the value continues from the previous line
        {
            if (previousBlockState() == regexState) {
                index = text.indexOf(quoteMark);
                while (isEscapedChar(text, index))
                    index = text.indexOf(quoteMark, index + 1);
                if (index < 0) {
                    setFormat(0, text.length(), regexFormat);
                    setCurrentBlockState(regexState);
                }
                else {
                    ++index;
                    setFormat(0, index, regexFormat);
                }
            }
            if (index > -1) {
                jsonValue(text, index, K, V, B, insideValue, braces);
            }
        }
        else  // the key continues from the previous line
        {
            if (previousBlockState() == doubleQuoteState) {
                index = text.indexOf(quoteMark);
                while (isEscapedChar(text, index))
                    index = text.indexOf(quoteMark, index + 1);
                if (index < 0) {
                    setFormat(0, text.length(), quoteFormat);
                    setCurrentBlockState(doubleQuoteState);
                }
                else {
                    ++index;
                    setFormat(0, index, quoteFormat);
                }
            }
            if (index > -1) {
                jsonKey(text, index, K, V, B, insideValue, braces);
            }
        }
    }
    else {  // search for a starting brace or bracket
        index = text.indexOf(startBraceOrBracket);
        if (index > -1) {
            setFormat(0, index, errorFormat);
            ++K;
            if (text.at(index) == '{') {
                braces += "{";
                jsonKey(text, index + 1, K, V, B, insideValue, braces);
            }
            else {  // consider a virtual key and serach in the value
                ++V;
                insideValue = true;
                ++B;
                braces += "v{[";
                jsonValue(text, index + 1, K, V, B, insideValue, braces);
            }
        }
        else
            setFormat(0, text.length(), errorFormat);
    }

    data->insertNestInfo(K);            // open keys
    data->insertLastFormattedRegex(V);  // open values
    data->insertLastFormattedQuote(B);  // open brackets (inside values)
    data->setProperty(insideValue);     // locally inside a value
    data->insertInfo(braces);           // the order of open braces and brackets

    /* this is much faster than comparing old and new braces and
       rehighlighting the next block, especially with text editing */
    if (currentBlockState() == 0 && !braces.isEmpty()) {
        int n = static_cast<int>(qHash(braces));
        int state = 2 * (n + (n >= 0 ? endState / 2 + 1 : 0));  // always even and not endState
        setCurrentBlockState(state);
    }

    if (currentBlockState() == data->lastState() &&
        (K != oldK || V != oldV || B != oldB || insideValue != oldInsideValue || braces != oldBraces)) {
        rehighlightNextBlock = true;
    }

    /* also, format whitespaces */
    if (mainFormatting) {
        data->setHighlighted();
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
        for (const HighlightingRule& rule : std::as_const(highlightingRules))
#else
        for (const HighlightingRule& rule : qAsConst(highlightingRules))
#endif
        {
            if (rule.format == whiteSpaceFormat) {
                QRegularExpressionMatch match;
                index = text.indexOf(rule.pattern, 0, &match);
                while (index >= 0) {
                    setFormat(index, match.capturedLength(), rule.format);
                    index = text.indexOf(rule.pattern, index + match.capturedLength(), &match);
                }
                break;
            }
        }
    }

    QTextCharFormat fi;

    /* left brace */
    index = text.indexOf('{');
    fi = format(index);
    while (index >= 0 && (fi == quoteFormat || fi == regexFormat)) {
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
        while (index >= 0 && (fi == quoteFormat || fi == regexFormat)) {
            index = text.indexOf('{', index + 1);
            fi = format(index);
        }
    }

    /* right brace */
    index = text.indexOf('}');
    fi = format(index);
    while (index >= 0 && (fi == quoteFormat || fi == regexFormat)) {
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
        while (index >= 0 && (fi == quoteFormat || fi == regexFormat)) {
            index = text.indexOf('}', index + 1);
            fi = format(index);
        }
    }

    /* left bracket */
    index = text.indexOf('[');
    fi = format(index);
    while (index >= 0 && (fi == quoteFormat || fi == regexFormat)) {
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
        while (index >= 0 && (fi == quoteFormat || fi == regexFormat)) {
            index = text.indexOf('[', index + 1);
            fi = format(index);
        }
    }

    /* right bracket */
    index = text.indexOf(']');
    fi = format(index);
    while (index >= 0 && (fi == quoteFormat || fi == regexFormat)) {
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
        while (index >= 0 && (fi == quoteFormat || fi == regexFormat)) {
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
