#include "textedit.h"

#include <QRegularExpression>
#include <QStringList>
#include <QSet>

#include <algorithm>

namespace Texxy {

void TextEdit::sortLines(bool reverse) {
    if (isReadOnly())
        return;

    QTextCursor cursor = textCursor();
    if (!cursor.selectedText().contains(QChar(QChar::ParagraphSeparator)))
        return;

    const int anchorPos = cursor.anchor();
    const int curPos = cursor.position();

    cursor.beginEditBlock();

    cursor.setPosition(std::min(anchorPos, curPos));
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.setPosition(std::max(anchorPos, curPos), QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    QStringList lines = cursor.selectedText().split(QChar(QChar::ParagraphSeparator));

    std::sort(lines.begin(), lines.end(),
              [](const QString& a, const QString& b) { return QString::localeAwareCompare(a, b) < 0; });

    if (reverse)
        std::reverse(lines.begin(), lines.end());

    cursor.removeSelectedText();
    for (int i = 0; i < lines.size(); ++i) {
        cursor.insertText(lines.at(i));
        if (i < lines.size() - 1)
            cursor.insertBlock();
    }

    cursor.endEditBlock();
}

void TextEdit::rmDupeSort(bool reverse) {
    if (isReadOnly())
        return;

    QTextCursor cursor = textCursor();
    if (!cursor.selectedText().contains(QChar(QChar::ParagraphSeparator)))
        return;

    const int anchorPos = cursor.anchor();
    const int curPos = cursor.position();

    cursor.beginEditBlock();

    cursor.setPosition(std::min(anchorPos, curPos));
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.setPosition(std::max(anchorPos, curPos), QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    QStringList lines = cursor.selectedText().split(QChar(QChar::ParagraphSeparator));

    for (QString& line : lines)
        line = line.trimmed();

    std::sort(lines.begin(), lines.end(),
              [](const QString& a, const QString& b) { return QString::localeAwareCompare(a, b) < 0; });

    lines.erase(std::unique(lines.begin(), lines.end()), lines.end());

    if (reverse)
        std::reverse(lines.begin(), lines.end());

    cursor.removeSelectedText();
    for (int i = 0; i < lines.size(); ++i) {
        cursor.insertText(lines.at(i));
        if (i < lines.size() - 1)
            cursor.insertBlock();
    }

    cursor.endEditBlock();
}

void TextEdit::spaceDupeSort(bool reverse) {
    if (isReadOnly())
        return;

    QTextCursor cursor = textCursor();
    if (cursor.anchor() == cursor.position())
        return;

    cursor.beginEditBlock();

    QString rawSelection = cursor.selectedText();
    cursor.removeSelectedText();

    rawSelection.replace(QChar(QChar::ParagraphSeparator), QLatin1Char(' '));
    rawSelection.replace(QChar::CarriageReturn, QLatin1Char(' '));
    rawSelection.replace(QChar::LineFeed, QLatin1Char(' '));

    QStringList tokens = rawSelection.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    QSet<QString> uniqueSet;
    for (const QString& tk : tokens)
        uniqueSet.insert(tk);
    tokens = QStringList(uniqueSet.cbegin(), uniqueSet.cend());

    std::sort(tokens.begin(), tokens.end(),
              [](const QString& a, const QString& b) { return QString::localeAwareCompare(a, b) < 0; });

    if (reverse)
        std::reverse(tokens.begin(), tokens.end());

    const QString singleLine = tokens.join(QLatin1Char(' '));
    cursor.insertText(singleLine);

    cursor.endEditBlock();
}

}  // namespace Texxy
