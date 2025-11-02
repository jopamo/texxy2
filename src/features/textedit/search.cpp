// src/features/textedit/search.cpp
#include "textedit/textedit_prelude.h"

namespace {

QRegularExpression buildRegex(const QString& pattern, QTextDocument::FindFlags flags) {
    QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
    if (!(flags & QTextDocument::FindCaseSensitively))
        opts |= QRegularExpression::CaseInsensitiveOption;
    return QRegularExpression(pattern, opts);
}

bool exceedsLimit(const QTextCursor& cursor, int limit, bool backward) {
    if (limit <= 0 || cursor.isNull())
        return false;

    return backward ? (cursor.anchor() < limit) : (cursor.selectionEnd() > limit);
}

}  // namespace

namespace Texxy {

QTextCursor TextEdit::finding(const QString& str,
                              const QTextCursor& start,
                              QTextDocument::FindFlags flags,
                              bool useRegex,
                              int end) const {
    if (str.isEmpty() || !document())
        return QTextCursor();

    QTextCursor cursor = start;
    if (cursor.isNull()) {
        cursor = textCursor();
        if (cursor.isNull())
            cursor = QTextCursor(document());
    }

    QTextCursor result;
    if (useRegex) {
        QRegularExpression regex = buildRegex(str, flags);
        if (!regex.isValid())
            return QTextCursor();
        // Strip flags not supported by regex overloaded find
        const QTextDocument::FindFlags effectiveFlags =
            flags & (QTextDocument::FindBackward | QTextDocument::FindCaseSensitively);

        result = document()->find(regex, cursor, effectiveFlags);
    }
    else {
        result = document()->find(str, cursor, flags);
    }

    if (result.isNull())
        return QTextCursor();

    const bool backward = (flags & QTextDocument::FindBackward) != 0;
    if (exceedsLimit(result, end, backward))
        return QTextCursor();

    return result;
}

}  // namespace Texxy
