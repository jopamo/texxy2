// src/features/textedit/linenumbers.cpp
#include "textedit/textedit_prelude.h"

namespace Texxy {

void TextEdit::setCurLineHighlight(int value) {
    // accept explicit grayscale 0..255, otherwise derive subtle overlay from theme
    const int v = std::clamp(value, -1, 255);
    if (v >= 0) {
        lineHColor_ = QColor(v, v, v);
        return;
    }

    if (darkValue_ == -1) {
        // light theme gets a faint dark overlay
        lineHColor_ = QColor(0, 0, 0, 4);
        return;
    }

    // dark theme gets a faint light overlay, alpha derived from background value
    // keeps highlight visible across a wide range of darkValue_
    const int alpha = std::clamp(
        static_cast<int>(std::lround(static_cast<double>(darkValue_ * (19 * darkValue_ - 2813)) / 5175) + 20), 1, 30);
    lineHColor_ = QColor(255, 255, 255, alpha);
}

void TextEdit::showLineNumbers(bool show) {
    if (show) {
        lineNumberArea_->show();

        // prevent duplicate signal connections when toggled repeatedly
        connect(this, &QPlainTextEdit::blockCountChanged, this, &TextEdit::updateLineNumberAreaWidth,
                Qt::UniqueConnection);
        connect(this, &QPlainTextEdit::updateRequest, this, &TextEdit::updateLineNumberArea, Qt::UniqueConnection);
        connect(this, &QPlainTextEdit::cursorPositionChanged, this, &TextEdit::highlightCurrentLine,
                Qt::UniqueConnection);

        updateLineNumberAreaWidth(0);
        highlightCurrentLine();
        return;
    }

    // detach listeners and clear visuals
    disconnect(this, &QPlainTextEdit::blockCountChanged, this, &TextEdit::updateLineNumberAreaWidth);
    disconnect(this, &QPlainTextEdit::updateRequest, this, &TextEdit::updateLineNumberArea);
    disconnect(this, &QPlainTextEdit::cursorPositionChanged, this, &TextEdit::highlightCurrentLine);

    lineNumberArea_->hide();
    setViewportMargins(0, 0, 0, 0);

    QList<QTextEdit::ExtraSelection> es = extraSelections();
    if (!es.isEmpty() && !currentLine_.cursor.isNull())
        es.removeFirst();
    setExtraSelections(es);

    currentLine_.cursor = QTextCursor();
    lastCurrentLine_ = QRect();
}

int TextEdit::lineNumberAreaWidth() {
    // compute number of digits for the largest visible line number
    int blocks = std::max(1, blockCount());
    int digits = 1;
    while (blocks >= 10) {
        blocks /= 10;
        ++digits;
    }

    // build a string of the widest digit repeated to match digits count
    QFont boldFont = font();
    boldFont.setBold(true);
    const QString d = locale().toString(widestDigit_);
    QString num;
    num.reserve(digits * d.size());
    for (int i = 0; i < digits; ++i)
        num += d;

    return 6 + QFontMetrics(boldFont).horizontalAdvance(num);
}

void TextEdit::updateLineNumberAreaWidth(int /* newBlockCount */) {
    // reserve space on the appropriate side for the gutter
    if (QApplication::layoutDirection() == Qt::RightToLeft)
        setViewportMargins(0, 0, lineNumberAreaWidth(), 0);
    else
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void TextEdit::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy != 0) {
        lineNumberArea_->scroll(0, dy);
    }
    else {
        if (lastCurrentLine_.isValid())
            lineNumberArea_->update(0, lastCurrentLine_.y(), lineNumberArea_->width(), lastCurrentLine_.height());

        QRect totalRect;
        const QTextCursor cur = cursorForPosition(rect.center());
        if (rect.contains(cursorRect(cur).center())) {
            const QRectF blockRect = blockBoundingGeometry(cur.block()).translated(contentOffset());
            totalRect = rect.united(blockRect.toRect());
        }
        else {
            totalRect = rect;
        }
        lineNumberArea_->update(0, totalRect.y(), lineNumberArea_->width(), totalRect.height());
    }

    // if the entire viewport was invalidated, recompute the gutter width
    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void TextEdit::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> es = extraSelections();
    if (!es.isEmpty() && !currentLine_.cursor.isNull())
        es.removeFirst();

    // use transparent when paragraph separators are shown to avoid a block overlay
    currentLine_.format.setBackground(
        document()->defaultTextOption().flags() & QTextOption::ShowLineAndParagraphSeparators ? Qt::transparent
                                                                                              : lineHColor_);
    currentLine_.format.setProperty(QTextFormat::FullWidthSelection, true);

    currentLine_.cursor = textCursor();
    currentLine_.cursor.clearSelection();

    es.prepend(currentLine_);
    setExtraSelections(es);
}

void TextEdit::lineNumberAreaPaintEvent(QPaintEvent* event) {
    QPainter painter(lineNumberArea_);

    // theme aware base colors for the gutter and current line marker
    QColor currentBlockFg, currentLineBg, currentLineFg;
    if (darkValue_ > -1) {
        painter.fillRect(event->rect(), QColor(200, 200, 200));
        painter.setPen(Qt::black);
        currentBlockFg = QColor(150, 0, 0);
        currentLineBg = QColor(140, 0, 0);
        currentLineFg = Qt::white;
    }
    else {
        painter.fillRect(event->rect(), QColor(40, 40, 40));
        painter.setPen(Qt::white);
        currentBlockFg = QColor(255, 205, 0);
        currentLineBg = QColor(255, 235, 130);
        currentLineFg = Qt::black;
    }

    const bool rtl = QApplication::layoutDirection() == Qt::RightToLeft;
    const int w = lineNumberArea_->width();
    const int left = rtl ? 3 : 0;

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    const int curBlock = textCursor().blockNumber();
    const int h = fontMetrics().height();

    QFont boldFont = font();
    boldFont.setBold(true);

    QLocale loc = locale();
    loc.setNumberOptions(QLocale::OmitGroupSeparator);

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const QString number = loc.toString(blockNumber + 1);

            if (blockNumber == curBlock) {
                // remember the painted rectangle for targeted updates
                lastCurrentLine_ = QRect(0, top, w, h);

                painter.save();
                painter.setFont(boldFont);
                painter.setPen(currentLineFg);

                QTextCursor tmp = textCursor();
                tmp.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);

                if (tmp.atBlockStart()) {
                    painter.fillRect(0, top, w, h, currentLineBg);
                }
                else {
                    int cy = cursorRect().center().y();
                    painter.fillRect(0, cy - h / 2, w, h, currentLineBg);
                    painter.drawText(left, cy - h / 2, w - 3, h, Qt::AlignRight,
                                     rtl ? QStringLiteral("↲") : QStringLiteral("↳"));

                    painter.setPen(currentBlockFg);
                    if (tmp.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor)) {
                        tmp.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
                        if (!tmp.atBlockStart()) {
                            cy = cursorRect(tmp).center().y();
                            painter.drawText(left, cy - h / 2, w - 3, h, Qt::AlignRight, number);
                        }
                    }
                }
            }

            painter.drawText(left, top, w - 3, h, Qt::AlignRight, number);
            if (blockNumber == curBlock)
                painter.restore();
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

}  // namespace Texxy
