// src/ui/ui/vscrollbar.cpp
// vscrollbar.cpp

#include "ui/vscrollbar.h"
#include <QCursor>
#include <QApplication>
#include <cmath>

namespace Texxy {

VScrollBar::VScrollBar(QWidget* parent)
    : QScrollBar(parent), m_accumulatedAngleDelta(0.0), m_accumulatedPixelDelta(0.0, 0.0) {}

void VScrollBar::wheelEvent(QWheelEvent* event) {
    // If not “real” wheel event or not over the scrollbar, fallback
    if (!underMouse() || !event->spontaneous() || event->source() != Qt::MouseEventNotSynthesized ||
        !rect().contains(mapFromGlobal(QCursor::pos()))) {
        QScrollBar::wheelEvent(event);
        return;
    }

    // Prefer pixelDelta for high-resolution devices
    QPointF pixel = event->pixelDelta();
    QPoint angle = event->angleDelta();

    int step = 0;

    if (!pixel.isNull()) {
        // accumulate pixel delta (vertical component)
        m_accumulatedPixelDelta += pixel;
        // decide threshold: e.g. 1 line = pageStep / 5 or something
        qreal thresh = pageStep() / 2.0;
        if (qAbs(m_accumulatedPixelDelta.y()) >= thresh) {
            step = int(m_accumulatedPixelDelta.y() / thresh);
            m_accumulatedPixelDelta.setY(0);
        }
    }
    else {
        // Use angleDelta fallback for classic mouse wheels
        qreal deltaAngle = (std::abs(angle.x()) > std::abs(angle.y()) ? angle.x() : angle.y());
        m_accumulatedAngleDelta += deltaAngle;
        // Qt’s standard “one notch” is 120 units
        if (std::abs(m_accumulatedAngleDelta) >= 120.0) {
            step = computeStepFromAngleDelta(m_accumulatedAngleDelta);
            m_accumulatedAngleDelta = 0.0;
        }
    }

    if (step != 0) {
        // Optionally accelerate if Shift pressed
        int factor = (event->modifiers() & Qt::ShiftModifier) ? 2 : 1;
        int scrollStep = step * std::max(pageStep() / factor, 1);
        int newPos = sliderPosition() - scrollStep;  // minus because positive angle = wheel away = scroll up
        newPos = qBound(minimum(), newPos, maximum());
        setSliderPosition(newPos);
        event->accept();
    }
    else {
        // Not enough movement yet — ignore to let parent accumulate / other handlers
        event->ignore();
    }
}

int VScrollBar::computeStepFromAngleDelta(qreal deltaAngle) {
    // deltaAngle > 0 means wheel moved “away” (scroll upward)
    // map to integer step count: e.g. deltaAngle / 120, but we invert sign to scroll direction
    int sign = (deltaAngle < 0 ? -1 : 1);
    return -sign * int(std::abs(deltaAngle) / 120.0 + 0.5);
}

}  // namespace Texxy
