// src/features/textedit/helpers.cpp
#include "textedit/textedit_prelude.h"

namespace Texxy {

/*************************/
void TextEdit::zooming(float range) {
    // forget saved horizontal cursor position
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;

    const QFont currentFont = document()->defaultFont();

    if (qFuzzyCompare(range, 0.0f)) {
        // Reset to default editor font
        setEditorFont(font_, false);

        // If we zoomed out below original size, emit signal
        if (font_.pointSizeF() < currentFont.pointSizeF()) {
            emit zoomedOut(this);
        }
    }
    else {
        const float newSize = currentFont.pointSizeF() + range;
        if (!(newSize > 0.0f)) {
            return;  // invalid size, ignore
        }

        QFont zoomFont = currentFont;
        zoomFont.setPointSizeF(static_cast<double>(newSize));
        setEditorFont(zoomFont, false);

        if (range < 0.0f) {
            emit zoomedOut(this);
        }
    }

    // Workaround for Qt bug: ensure scrollbar ranges update
    adjustScrollbars();
}

/*************************/
static const QRegularExpression urlOrEmailPattern(
    QStringLiteral(
        R"((?:                                       # non-capturing group for either URL or email
              (?:[a-z0-9.+-]+)://                   # scheme
              (?:[^\s:@/]+(?::[^\s:@/]*)?@)?        # optional userinfo@
              (?:
                 (?:0|25[0-5]|2[0-4]\d|[01]?\d?\d)   # IPv4 octet
                 (?:\.(?:0|25[0-5]|2[0-4]\d|[01]?\d?\d)){3}
               | \[[0-9A-Fa-f:.]+\]                  # IPv6/IPv4 literal
               | (?:[a-z0-9-]+\.)+[a-z0-9-]+        # hostname
               | localhost
              )
              (?::\d{1,5})?                         # optional port
              (?:[/?#][^\s]*)?                     # optional resource
         )
       |                             # or
         (?:                                      # email pattern
            (?:[-!#$%&'*+/=?^_`{}|~0-9A-Z]+
               (?:\.[-!#$%&'*+/=?^_`{}|~0-9A-Z]+)*)
          | "(?:[\001-\010\013\014\016-\037!#-\[\]-\177]"
               | \\[\001-\011\013\014\016-\177])*"
         )
         @
         (?:(?:[a-z0-9-]+\.)+[a-z0-9-]+|localhost|\[[0-9A-Fa-f:.]+\])
        )"),
    QRegularExpression::CaseInsensitiveOption | QRegularExpression::ExtendedPatternSyntaxOption);

QString TextEdit::getUrl(int pos) const {
    QString result;
    const QTextBlock block = document()->findBlock(pos);
    const QString text = block.text();
    constexpr qsizetype MaxTextSize = 30000;

    if (static_cast<qsizetype>(text.size()) <= MaxTextSize) {
        const qsizetype localPos = static_cast<qsizetype>(pos) - block.position();
        if (localPos >= 0 && localPos < text.size()) {
            const QRegularExpressionMatch match = urlOrEmailPattern.match(text, localPos);
            if (match.hasMatch()) {
                result = match.captured(0);
                // treat as email if contains '@' but doesn’t look like scheme
                if (result.contains(QLatin1Char('@')) && !result.startsWith(QLatin1String("http"))) {
                    result.prepend(QStringLiteral("mailto:"));
                }
            }
        }
    }

    return result;
}

}  // namespace Texxy
