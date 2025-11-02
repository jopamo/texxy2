// src/ui/ui/vscrollbar.h
// vscrollbar.h

#ifndef VSCROLLBAR_H
#define VSCROLLBAR_H

#include <QScrollBar>
#include <QWheelEvent>

namespace Texxy {

class VScrollBar : public QScrollBar {
    Q_OBJECT
   public:
    explicit VScrollBar(QWidget* parent = nullptr);

   protected:
    void wheelEvent(QWheelEvent* event) override;

   private:
    // Accumulate deltas until threshold
    qreal m_accumulatedAngleDelta = 0.0;
    QPointF m_accumulatedPixelDelta = {0.0, 0.0};

    int computeStepFromAngleDelta(qreal deltaAngle);
};

}  // namespace Texxy

#endif  // VSCROLLBAR_H
