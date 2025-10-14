/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2021 <tsujan2000@gmail.com>
 *
 * Texxy is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Texxy is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @license GPL-3.0+ <https://spdx.org/licenses/GPL-3.0+.html>
 */

#include "tabwidget.h"

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
