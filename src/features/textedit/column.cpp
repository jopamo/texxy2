// src/features/textedit/column.cpp
#include "textedit/textedit_prelude.h"

namespace Texxy {

// small helpers local to this translation unit
namespace {
// try to move to end of line, but if the cursor would land past the last visible glyph,
// step back one char so columnNumber stays meaningful on wrapped lines
auto moveToVisibleEOL = [](QTextCursor& c) {
    const int origCol = c.columnNumber();
    if (c.movePosition(QTextCursor::EndOfLine)) {
        if (c.columnNumber() <= origCol)
            c.movePosition(QTextCursor::PreviousCharacter);
    }
};

// like above, but starting from a temporary copy to probe the line's hard limit
auto endLimitFor = [](const QTextCursor& from) {
    QTextCursor tmp = from;
    moveToVisibleEOL(tmp);
    return tmp.position();
};

// RAII guard to flip selectionHighlighting_ off during programmatic selection changes
struct SelHiGuard {
    bool& ref;
    bool saved;
    explicit SelHiGuard(bool& on) : ref(on), saved(on) { ref = false; }
    ~SelHiGuard() { ref = saved; }
};

inline int spaceColumns(const QFont& f, int px) {
    const QFontMetricsF fm(f);
    const qreal w = fm.horizontalAdvance(QStringLiteral(" "));
    if (w <= 0.0)
        return 0;
    return static_cast<int>(std::llround(std::fabs(px) / w));
}

inline QPoint clampPoint(const QPoint& p, const QWidget* w) {
    return QPoint(std::clamp(p.x(), 0, w->width()), std::clamp(p.y(), 0, w->height()));
}

inline void safePrevCharSameBlock(QTextCursor& c) {
    const int blk = c.blockNumber();
    if (!c.movePosition(QTextCursor::PreviousCharacter) || c.blockNumber() != blk)
        c.movePosition(QTextCursor::NextCharacter);  // restore if we crossed block
}

inline void safeNextCharSameBlock(QTextCursor& c) {
    const int blk = c.blockNumber();
    if (!c.movePosition(QTextCursor::NextCharacter) || c.blockNumber() != blk)
        c.movePosition(QTextCursor::PreviousCharacter);  // restore if we crossed block
}
}  // namespace

void TextEdit::highlightColumn(const QTextCursor& endCur, int gap) {
    SelHiGuard guard(selectionHighlighting_);

    QTextCursor cur = textCursor();
    if (cur.hasSelection()) {
        // drop any user selection to avoid mixing with column selections
        cur.setPosition(cur.position());
        setTextCursor(cur);
    }

    const int startIndent = cur.columnNumber();
    const int endIndent = endCur.columnNumber() + gap;

    const int hDistance = std::abs(endIndent - startIndent);
    const int minIndent = std::min(startIndent, endIndent);

    QTextCursor tlCur;
    QTextCursor limitCur;
    if (cur < endCur) {
        tlCur = cur;
        limitCur = endCur;
        if (startIndent > endIndent)
            tlCur.setPosition(tlCur.position() - (startIndent - endIndent));
    }
    else {
        tlCur = endCur;
        limitCur = cur;
        if (endIndent > startIndent)
            tlCur.setPosition(tlCur.position() - std::max(tlCur.columnNumber() - startIndent, 0));
    }

    // make sure limitCur is inside the last visible glyph on its line
    {
        QTextCursor tmp = limitCur;
        const int colNum = tmp.columnNumber();
        if (tmp.movePosition(QTextCursor::EndOfLine)) {
            if (tmp.columnNumber() <= colNum)
                tmp.movePosition(QTextCursor::PreviousCharacter);
        }
        limitCur = tmp;
    }

    QList<QTextEdit::ExtraSelection> es = extraSelections();

    // trim any previous column selections while keeping redSel_ and currentLine_
    int toRemove = colSel_.count() + redSel_.count();
    while (toRemove > 0 && !es.isEmpty()) {
        es.removeLast();
        --toRemove;
    }
    colSel_.clear();

    QTextEdit::ExtraSelection extra;
    extra.format.setBackground(palette().highlight().color());
    extra.format.setForeground(palette().highlightedText().color());

    bool empty = true;
    int guardCount = 0;

    QTextCursor curCopy = cur;
    QTextCursor tmp;
    while (tlCur <= limitCur) {
        if (++guardCount > 1000) {  // safety net for runaway selections
            emit hugeColumn();
            break;
        }

        curCopy.setPosition(tlCur.position());

        tmp = curCopy;
        moveToVisibleEOL(tmp);

        const int endPos = std::min(curCopy.position() + hDistance, tmp.position());
        curCopy.setPosition(endPos, QTextCursor::KeepAnchor);

        if (empty && curCopy.hasSelection())
            empty = false;

        extra.cursor = curCopy;
        colSel_.append(extra);

        // advance tlCur to next line start + minIndent columns when possible
        int prevCol = tlCur.columnNumber();
        if (tlCur.movePosition(QTextCursor::EndOfLine)) {
            if (tlCur.columnNumber() > prevCol) {
                if (!tlCur.movePosition(QTextCursor::NextCharacter))
                    break;
            }
        }
        else if (!tlCur.movePosition(QTextCursor::NextCharacter)) {
            break;
        }

        QTextCursor eolProbe = tlCur;
        moveToVisibleEOL(eolProbe);
        tlCur.setPosition(std::min(tlCur.position() + minIndent, eolProbe.position()));
    }

    if (empty)
        colSel_.clear();
    else
        es.append(colSel_);

    es.append(redSel_);

    if (!currentLine_.cursor.isNull())
        es.prepend(currentLine_);

    setExtraSelections(es);

    if (!colSel_.isEmpty())
        emit canCopy(true);

    if (selectionTimerId_) {
        killTimer(selectionTimerId_);
        selectionTimerId_ = 0;
    }
    selectionTimerId_ = startTimer(kUpdateIntervalMs);
}

void TextEdit::makeColumn(const QPoint& endPoint) {
    QPoint p = clampPoint(endPoint, viewport());
    QTextCursor endCur = cursorForPosition(p);
    QRect cRect = cursorRect(endCur);

    const bool rtl = (endCur.block().textDirection() == Qt::RightToLeft);

    // gently snap to neighbor glyph when the pointer is just above or below the current cell
    if (rtl) {
        if (p.y() <= cRect.top()) {
            QTextCursor t = endCur;
            if (t.movePosition(QTextCursor::StartOfLine) && t.movePosition(QTextCursor::PreviousCharacter) &&
                t.blockNumber() == endCur.blockNumber()) {
                endCur = t;
                cRect = cursorRect(endCur);
            }
        }
        else if (p.y() > cRect.bottom()) {
            QTextCursor t = endCur;
            if (t.movePosition(QTextCursor::NextCharacter) && t.blockNumber() == endCur.blockNumber()) {
                endCur = t;
                cRect = cursorRect(endCur);
            }
        }
    }

    int extraGap = 0;

    // handle pointer above current cell
    if (p.y() <= cRect.top()) {
        const bool horizOk = rtl ? (p.x() <= cRect.right() + 1) : (p.x() >= cRect.left());
        if (horizOk) {
            QTextCursor t = endCur;
            if (!t.movePosition(QTextCursor::StartOfLine) && t.movePosition(QTextCursor::PreviousCharacter) &&
                t.blockNumber() == endCur.blockNumber()) {
                QRect tRect = cursorRect(t);
                if (p.y() >= tRect.top()) {
                    endCur = t;
                    cRect = tRect;
                    extraGap = spaceColumns(document()->defaultFont(), std::abs(p.x() - cRect.center().x()));
                    p = cRect.center();
                }
            }
        }
    }
    // handle pointer below current cell
    else if (p.y() > cRect.bottom()) {
        p.setY(std::max(0, cRect.center().y()));
        endCur = cursorForPosition(p);
        cRect = cursorRect(endCur);

        if (rtl) {
            if (p.y() <= cRect.top()) {
                QTextCursor t = endCur;
                safePrevCharSameBlock(t);
                endCur = t;
                cRect = cursorRect(endCur);
            }
            else if (p.y() > cRect.bottom()) {
                QTextCursor t = endCur;
                safeNextCharSameBlock(t);
                endCur = t;
                cRect = cursorRect(endCur);
            }
        }
    }

    // clamp horizontally to keep rectangular selections sane across LTR/RTL
    QPoint c = cRect.center();
    if (rtl)
        p.setX(std::min(p.x(), std::min(c.x(), viewport()->width())));
    else
        p.setX(std::max(std::max(0, c.x()), p.x()));
    p.setY(std::max(std::max(0, c.y()), p.y()));

    endCur = cursorForPosition(p);

    if (rtl) {
        cRect = cursorRect(endCur);
        if (p.y() <= cRect.top()) {
            QTextCursor t = endCur;
            safePrevCharSameBlock(t);
            endCur = t;
        }
        else if (p.y() > cRect.bottom()) {
            QTextCursor t = endCur;
            safeNextCharSameBlock(t);
            endCur = t;
        }
    }

    const int gapCols =
        spaceColumns(document()->defaultFont(), std::abs(p.x() - cursorRect(endCur).center().x())) + extraGap;

    highlightColumn(endCur, gapCols);
}

void TextEdit::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() == Qt::LeftButton && event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
        makeColumn(event->position().toPoint());
        event->accept();
        return;
    }

    QPlainTextEdit::mouseMoveEvent(event);

    if (!highlighter_)
        return;

    if (event->modifiers() != Qt::ControlModifier) {
        if (viewport()->cursor().shape() != Qt::IBeamCursor)
            viewport()->setCursor(Qt::IBeamCursor);
        return;
    }

    const bool overLink = !getUrl(cursorForPosition(event->position().toPoint()).position()).isEmpty();
    const auto desired = overLink ? Qt::PointingHandCursor : Qt::IBeamCursor;
    if (viewport()->cursor().shape() != desired)
        viewport()->setCursor(desired);
}

void TextEdit::mousePressEvent(QMouseEvent* event) {
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;

    mousePressed_ = true;

    if (tripleClickTimer_.isValid()) {
        if (!tripleClickTimer_.hasExpired(qApp->doubleClickInterval()) && event->buttons() == Qt::LeftButton) {
            tripleClickTimer_.invalidate();
            if (event->modifiers() != Qt::ControlModifier) {
                // select trimmed content of the line
                QTextCursor txtCur = textCursor();
                const QString txt = txtCur.block().text();
                const int l = txt.length();

                txtCur.movePosition(QTextCursor::StartOfBlock);

                int i = 0;
                while (i < l && txt.at(i).isSpace())
                    ++i;

                if (i < l) {
                    txtCur.setPosition(txtCur.position() + i);
                    int j = l;
                    while (j > i && txt.at(j - 1).isSpace())
                        --j;
                    txtCur.setPosition(txtCur.position() + (j - i), QTextCursor::KeepAnchor);
                    setTextCursor(txtCur);
                }

                if (txtCur.hasSelection()) {
                    if (QClipboard* cl = QApplication::clipboard(); cl && cl->supportsSelection())
                        cl->setText(txtCur.selection().toPlainText(), QClipboard::Selection);
                }

                event->accept();
                return;
            }
        }
        else {
            tripleClickTimer_.invalidate();
        }
    }

    if (!colSel_.isEmpty() && event->button() != Qt::RightButton)
        removeColumnHighlight();

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
            makeColumn(event->position().toPoint());
            event->accept();
            return;
        }
        pressPoint_ = event->position().toPoint();
    }

    QPlainTextEdit::mousePressEvent(event);
}

void TextEdit::mouseReleaseEvent(QMouseEvent* event) {
    QPlainTextEdit::mouseReleaseEvent(event);
    mousePressed_ = false;

    if (event->button() != Qt::LeftButton || !highlighter_)
        return;

    if (event->modifiers() != Qt::ControlModifier) {
        if (viewport()->cursor().shape() == Qt::PointingHandCursor)
            viewport()->setCursor(Qt::IBeamCursor);
        return;
    }

    if (viewport()->cursor().shape() != Qt::PointingHandCursor)
        return;

    const QPoint pos = event->position().toPoint();
    QTextCursor cur = cursorForPosition(pos);

    if (cur == cursorForPosition(pressPoint_)) {
        QString str = getUrl(cur.position());
        if (!str.isEmpty()) {
            QUrl url(str);
            if (url.isRelative())
                url = QUrl::fromUserInput(str, QStringLiteral("/"));

            const bool haveGio = QStandardPaths::findExecutable(QStringLiteral("gio")).isEmpty() == false;
            const bool launched =
                haveGio &&
                QProcess::startDetached(QStringLiteral("gio"), QStringList{QStringLiteral("open"), url.toString()});
            if (!launched)
                QDesktopServices::openUrl(url);
        }
    }

    pressPoint_ = QPoint();
}

void TextEdit::mouseDoubleClickEvent(QMouseEvent* event) {
    tripleClickTimer_.start();
    QPlainTextEdit::mouseDoubleClickEvent(event);

    if (event->button() == Qt::LeftButton && event->modifiers() == Qt::ControlModifier) {
        QTextCursor txtCur = textCursor();
        const int blockPos = txtCur.block().position();
        const QString txt = txtCur.block().text();
        const int l = txt.length();

        int anc = txtCur.anchor();
        int pos = txtCur.position();

        while (anc > blockPos && !txt.at(anc - blockPos - 1).isSpace())
            --anc;
        while (pos < blockPos + l && !txt.at(pos - blockPos).isSpace())
            ++pos;

        if (anc != textCursor().anchor() || pos != textCursor().position()) {
            txtCur.setPosition(anc);
            txtCur.setPosition(pos, QTextCursor::KeepAnchor);
            setTextCursor(txtCur);

            if (txtCur.hasSelection()) {
                if (QClipboard* cl = QApplication::clipboard(); cl && cl->supportsSelection())
                    cl->setText(txtCur.selection().toPlainText(), QClipboard::Selection);
            }

            event->accept();
        }
    }
}

void TextEdit::removeColumnHighlight() {
    int n = colSel_.count();
    if (n == 0)
        return;

    QList<QTextEdit::ExtraSelection> es = extraSelections();
    const int nRed = redSel_.count();

    while (n > 0 && es.size() - nRed > 0) {
        es.removeAt(es.size() - 1 - nRed);
        --n;
    }

    colSel_.clear();
    setExtraSelections(es);

    if (!textCursor().hasSelection())
        emit canCopy(false);

    if (selectionTimerId_) {
        killTimer(selectionTimerId_);
        selectionTimerId_ = 0;
    }
    selectionTimerId_ = startTimer(kUpdateIntervalMs);
}

int TextEdit::selectionSize() const {
    int res = textCursor().selectedText().size();
    if (res > 0)
        return res;

    for (const auto& extra : std::as_const(colSel_))
        res += extra.cursor.selectedText().size();

    return res;
}

}  // namespace Texxy
