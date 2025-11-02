

#include "ui/tabwidget.h"

namespace Texxy {

TabWidget::TabWidget(QWidget* parent) : QTabWidget(parent) {
    tb_ = new TabBar;
    setTabBar(tb_);
    setFocusProxy(nullptr);            // ensure that the tab bar isn't the focus proxy...
    tb_->setFocusPolicy(Qt::NoFocus);  // ... and don't let the active tab get focus
    setFocusPolicy(Qt::NoFocus);       // also, give the Tab key focus to the page
    curIndx_ = -1;
    timerId_ = 0;
    connect(this, &QTabWidget::currentChanged, this, &TabWidget::tabSwitch);
}
/*************************/
TabWidget::~TabWidget() {
    delete tb_;
}
/*************************/
void TabWidget::tabSwitch(int index) {
    curIndx_ = index;
    if (timerId_) {
        killTimer(timerId_);
        timerId_ = 0;
    }
    timerId_ = startTimer(50);
}
/*************************/
void TabWidget::timerEvent(QTimerEvent* e) {
    QTabWidget::timerEvent(e);

    if (e->timerId() == timerId_) {
        killTimer(e->timerId());
        timerId_ = 0;
        emit currentTabChanged(curIndx_);

        if (QWidget* w = widget(curIndx_)) {
            const int n = activatedTabs_.size();
            activatedTabs_.removeOne(w);
            activatedTabs_ << w;
            if (n <= 1 && activatedTabs_.size() > 1)
                emit hasLastActiveTab(true);
        }
    }
}
/*************************/
QWidget* TabWidget::getLastActiveTab() {
    const int n = activatedTabs_.size();
    if (n > 1) {
        if (QWidget* w = activatedTabs_.at(n - 2))
            return w;
    }
    return nullptr;
}
/*************************/
void TabWidget::removeTab(int index) {
    if (QWidget* w = widget(index)) {
        const int n = activatedTabs_.size();
        activatedTabs_.removeOne(w);
        if (n > 1 && activatedTabs_.size() <= 1)
            emit hasLastActiveTab(false);
    }
    QTabWidget::removeTab(index);
}
/*************************/
void TabWidget::selectLastActiveTab() {
    const int n = activatedTabs_.size();
    if (n > 1) {
        if (QWidget* w = activatedTabs_.at(n - 2))
            setCurrentWidget(w);
    }
}
/*************************/
void TabWidget::keyPressEvent(QKeyEvent* event) {
    event->ignore();  // we have our shortcuts for tab switching
}

}  // namespace Texxy
