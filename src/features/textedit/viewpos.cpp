#include "textedit/textedit_prelude.h"

namespace Texxy {

TextEdit::viewPosition TextEdit::getViewPosition() const {
    viewPosition vPos;

    const QWidget* vp = viewport();
    if (!vp)
        return vPos;

    const QRect vr = vp->rect();

    auto cursorAtX = [&](int x, int y) {
        const QTextCursor c = cursorForPosition(QPoint(x, y));
        if (c.block().textDirection() == Qt::RightToLeft)
            return cursorForPosition(QPoint(vr.right() + 1, y));
        return c;
    };

    // compute a reasonable y to probe the top line center
    const int lineH = QFontMetricsF(document()->defaultFont()).lineSpacing();
    const int probeTopY = vr.top() + lineH / 2;

    // top cursor immediately below the top edge of the viewport
    QTextCursor topCur = cursorAtX(vr.left(), probeTopY);
    QRect topRect = cursorRect(topCur);
    const int top = topRect.top();
    vPos.topPos = topCur.position();

    // current cursor if it is fully inside the viewport rect
    const int curPosition = textCursor().position();
    if (curPosition >= vPos.topPos) {
        const QRect cRect = cursorRect();
        if (cRect.bottom() <= vr.bottom() && cRect.left() >= vr.left() && cRect.right() <= vr.right())
            vPos.curPos = curPosition;
    }

    // bottom cursor immediately above the bottom edge of the viewport
    QTextCursor bottomCur = cursorAtX(vr.left(), vr.bottom());
    QRect bRect = cursorRect(bottomCur);
    int bottom = bRect.bottom();

    // walk upward while the measured bottom extends past the viewport
    // guard against pathological layouts by limiting iterations
    {
        QTextCursor tmp = bottomCur;
        int guard = 2048;
        while (!bottomCur.atStart() && bottom > vr.bottom() && guard-- > 0) {
            tmp.setPosition(std::max(bottomCur.position() - 1, 0));
            const QRect tmpRect = cursorRect(tmp);
            const QTextCursor reProbe = cursorAtX(vr.left(), tmpRect.center().y());
            if (tmpRect.bottom() >= bottom || tmpRect.top() < vr.top())
                break;
            bottomCur.setPosition(reProbe.position());
            bottom = tmpRect.bottom();
        }
    }
    vPos.bottomPos = bottomCur.position();

    // middle cursor centered between top and bottom
    int midY = (top + bottom + 1) / 2;
    // at the top of the document adjust for content offset
    if (topCur.position() == 0)
        midY -= static_cast<int>(contentOffset().y());

    QTextCursor midCur = cursorAtX(vr.left(), midY);
    QRect midRect = cursorRect(midCur);
    if (midRect.top() >= midY) {
        midCur.setPosition(std::max(midCur.position() - 1, 0));
        const QRect tmpRect = cursorRect(midCur);
        midCur = cursorAtX(vr.left(), tmpRect.center().y());
    }
    vPos.midPos = midCur.position();

    return vPos;
}

/*************************/
void TextEdit::setViewPostion(const viewPosition vPos) {
    // keep API spelling but apply robust clamping and centering
    QTextCursor cur = textCursor();
    cur.movePosition(QTextCursor::End);
    const int endPos = cur.position();

    if (vPos.midPos < 0) {
        if (vPos.curPos >= 0) {
            cur.setPosition(std::min(vPos.curPos, endPos));
            setTextCursor(cur);
        }
        return;
    }

    // first center the middle cursor
    cur.setPosition(std::min(vPos.midPos, endPos));
    setTextCursor(cur);
    centerCursor();

    // if the middle cursor is not at the start of the visual line, text layout likely changed
    if (const QWidget* vp = viewport()) {
        const QRect vr = vp->rect();
        const QRect cRect = cursorRect(cur);

        auto reProbeAtLineStart = [&]() {
            if (cur.block().textDirection() == Qt::RightToLeft)
                return cursorForPosition(QPoint(vr.right() + 1, cRect.center().y()));
            return cursorForPosition(QPoint(vr.left(), cRect.center().y()));
        };

        QTextCursor tmp = reProbeAtLineStart();
        if (tmp != cur) {
            setTextCursor(tmp);
            centerCursor();
            if (vPos.curPos >= 0) {
                tmp.setPosition(std::min(vPos.curPos, endPos));
                setTextCursor(tmp);
            }
            return;
        }
    }

    // ensure that the top and bottom cursors are visible if provided
    {
        QTextCursor tmp = cur;
        if (vPos.topPos >= 0) {
            tmp.setPosition(std::min(vPos.topPos, endPos));
            setTextCursor(tmp);
        }
        if (vPos.bottomPos >= 0) {
            tmp.setPosition(std::min(vPos.bottomPos, endPos));
            setTextCursor(tmp);
        }
    }

    // restore original cursor if visible, else keep middle
    if (vPos.curPos >= 0)
        cur.setPosition(std::min(vPos.curPos, endPos));
    setTextCursor(cur);
}

}  // namespace Texxy
