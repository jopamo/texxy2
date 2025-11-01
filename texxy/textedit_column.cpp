#include "textedit.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFontMetricsF>
#include <QList>
#include <QMouseEvent>
#include <QPoint>
#include <QProcess>
#include <QRect>
#include <QStandardPaths>
#include <QStringList>
#include <QTextDocumentFragment>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Texxy {

void TextEdit::highlightColumn(const QTextCursor& endCur, int gap) {
    bool selectionHighlightingOrig = selectionHighlighting_;
    selectionHighlighting_ = false;

    QTextCursor cur = textCursor();
    if (cur.hasSelection()) {
        cur.setPosition(cur.position());
        setTextCursor(cur);
    }

    int startIndent = cur.columnNumber();
    int endIndent = endCur.columnNumber() + gap;

    int hDistance = std::abs(endIndent - startIndent);
    int minIndent = std::min(startIndent, endIndent);

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
    int colNum = limitCur.columnNumber();
    if (limitCur.movePosition(QTextCursor::EndOfLine)) {
        if (limitCur.columnNumber() <= colNum)
            limitCur.movePosition(QTextCursor::PreviousCharacter);
    }

    QList<QTextEdit::ExtraSelection> es = extraSelections();
    int n = colSel_.count() + redSel_.count();
    while (n > 0 && !es.isEmpty()) {
        es.removeLast();
        --n;
    }
    colSel_.clear();

    QTextEdit::ExtraSelection extra;
    extra.format.setBackground(palette().highlight().color());
    extra.format.setForeground(palette().highlightedText().color());

    bool empty = true;
    int i = 0;
    QTextCursor tmp;
    QTextCursor curCopy = cur;
    while (tlCur <= limitCur) {
        ++i;
        if (i > 1000) {
            emit hugeColumn();
            break;
        }

        curCopy.setPosition(tlCur.position());
        tmp = curCopy;
        if (tmp.movePosition(QTextCursor::EndOfLine)) {
            if (tmp.columnNumber() <= curCopy.columnNumber() && tmp.position() == curCopy.position() + 1)
                tmp.movePosition(QTextCursor::PreviousCharacter);
        }
        curCopy.setPosition(std::min(curCopy.position() + hDistance, tmp.position()), QTextCursor::KeepAnchor);
        if (empty && curCopy.hasSelection())
            empty = false;

        extra.cursor = curCopy;
        colSel_.append(extra);

        colNum = tlCur.columnNumber();
        if (tlCur.movePosition(QTextCursor::EndOfLine)) {
            if (tlCur.columnNumber() > colNum) {
                if (!tlCur.movePosition(QTextCursor::NextCharacter))
                    break;
            }
        }
        else if (!tlCur.movePosition(QTextCursor::NextCharacter))
            break;
        tmp = tlCur;
        if (tmp.movePosition(QTextCursor::EndOfLine)) {
            if (tmp.columnNumber() <= tlCur.columnNumber())
                tmp.movePosition(QTextCursor::PreviousCharacter);
        }
        tlCur.setPosition(std::min(tlCur.position() + minIndent, tmp.position()));
    }

    if (empty)
        colSel_.clear();
    else
        es.append(colSel_);

    es.append(redSel_);

    if (!currentLine_.cursor.isNull())
        es.prepend(currentLine_);
    setExtraSelections(es);

    selectionHighlighting_ = selectionHighlightingOrig;

    if (!colSel_.isEmpty())
        emit canCopy(true);

    if (selectionTimerId_) {
        killTimer(selectionTimerId_);
        selectionTimerId_ = 0;
    }
    selectionTimerId_ = startTimer(kUpdateIntervalMs);
}

void TextEdit::makeColumn(const QPoint& endPoint) {
    QPoint p(std::clamp(endPoint.x(), 0, viewport()->width()), std::clamp(endPoint.y(), 0, viewport()->height()));
    QTextCursor endCur = cursorForPosition(p);
    QRect cRect(cursorRect(endCur));
    bool rtl = (endCur.block().textDirection() == Qt::RightToLeft);
    if (rtl) {
        if (p.y() <= cRect.top()) {
            QTextCursor tmp = endCur;
            if (tmp.movePosition(QTextCursor::StartOfLine) && tmp.movePosition(QTextCursor::PreviousCharacter) &&
                tmp.blockNumber() == endCur.blockNumber()) {
                endCur = tmp;
                cRect = cursorRect(endCur);
            }
        }
        else if (p.y() > cRect.bottom()) {
            QTextCursor tmp = endCur;
            if (tmp.movePosition(QTextCursor::NextCharacter) && tmp.blockNumber() == endCur.blockNumber()) {
                endCur = tmp;
                cRect = cursorRect(endCur);
            }
        }
    }
    int extraGap = 0;
    if (p.y() <= cRect.top()) {
        if (rtl ? p.x() <= cRect.right() + 1 : p.x() >= cRect.left()) {
            QTextCursor tmp = endCur;
            if (!tmp.movePosition(QTextCursor::StartOfLine) && tmp.movePosition(QTextCursor::PreviousCharacter) &&
                tmp.blockNumber() == endCur.blockNumber()) {
                QRect tRect(cursorRect(tmp));
                if (p.y() >= tRect.top()) {
                    endCur = tmp;
                    cRect = tRect;
                    extraGap = static_cast<int>(std::abs(p.x() - cRect.center().x()) /
                                                QFontMetricsF(document()->defaultFont()).horizontalAdvance(" "));
                    p = cRect.center();
                }
            }
        }
    }
    else if (p.y() > cRect.bottom()) {
        p.setY(std::max(0, cRect.center().y()));
        endCur = cursorForPosition(p);
        cRect = cursorRect(endCur);
        if (rtl) {
            if (p.y() <= cRect.top()) {
                QTextCursor tmp = endCur;
                if (tmp.movePosition(QTextCursor::PreviousCharacter) && tmp.blockNumber() == endCur.blockNumber()) {
                    endCur = tmp;
                    cRect = cursorRect(endCur);
                }
            }
            else if (p.y() > cRect.bottom()) {
                QTextCursor tmp = endCur;
                if (tmp.movePosition(QTextCursor::NextCharacter) && tmp.blockNumber() == endCur.blockNumber()) {
                    endCur = tmp;
                    cRect = cursorRect(endCur);
                }
            }
        }
    }

    QPoint c(cRect.center());
    if (rtl)
        p.setX(std::min(p.x(), std::min(c.x(), viewport()->width())));
    else
        p.setX(std::max(std::max(0, c.x()), p.x()));
    p.setY(std::max(std::max(0, c.y()), p.y()));
    endCur = cursorForPosition(p);
    if (rtl) {
        cRect = cursorRect(endCur);
        if (p.y() <= cRect.top()) {
            QTextCursor tmp = endCur;
            if (tmp.movePosition(QTextCursor::PreviousCharacter) && tmp.blockNumber() == endCur.blockNumber()) {
                endCur = tmp;
            }
        }
        else if (p.y() > cRect.bottom()) {
            QTextCursor tmp = endCur;
            if (tmp.movePosition(QTextCursor::NextCharacter) && tmp.blockNumber() == endCur.blockNumber()) {
                endCur = tmp;
            }
        }
    }

    highlightColumn(
        endCur,
        static_cast<int>(std::abs(p.x() - cursorRect(endCur).center().x()) /
                         QFontMetricsF(document()->defaultFont()).horizontalAdvance(" ")) +
            extraGap);
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

    if (getUrl(cursorForPosition(event->position().toPoint()).position()).isEmpty()) {
        if (viewport()->cursor().shape() != Qt::IBeamCursor)
            viewport()->setCursor(Qt::IBeamCursor);
    }
    else if (viewport()->cursor().shape() != Qt::PointingHandCursor)
        viewport()->setCursor(Qt::PointingHandCursor);
}

void TextEdit::mousePressEvent(QMouseEvent* event) {
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;

    mousePressed_ = true;

    if (tripleClickTimer_.isValid()) {
        if (!tripleClickTimer_.hasExpired(qApp->doubleClickInterval()) && event->buttons() == Qt::LeftButton) {
            tripleClickTimer_.invalidate();
            if (event->modifiers() != Qt::ControlModifier) {
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
                    txtCur.setPosition(txtCur.position() + j - i, QTextCursor::KeepAnchor);
                    setTextCursor(txtCur);
                }
                if (txtCur.hasSelection()) {
                    QClipboard* cl = QApplication::clipboard();
                    if (cl->supportsSelection())
                        cl->setText(txtCur.selection().toPlainText(), QClipboard::Selection);
                }
                event->accept();
                return;
            }
        }
        else
            tripleClickTimer_.invalidate();
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
        if (viewport()->cursor().shape() == Qt::PointingHandCursor) {
            viewport()->setCursor(Qt::IBeamCursor);
        }
        return;
    }
    if (viewport()->cursor().shape() != Qt::PointingHandCursor)
        return;

    QTextCursor cur = cursorForPosition(event->position().toPoint());
    if (cur == cursorForPosition(pressPoint_)) {
        QString str = getUrl(cur.position());
        if (!str.isEmpty()) {
            QUrl url(str);
            if (url.isRelative())
                url = QUrl::fromUserInput(str, "/");
            if (QStandardPaths::findExecutable("gio").isEmpty() ||
                !QProcess::startDetached(QStringLiteral("gio"),
                                         QStringList() << QStringLiteral("open") << url.toString())) {
                QDesktopServices::openUrl(url);
            }
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
                QClipboard* cl = QApplication::clipboard();
                if (cl->supportsSelection())
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
    int nRed = redSel_.count();
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
    for (auto const& extra : std::as_const(colSel_))
        res += extra.cursor.selectedText().size();
    return res;
}

}  // namespace Texxy
