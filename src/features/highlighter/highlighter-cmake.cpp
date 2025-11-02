// src/features/highlighter/highlighter-cmake.cpp
#include "highlighter.h"

namespace Texxy {

static const QRegularExpression cmakeBracketStart("\\[\\=*\\[");
static const QRegularExpression cmakeBracketEnd("\\]\\]");

bool Highlighter::isCmakeDoubleBracketed(const QString& text, const int index, const int start) {
    if (index < 0 || start < 0 || index < start)
        return false;

    bool res = false;
    int pos = start - 1;
    int bracketLength = 0;
    int N;
    QTextBlock prevBlock = currentBlock().previous();
    QRegularExpressionMatch commentMatch;
    QRegularExpression commentExpression;
    if (pos >= 0 || previousBlockState() != commentState) {
        N = 0;
        commentExpression = cmakeBracketStart;
    }
    else {
        N = 1;
        res = true;
        if (prevBlock.isValid()) {
            if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData()))
                bracketLength = prevData->openNests();
        }
        if (bracketLength > 0)
            commentExpression.setPattern("\\]\\={" + QString::number(bracketLength) + "}\\]");
        else
            commentExpression = cmakeBracketEnd;
    }

    while ((pos = text.indexOf(commentExpression, pos + 1, &commentMatch)) >= 0) {
        QTextCharFormat fi = format(pos);
        if (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat) {
            continue;
        }

        ++N;

        if (index < pos + (N % 2 == 0 ? commentMatch.capturedLength() : 0)) {
            if (N % 2 == 0)
                res = true;
            else
                res = false;
            break;
        }

        if (N % 2 != 0) {
            bracketLength = commentMatch.capturedLength() - 2;
            if (bracketLength > 0)
                commentExpression.setPattern("\\]\\={" + QString::number(bracketLength) + "}\\]");
            else
                commentExpression = cmakeBracketEnd;
            res = true;
        }
        else {
            commentExpression = cmakeBracketStart;
            res = false;
        }
    }

    return res;
}
/*************************/
bool Highlighter::cmakeDoubleBrackets(const QString& text, int oldBracketLength, bool wasComment) {
    bool rehighlightNextBlock = false;

    int startIndex = 0;
    int prevState = previousBlockState();
    int bracketLength = 0;
    bool isComment = false;

    QRegularExpression commentEndExp;
    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;

    if (prevState != commentState) {
        startIndex = text.indexOf(cmakeBracketStart, 0, &startMatch);
        QTextCharFormat fi = format(startIndex);
        while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat) {
            startIndex = text.indexOf(cmakeBracketStart, startIndex + 1, &startMatch);
            fi = format(startIndex);
        }
        /* skip single-line comments */
        if (format(startIndex) == commentFormat || format(startIndex) == urlFormat)
            startIndex = -1;

        if (startIndex >= 0) {
            isComment = (startIndex > 0 && text.at(startIndex - 1) == '#');
            bracketLength = startMatch.capturedLength() - 2;
            if (bracketLength > 0)
                commentEndExp.setPattern("\\]\\={" + QString::number(bracketLength) + "}\\]");
            else
                commentEndExp = cmakeBracketEnd;
        }
    }
    else {
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid()) {
            if (TextBlockData* prevData = static_cast<TextBlockData*>(prevBlock.userData())) {
                bracketLength = prevData->openNests();
                isComment = prevData->getProperty();
            }
        }
        if (bracketLength > 0)
            commentEndExp.setPattern("\\]\\={" + QString::number(bracketLength) + "}\\]");
        else
            commentEndExp = cmakeBracketEnd;
    }

    while (startIndex >= 0) {
        int endIndex;
        if (startIndex == 0 && prevState == commentState)
            endIndex = text.indexOf(commentEndExp, 0, &endMatch);
        else
            endIndex = text.indexOf(commentEndExp, startIndex + startMatch.capturedLength(), &endMatch);

        if (endIndex >= 0) {
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
            if (hadSingleLineComment)
                multiLineQuote(text, i);
        }

        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(commentState);
            commentLength = text.length() - startIndex;
            TextBlockData* curData = static_cast<TextBlockData*>(currentBlock().userData());
            if (curData) {
                curData->insertNestInfo(bracketLength);
                if (isComment)
                    curData->setProperty(true);
                setCurrentBlockUserData(curData);
                if (curData->lastState() == commentState &&
                    (oldBracketLength != bracketLength || wasComment != isComment)) {
                    rehighlightNextBlock = true;
                }
            }
        }
        else
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        if (isComment) {
            if (startIndex == 0 && prevState == commentState)
                setFormat(startIndex, commentLength, commentFormat);
            else  // also format the starting "#"
                setFormat(startIndex - 1, commentLength + 1, commentFormat);

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
        }
        else  // regexFormat isn't used elsewhere with cmake
            setFormat(startIndex, commentLength, regexFormat);

        startIndex = text.indexOf(cmakeBracketStart, startIndex + commentLength, &startMatch);

        QTextCharFormat fi = format(startIndex);
        while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat) {
            startIndex = text.indexOf(cmakeBracketStart, startIndex + 1, &startMatch);
            fi = format(startIndex);
        }
        if (format(startIndex) == commentFormat || format(startIndex) == urlFormat)
            startIndex = -1;

        if (startIndex >= 0) {
            isComment = (startIndex > 0 && text.at(startIndex - 1) == '#');
            bracketLength = startMatch.capturedLength() - 2;
            if (bracketLength > 0)
                commentEndExp.setPattern("\\]\\={" + QString::number(bracketLength) + "}\\]");
            else
                commentEndExp = cmakeBracketEnd;
        }
    }

    return rehighlightNextBlock;
}

}  // namespace Texxy
