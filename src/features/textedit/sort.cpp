#include "textedit/textedit_prelude.h"

#include <QCollator>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <unordered_set>

namespace Texxy {

static QCollator makeCollator() {
    QCollator coll;
    coll.setLocale(QLocale());  // system locale
    coll.setNumericMode(true);
    coll.setCaseSensitivity(Qt::CaseSensitive);
    coll.setIgnorePunctuation(false);
    return coll;
}

void TextEdit::sortLines(bool reverse) {
    if (isReadOnly())
        return;

    QTextCursor cursor = textCursor();
    const QString sel = cursor.selectedText();
    if (!sel.contains(QChar(QChar::ParagraphSeparator)))
        return;

    const int a = cursor.anchor();
    const int p = cursor.position();

    cursor.beginEditBlock();
    cursor.setPosition(std::min(a, p));
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.setPosition(std::max(a, p), QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    QStringList lines = cursor.selectedText().split(QChar(QChar::ParagraphSeparator), Qt::KeepEmptyParts);

    QCollator coll = makeCollator();
    std::stable_sort(lines.begin(), lines.end(), [&](const QString& lhs, const QString& rhs) {
        const int cmp = coll.compare(lhs, rhs);
        return reverse ? (cmp > 0) : (cmp < 0);
    });

    cursor.removeSelectedText();
    const int startPos = cursor.position();
    for (int i = 0; i < lines.size(); ++i) {
        cursor.insertText(lines.at(i));
        if (i + 1 < lines.size())
            cursor.insertBlock();
    }
    const int endPos = cursor.position();

    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    cursor.endEditBlock();
}

void TextEdit::rmDupeSort(bool reverse) {
    if (isReadOnly())
        return;

    QTextCursor cursor = textCursor();
    const QString sel = cursor.selectedText();
    if (!sel.contains(QChar(QChar::ParagraphSeparator)))
        return;

    const int a = cursor.anchor();
    const int p = cursor.position();

    cursor.beginEditBlock();
    cursor.setPosition(std::min(a, p));
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.setPosition(std::max(a, p), QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    QStringList lines = cursor.selectedText().split(QChar(QChar::ParagraphSeparator), Qt::SkipEmptyParts);

    for (auto& ln : lines)
        ln = ln.trimmed();

    QCollator coll = makeCollator();
    std::stable_sort(lines.begin(), lines.end(), [&](const QString& lhs, const QString& rhs) {
        const int cmp = coll.compare(lhs, rhs);
        return reverse ? (cmp > 0) : (cmp < 0);
    });

    auto newEnd = std::unique(lines.begin(), lines.end(),
                              [&](const QString& lhs, const QString& rhs) { return coll.compare(lhs, rhs) == 0; });
    lines.erase(newEnd, lines.end());

    cursor.removeSelectedText();
    const int startPos = cursor.position();
    for (int i = 0; i < lines.size(); ++i) {
        cursor.insertText(lines.at(i));
        if (i + 1 < lines.size())
            cursor.insertBlock();
    }
    const int endPos = cursor.position();

    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    cursor.endEditBlock();
}

void TextEdit::spaceDupeSort(bool reverse) {
    if (isReadOnly())
        return;

    QTextCursor cursor = textCursor();
    if (cursor.anchor() == cursor.position())
        return;

    cursor.beginEditBlock();

    QString raw = cursor.selectedText();
    cursor.removeSelectedText();

    raw.replace(QChar(QChar::ParagraphSeparator), QLatin1Char(' '));
    raw.replace(QChar::CarriageReturn, QLatin1Char(' '));
    raw.replace(QChar::LineFeed, QLatin1Char(' '));

    QStringList tokensList = raw.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    std::unordered_set<QString> uniq(tokensList.begin(), tokensList.end());

    QStringList tokens;
    tokens.reserve(static_cast<int>(uniq.size()));
    for (const auto& tk : uniq)
        tokens.append(tk);

    QCollator coll = makeCollator();
    std::stable_sort(tokens.begin(), tokens.end(), [&](const QString& lhs, const QString& rhs) {
        const int cmp = coll.compare(lhs, rhs);
        return reverse ? (cmp > 0) : (cmp < 0);
    });

    const int startPos = cursor.position();
    cursor.insertText(tokens.join(QLatin1Char(' ')));
    const int endPos = cursor.position();

    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);
    setTextCursor(cursor);

    cursor.endEditBlock();
}

}  // namespace Texxy
