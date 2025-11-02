// src/ui/ui/menubartitle.cpp
// menubartitle.cpp

#include "ui/menubartitle.h"
#include <QPainter>
#include <QStyleOption>
#include <QResizeEvent>

namespace Texxy {

MenuBarTitle::MenuBarTitle(QWidget* parent, Qt::WindowFlags f)
    : QLabel(parent, f), lastWidth_(0), start_(0), heightOverride_(0) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setContentsMargins(10, 0, 10, 0);
    setFrameShape(QFrame::NoFrame);

    int minChars = 10;
    int aw = fontMetrics().averageCharWidth();
    setMinimumWidth(aw * minChars);
    heightOverride_ = fontMetrics().height();

    QFont fnt = font();
    fnt.setBold(true);
    fnt.setItalic(true);
    setFont(fnt);

    setContextMenuPolicy(Qt::CustomContextMenu);
}

void MenuBarTitle::setTitle(const QString& title) {
    QString s = title.simplified();
    if (s != lastText_) {
        lastText_ = s;
        elidedText_.clear();  // mark dirty
        update();
    }
    QLabel::setText(s);
}

void MenuBarTitle::resizeEvent(QResizeEvent* ev) {
    QLabel::resizeEvent(ev);
    if (ev->size().width() != lastWidth_) {
        elidedText_.clear();  // mark to recalc
    }
}

void MenuBarTitle::updateElidedText() {
    QRect cr = contentsRect().adjusted(margin(), margin(), -margin(), -margin());
    int w = cr.width();
    if (w <= 0) {
        elidedText_.clear();
        return;
    }
    if (lastText_.isEmpty()) {
        elidedText_.clear();
        return;
    }
    if (elidedText_.isEmpty() || w != lastWidth_) {
        lastWidth_ = w;
        elidedText_ = fontMetrics().elidedText(lastText_, Qt::ElideMiddle, w);
    }
}

void MenuBarTitle::paintEvent(QPaintEvent* /*event*/) {
    updateElidedText();
    if (elidedText_.isEmpty()) {
        QLabel::paintEvent(nullptr);
        return;
    }

    QPainter painter(this);
    QStyleOption opt;
    opt.initFrom(this);

    QRect cr = contentsRect().adjusted(margin(), margin(), -margin(), -margin());
    // shift region by start_ if needed
    cr.adjust(start_, 0, 0, 0);

    style()->drawItemText(&painter, cr, Qt::AlignRight | Qt::AlignVCenter, opt.palette, isEnabled(), elidedText_,
                          foregroundRole());
}

void MenuBarTitle::mouseDoubleClickEvent(QMouseEvent* event) {
    QLabel::mouseDoubleClickEvent(event);
    if (event->button() == Qt::LeftButton)
        emit doubleClicked();
}

QSize MenuBarTitle::sizeHint() const {
    QWidget* p = parentWidget();
    if (p) {
        return QSize(p->width() - start_, heightOverride_);
    }
    return QLabel::sizeHint();
}

}  // namespace Texxy
