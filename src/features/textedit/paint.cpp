// src/features/textedit/paint.cpp
#include "textedit/textedit_prelude.h"

namespace Texxy {

namespace {

// fill rect with brush while honoring gradient logical coordinates if a gradientRect is provided
void fillBackground(QPainter* painter, const QRectF& rect, const QBrush& brush, const QRectF& gradientRect = QRectF()) {
    painter->save();

    QBrush b = brush;
    if (b.style() >= Qt::LinearGradientPattern && b.style() <= Qt::ConicalGradientPattern) {
        if (!gradientRect.isNull()) {
            QTransform transform = QTransform::fromTranslate(gradientRect.left(), gradientRect.top());
            transform.scale(gradientRect.width(), gradientRect.height());
            b.setTransform(transform);
            // Qt exposes a const QGradient* from QBrush, so we flip coordinate mode via const_cast here
            const_cast<QGradient*>(b.gradient())->setCoordinateMode(QGradient::LogicalMode);
        }
    }
    else {
        painter->setBrushOrigin(rect.topLeft());
    }

    painter->fillRect(rect, b);
    painter->restore();
}

// true if a block rectangle intersects the clip vertically
inline bool intersectsVertically(const QRectF& r, const QRect& clip) {
    return r.bottom() >= clip.top() && r.top() <= clip.bottom();
}

// compute the y span for auxiliary vertical guides inside a block rect
inline std::pair<int, int> vSpanForBlock(const QRectF& r, const QFontMetricsF& fm) {
    const int yTop = std::lround(r.topLeft().y());
    if (r.height() >= 2.0 * fm.lineSpacing())
        return {yTop, yTop + static_cast<int>(std::lround(fm.height()))};
    return {yTop, static_cast<int>(std::lround(r.bottomLeft().y())) - 1};
}

}  // namespace

void TextEdit::paintEvent(QPaintEvent* event) {
    QPainter painter(viewport());
    Q_ASSERT(qobject_cast<QPlainTextDocumentLayout*>(document()->documentLayout()));

    const QPointF offset = contentOffset();
    QRect clipRect = event->rect();
    const QRect viewportRect = viewport()->rect();

    // cache layout wide values once
    const qreal docWidth = document()->documentLayout()->documentSize().width();
    const qreal docMargin = document()->documentMargin();
    painter.setBrushOrigin(offset);

    // clamp clip right edge to either viewport or document width, whichever is wider
    const int maxX = static_cast<int>(offset.x() + std::max<qreal>(viewportRect.width(), docWidth) - docMargin);
    clipRect.setRight(std::min(clipRect.right(), maxX));
    painter.setClipRect(clipRect);

    const bool editable = !isReadOnly();
    QAbstractTextDocumentLayout::PaintContext context = getPaintContext();

    // these are used in multiple hot paths, avoid repeated construction
    const QFont defaultFont = document()->defaultFont();
    const QFontMetricsF fm(defaultFont);
    const bool isFixedPitch = QFontInfo(defaultFont).fixedPitch();

    const qreal offsetX = offset.x();
    qreal blockY = offset.y();
    qreal tailY = blockY;
    QTextBlock block = firstVisibleBlock();

    while (block.isValid()) {
        const QRectF blockRect = blockBoundingRect(block).translated(QPointF(offsetX, blockY));
        const QPointF layoutOffset(offsetX, blockRect.top());
        const QRectF& r = blockRect;
        QTextLayout* layout = block.layout();

        if (!block.isVisible()) {
            tailY = r.bottom();
            blockY += r.height();
            block = block.next();
            continue;
        }

        // short-circuit if the block is entirely below the viewport
        if (r.top() > clipRect.bottom())
            break;

        if (intersectsVertically(r, clipRect)) {
            const bool rtl = (block.textDirection() == Qt::RightToLeft);

            // apply per-block text option updates for RTL and widget width wrapping
            if (rtl) {
                QTextOption opt = document()->defaultTextOption();
                if (lineWrapMode() == QPlainTextEdit::WidgetWidth)
                    opt.setAlignment(Qt::AlignRight);
                opt.setTextDirection(Qt::RightToLeft);
                layout->setTextOption(opt);
            }

            // block background fill
            const QTextBlockFormat blockFormat = block.blockFormat();
            const QBrush bg = blockFormat.background();
            if (bg != Qt::NoBrush) {
                QRectF contentsRect = r;
                contentsRect.setWidth(std::max<qreal>(r.width(), docWidth));
                fillBackground(&painter, contentsRect, bg);
            }

            // highlight current line when line-number area is visible and separators are shown
            if (lineNumberArea_->isVisible() &&
                (document()->defaultTextOption().flags() & QTextOption::ShowLineAndParagraphSeparators)) {
                QRectF contentsRect = r;
                contentsRect.setWidth(std::max<qreal>(r.width(), docWidth));
                if (contentsRect.contains(cursorRect().center())) {
                    contentsRect.setTop(cursorRect().top());
                    contentsRect.setBottom(cursorRect().bottom());
                    fillBackground(&painter, contentsRect, lineHColor_);
                }
            }

            // translate PaintContext selections to QTextLayout ranges for this block
            QList<QTextLayout::FormatRange> selections;
            const int blpos = block.position();
            const int bllen = block.length();

            for (int i = 0, n = context.selections.size(); i < n; ++i) {
                const auto& range = context.selections.at(i);
                const int selStart = range.cursor.selectionStart() - blpos;
                const int selEnd = range.cursor.selectionEnd() - blpos;

                if (selStart < bllen && selEnd > 0 && selEnd > selStart) {
                    QTextLayout::FormatRange o;
                    o.start = std::max(0, selStart);
                    o.length = std::min(bllen, selEnd) - o.start;
                    o.format = range.format;
                    selections.append(o);
                }
                else if (!range.cursor.hasSelection() && range.format.hasProperty(QTextFormat::FullWidthSelection) &&
                         block.contains(range.cursor.position())) {
                    QTextLayout::FormatRange o;
                    const int posInBlock = range.cursor.position() - blpos;
                    const QTextLine l = layout->lineForTextPosition(std::max(0, posInBlock));
                    if (l.isValid()) {
                        o.start = l.textStart();
                        o.length = l.textLength();
                        if (o.start + o.length == bllen - 1)
                            ++o.length;  // include newline sentinel
                        o.format = range.format;
                        selections.append(o);
                    }
                }
            }

            // cursor painting logic
            const bool drawCursor = (editable || (textInteractionFlags() & Qt::TextSelectableByKeyboard)) &&
                                    context.cursorPosition >= blpos && context.cursorPosition < blpos + bllen;

            bool drawCursorAsBlock = drawCursor && overwriteMode();
            if (drawCursorAsBlock) {
                if (context.cursorPosition == blpos + bllen - 1) {
                    drawCursorAsBlock = false;  // do not draw block cursor on the virtual newline
                }
                else {
                    QTextLayout::FormatRange o;
                    o.start = context.cursorPosition - blpos;
                    o.length = 1;
                    if (darkValue_ > -1) {
                        o.format.setForeground(Qt::black);
                        o.format.setBackground(Qt::white);
                    }
                    else {
                        o.format.setForeground(Qt::white);
                        o.format.setBackground(Qt::black);
                    }
                    selections.append(o);
                }
            }

            // placeholder text when document is empty
            if (!placeholderText().isEmpty() && document()->isEmpty()) {
                painter.save();
                QColor col = palette().text().color();
                col.setAlpha(128);
                painter.setPen(QPen(col));
                const int margin = static_cast<int>(document()->documentMargin());
                painter.drawText(r.adjusted(margin, 0, 0, 0), Qt::AlignTop | Qt::TextWordWrap, placeholderText());
                painter.restore();
            }
            else {
                const bool showSep =
                    document()->defaultTextOption().flags() & QTextOption::ShowLineAndParagraphSeparators;
                if (showSep) {
                    painter.save();
                    painter.setPen(QPen(separatorColor_));
                }

                layout->draw(&painter, layoutOffset, selections, clipRect);

                if (showSep)
                    painter.restore();
            }

            if ((drawCursor && !drawCursorAsBlock) ||
                (editable && context.cursorPosition < -1 && !layout->preeditAreaText().isEmpty())) {
                int cpos = context.cursorPosition;
                if (cpos < -1)
                    cpos = layout->preeditAreaPosition() - (cpos + 2);
                else
                    cpos -= blpos;

                layout->drawCursor(&painter, layoutOffset, cpos, cursorWidth());
            }

            // indentation guide lines based on leading whitespace of the block
            if (drawIndetLines_) {
                QRegularExpressionMatch match;
                if (block.text().indexOf(QRegularExpression("\\s+"), 0, &match) == 0) {
                    painter.save();
                    painter.setOpacity(0.18);

                    QTextCursor cur = textCursor();
                    cur.setPosition(match.capturedLength() + block.position());

                    const auto [yTop, yBottom] = vSpanForBlock(r, fm);
                    const qreal tabWidth = fm.horizontalAdvance(textTab_);

                    if (rtl) {
                        const qreal leftMost = cursorRect(cur).left();
                        qreal x = r.topRight().x() - tabWidth;
                        while (x >= leftMost) {
                            painter.drawLine(QLine(std::lround(x), yTop, std::lround(x), yBottom));
                            x -= tabWidth;
                        }
                    }
                    else {
                        const qreal rightMost = cursorRect(cur).right();
                        qreal x = r.topLeft().x() + tabWidth;
                        while (x <= rightMost) {
                            painter.drawLine(QLine(std::lround(x), yTop, std::lround(x), yBottom));
                            x += tabWidth;
                        }
                    }

                    painter.restore();
                }
            }

            // vertical ruler lines every vLineDistance_ spaces for monospaced fonts in LTR
            if (vLineDistance_ >= 10 && !rtl && isFixedPitch) {
                painter.save();

                QColor col = (darkValue_ > -1) ? QColor(65, 154, 255) : Qt::blue;
                col.setAlpha((darkValue_ > -1) ? 90 : 70);
                painter.setPen(QPen(col));

                QTextCursor cur = textCursor();
                cur.setPosition(block.position());

                const auto [yTop, yBottom] = vSpanForBlock(r, fm);
                const qreal rulerSpace = fm.horizontalAdvance(QLatin1Char(' ')) * static_cast<qreal>(vLineDistance_);

                const qreal rightClip = clipRect.right();
                qreal x = static_cast<qreal>(cursorRect(cur).right()) + rulerSpace;

                while (x <= rightClip) {
                    painter.drawLine(QLine(std::lround(x), yTop, std::lround(x), yBottom));
                    x += rulerSpace;
                }

                painter.restore();
            }
        }

        tailY = r.bottom();
        blockY += r.height();
        block = block.next();
    }

    // fill remaining background when needed
    const bool atDocEnd = !block.isValid();
    const bool needsTailFill = backgroundVisible() && atDocEnd && tailY <= clipRect.bottom() &&
                               (centerOnScroll() || verticalScrollBar()->maximum() == verticalScrollBar()->minimum());

    if (needsTailFill) {
        painter.fillRect(
            QRect(QPoint(static_cast<int>(clipRect.left()), static_cast<int>(tailY)), clipRect.bottomRight()),
            palette().window());
    }
}

void TextEdit::adjustScrollbars() {
    const QSize vSize = viewport()->size();
    auto* resizeEvent = new QResizeEvent(vSize, vSize);
    QCoreApplication::postEvent(viewport(), resizeEvent);
}

}  // namespace Texxy
