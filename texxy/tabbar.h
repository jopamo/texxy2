/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2022 <tsujan2000@gmail.com>
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

#ifndef TABBAR_H
#define TABBAR_H

#include <QTabBar>

namespace Texxy {

class TabBar : public QTabBar {
    Q_OBJECT

   public:
    TabBar(QWidget* parent = nullptr);

    void finishMouseMoveEvent();
    void releaseMouse();

    void hideSingle(bool hide) { hideSingle_ = hide; }

    void lockTabs(bool lock) { locked_ = lock; }

    void noTabDND() { noTabDND_ = true; }

    /* An object property used for knowing whether
       a tab is dropped into one of our windows: */
    static const char* tabDropped;

   signals:
    void tabDetached();
    void addEmptyTab();
    void hideTabBar();

   protected:
    /* from qtabbar.cpp */
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    bool event(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    QSize tabSizeHint(int index) const override;
    QSize minimumTabSizeHint(int index) const override;
    void tabRemoved(int index) override;
    void tabInserted(int index) override;

   private:
    QPoint dragStartPosition_;
    bool dragStarted_;
    bool hideSingle_;
    bool locked_;
    bool noTabDND_;
};

}  // namespace Texxy

#endif  // TABBAR_H
