// src/ui/ui/tabbar.cpp
/*
 * texxy/tabbar.cpp
 */

#include <QPointer>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QIcon>
#include <QApplication>
#include <QToolTip>
#include <QCursor>
#include "ui/tabbar.h"

namespace Texxy {

static constexpr const char* kTabDroppedProp =
    "_texxy_tab_dropped";  // property set by TexxyWindow::dropEvent when a tab drop is accepted
static constexpr const char* kMimeType = "application/texxy-tab";  // mime type for dragged tab index

const char* TabBar::tabDropped = kTabDroppedProp;

TabBar::TabBar(QWidget* parent) : QTabBar(parent) {
    hideSingle_ = false;
    locked_ = false;
    dragStarted_ = false;
    noTabDND_ = false;

    setMouseTracking(true);
    setElideMode(Qt::ElideMiddle);  // works with minimumTabSizeHint
}
/*************************/
void TabBar::mousePressEvent(QMouseEvent* event) {
    dragStarted_ = false;
    dragStartPosition_ = QPoint();

    if (locked_) {
        event->ignore();
        return;
    }
    QTabBar::mousePressEvent(event);

    if (event->button() == Qt::LeftButton) {
        if (tabAt(event->position().toPoint()) > -1)
            dragStartPosition_ = event->position().toPoint();
        else if (event->type() == QEvent::MouseButtonDblClick && count() > 0)
            emit addEmptyTab();
    }
}
/*************************/
void TabBar::mouseReleaseEvent(QMouseEvent* event) {
    dragStarted_ = false;
    dragStartPosition_ = QPoint();

    QTabBar::mouseReleaseEvent(event);
    if (event->button() == Qt::MiddleButton) {
        int index = tabAt(event->position().toPoint());
        if (index > -1)
            emit tabCloseRequested(index);
        else
            emit hideTabBar();
    }
}
/*************************/
void TabBar::mouseMoveEvent(QMouseEvent* event) {
    if (!dragStarted_ && !dragStartPosition_.isNull() &&
        (event->position().toPoint() - dragStartPosition_).manhattanLength() >= QApplication::startDragDistance()) {
        dragStarted_ = true;
    }

    if (!noTabDND_ && (event->buttons() & Qt::LeftButton) && dragStarted_ &&
        !window()->geometry().contains(event->globalPosition().toPoint())) {
        int index = currentIndex();
        if (index == -1) {
            event->accept();
            return;
        }

        // detach or drop the tab only after finishing the drag and drop to keep the tabbar stable

        QDrag* drag = new QDrag(this);
        QMimeData* mimeData = new QMimeData;
        QByteArray array = QByteArray::number(index);  // avoid QString allocation
        mimeData->setData(kMimeType, array);
        drag->setMimeData(mimeData);

        static QPixmap s_px = QIcon(":icons/tab.svg").pixmap(22, 22);  // cache pixmap for speed
        drag->setPixmap(s_px);
        drag->setHotSpot(QPoint(s_px.width() / 2, s_px.height()));

        int N = count();
        Qt::DropAction dragged = drag->exec(Qt::MoveAction);
        if (dragged != Qt::MoveAction) {
            // drop not accepted by a Texxy window, detach if there is more than one tab
            if (N > 1)
                emit tabDetached();
            else
                finishMouseMoveEvent();
        }
        else {
            // another app may accept the drop, check our property flag and detach if missing
            if (property(TabBar::tabDropped).toBool())
                setProperty(TabBar::tabDropped, QVariant());  // reset
            else {
                if (N > 1)
                    emit tabDetached();
                else
                    finishMouseMoveEvent();
            }
        }
        event->accept();
        drag->deleteLater();
    }
    else {
        QTabBar::mouseMoveEvent(event);
        int index = tabAt(event->position().toPoint());
        static int lastTipIndex = -1;  // reduces redundant tooltip updates
        if (index > -1) {
            if (index != lastTipIndex) {
                lastTipIndex = index;
                QToolTip::showText(QCursor::pos(), tabToolTip(index), this);
            }
        }
        else {
            if (lastTipIndex != -1) {
                lastTipIndex = -1;
                QToolTip::hideText();
            }
        }
    }
}
/*************************/
// Don't show tooltip with setTabToolTip()
bool TabBar::event(QEvent* event) {
#ifndef QT_NO_TOOLTIP
    if (event->type() == QEvent::ToolTip)
        return QWidget::event(event);
    else
        return QTabBar::event(event);
#else
    return QTabBar::event(event);
#endif
}
/*************************/
void TabBar::wheelEvent(QWheelEvent* event) {
    if (!locked_)
        QTabBar::wheelEvent(event);
    else
        event->ignore();
}
/*************************/
void TabBar::tabRemoved(int /* index*/) {
    if (hideSingle_ && count() == 1)
        hide();
}
/*************************/
void TabBar::tabInserted(int /* index*/) {
    if (hideSingle_) {
        if (count() == 1)
            hide();
        else if (count() == 2)
            show();
    }
}
/*************************/
void TabBar::finishMouseMoveEvent() {
    dragStarted_ = false;
    dragStartPosition_ = QPoint();

    // synthesize a benign move to flush any hover states
    QMouseEvent finishingEvent(QEvent::MouseMove, QPointF(), QCursor::pos(), Qt::NoButton, Qt::NoButton,
                               Qt::NoModifier);
    mouseMoveEvent(&finishingEvent);
}
/*************************/
void TabBar::releaseMouse() {
    // synthesize a release before any tab removal to keep visuals in sync
    QMouseEvent releasingEvent(QEvent::MouseButtonRelease, QPointF(), QCursor::pos(), Qt::LeftButton, Qt::NoButton,
                               Qt::NoModifier);
    mouseReleaseEvent(&releasingEvent);
}
/*************************/
QSize TabBar::tabSizeHint(int index) const {
    switch (shape()) {
        case QTabBar::RoundedWest:
        case QTabBar::TriangularWest:
        case QTabBar::RoundedEast:
        case QTabBar::TriangularEast:
            return QSize(QTabBar::tabSizeHint(index).width(), qMin(height() / 2, QTabBar::tabSizeHint(index).height()));
        default:
            return QSize(qMin(width() / 2, QTabBar::tabSizeHint(index).width()), QTabBar::tabSizeHint(index).height());
    }
}
/*************************/
// Set minimumTabSizeHint to tabSizeHint
// to keep tabs from shrinking with eliding
QSize TabBar::minimumTabSizeHint(int index) const {
    return tabSizeHint(index);
}

}  // namespace Texxy
