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

#include "menubartitle.h"
#include <QMenuBar>
#include <QPainter>
#include <QStyleOption>

namespace Texxy {

MenuBarTitle::MenuBarTitle(QWidget* parent, Qt::WindowFlags f) : QLabel(parent, f), lastWidth_(0), start_(0) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setContentsMargins(10, 0, 10, 0);
    setFrameShape(QFrame::NoFrame);
    /* set a min width to prevent the window from widening with long texts */
    setMinimumWidth(fontMetrics().averageCharWidth() * 10);
    height_ = fontMetrics().height();

    QFont fnt = font();
    fnt.setBold(true);
    fnt.setItalic(true);
    setFont(fnt);

    setContextMenuPolicy(Qt::CustomContextMenu);
}
/*************************/
void MenuBarTitle::paintEvent(QPaintEvent* /*event*/) {
    QRect cr = contentsRect().adjusted(margin(), margin(), -margin(), -margin());
    QString txt = text();
    /* if the text is changed or its rect is resized (due to window resizing),
       find whether it needs to be elided... */
    if (txt != lastText_ || cr.width() != lastWidth_) {
        lastText_ = txt;
        lastWidth_ = cr.width();
        elidedText_ = fontMetrics().elidedText(txt, Qt::ElideMiddle, lastWidth_);
    }
    /* ... then, draw the (elided) text */
    if (!elidedText_.isEmpty()) {
        QPainter painter(this);
        QStyleOption opt;
        opt.initFrom(this);
        style()->drawItemText(&painter, cr, Qt::AlignRight | Qt::AlignVCenter, opt.palette, isEnabled(), elidedText_,
                              foregroundRole());
    }
}
/*************************/
void MenuBarTitle::mouseDoubleClickEvent(QMouseEvent* event) {
    QLabel::mouseDoubleClickEvent(event);
    if (event->button() == Qt::LeftButton)
        emit doubleClicked();
}
/*************************/
void MenuBarTitle::setTitle(const QString& title) {
    setText(title.simplified());
}
/*************************/
QSize MenuBarTitle::sizeHint() const {
    if (QWidget* p = parentWidget())
        return QSize(p->width() - start_, height_);
    return QLabel::sizeHint();
}

}  // namespace Texxy
