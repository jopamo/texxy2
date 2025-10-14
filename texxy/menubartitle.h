/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2023 <tsujan2000@gmail.com>
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

#ifndef MENUBARTITLE_H
#define MENUBARTITLE_H

#include <QLabel>
#include <QPaintEvent>
#include <QMouseEvent>

namespace Texxy {

class MenuBarTitle : public QLabel {
    Q_OBJECT

   public:
    MenuBarTitle(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

    void setTitle(const QString& title);

    void setStart(int s) { start_ = s; }
    void setHeight(int h) { height_ = h; }

   signals:
    void doubleClicked();

   protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QSize sizeHint() const override;

   private:
    QString elidedText_;
    QString lastText_;
    int lastWidth_;
    int start_;
    int height_;
};

}  // namespace Texxy

#endif  // MENUBARTITLE_H
