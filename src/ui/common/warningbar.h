// src/ui/common/warningbar.h

#ifndef WARNINGBAR_H
#define WARNINGBAR_H

#include <QWidget>
#include <QPointer>
#include <QEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QGridLayout>
#include <QPalette>
#include <QLabel>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QPropertyAnimation>
#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QList>
#include <QRect>
#include <QString>
#include <algorithm>

namespace Texxy {

static constexpr int kAnimDurationMs = 150;  // animation duration in ms

class WarningBar : public QWidget {
    Q_OBJECT
   public:
    explicit WarningBar(const QString& message, int verticalOffset = 0, int timeout = 10, QWidget* parent = nullptr)
        : QWidget(parent),
          message_(message),
          vOffset_(verticalOffset),
          isClosing_(false),
          mousePressed_(false),
          timer_(nullptr),
          grid_(nullptr) {
        bool anotherBar = false;  // track if an older bar exists

        // show only one warning bar at a time
        if (parent) {
            const QList<WarningBar*> warningBars = parent->findChildren<WarningBar*>();
            for (WarningBar* wb : warningBars) {
                if (wb && wb != this) {
                    wb->closeBar();
                    anotherBar = true;
                }
            }
        }

        // translucent layer styling
        setAutoFillBackground(true);
        QPalette p = palette();
        p.setColor(foregroundRole(), Qt::white);
        p.setColor(backgroundRole(), timeout > 0 ? QColor(125, 0, 0, 200) : QColor(0, 70, 0, 210));
        setPalette(p);

        // layout with a vertical compressor spacer and the label
        grid_ = new QGridLayout;
        grid_->setContentsMargins(5, 0, 5, 5);  // top margin is added when setting geometry
        auto* spacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
        grid_->addItem(spacer, 0, 0);

        auto* warningLabel = new QLabel(message_);
        warningLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);  // not needed but harmless
        warningLabel->setWordWrap(true);
        grid_->addWidget(warningLabel, 1, 0);
        setLayout(grid_);

        // compress vertically and show with animation when a parent exists
        if (parent) {
            QTimer::singleShot(anotherBar ? kAnimDurationMs + 10 : 0, this, [this, parent]() {
                parent->installEventFilter(this);
                const int h = grid_->minimumHeightForWidth(parent->width()) + grid_->contentsMargins().bottom();
                const QRect g(0, parent->height() - h - vOffset_, parent->width(), h);
                setGeometry(g);

                animation_ = new QPropertyAnimation(this, "geometry", this);
                animation_->setEasingCurve(QEasingCurve::Linear);
                animation_->setDuration(kAnimDurationMs);
                animation_->setStartValue(QRect(0, parent->height() - vOffset_, parent->width(), 0));
                animation_->setEndValue(g);
                animation_->start();
                show();
            });
        }
        else {
            show();
        }

        // auto close behavior
        setTimeout(timeout);
    }

    void setTimeout(int timeout) {  // can be used to change the timeout
        if (timeout <= 0) {
            if (timer_) {
                timer_->stop();
                timer_->deleteLater();
                timer_ = nullptr;
            }
            return;
        }

        if (!timer_) {
            timer_ = new QTimer(this);
            timer_->setSingleShot(true);
            connect(timer_, &QTimer::timeout, this, [this]() {
                if (!mousePressed_)
                    closeBar();
            });
        }
        timer_->start(timeout * 1000);
    }

    bool eventFilter(QObject* o, QEvent* e) override {
        if (e->type() == QEvent::Resize) {
            if (auto* w = qobject_cast<QWidget*>(o)) {
                if (w == parentWidget()) {
                    if (animation_) {
                        animation_->stop();
                        if (isClosing_) {
                            deleteLater();
                            return false;
                        }
                    }
                    // compress as far as text is fully visible
                    const int h = grid_->minimumHeightForWidth(w->width()) + grid_->contentsMargins().bottom();
                    setGeometry(QRect(0, w->height() - h - vOffset_, w->width(), h));
                }
            }
        }
        return false;
    }

    QString getMessage() const { return message_; }
    bool isClosing() const { return isClosing_; }

   public slots:
    void closeBar() {
        if (animation_ && parentWidget()) {
            if (!isClosing_) {
                isClosing_ = true;
                animation_->stop();
                animation_->setStartValue(geometry());
                animation_->setEndValue(QRect(0, parentWidget()->height() - vOffset_, parentWidget()->width(), 0));
                animation_->start();
                connect(animation_, &QAbstractAnimation::finished, this, &QObject::deleteLater);
            }
            return;
        }
        // prefer deleteLater to avoid deleting during event processing
        deleteLater();
    }

    void closeBarOnScrolling(const QRect& /*rect*/, int dy) {
        if (timer_ && dy != 0)
            closeBar();
    }

   protected:
    void mousePressEvent(QMouseEvent* event) override {
        QWidget::mousePressEvent(event);
        mousePressed_ = true;
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        QWidget::mouseReleaseEvent(event);
        mousePressed_ = false;
        if (timer_)
            QTimer::singleShot(0, this, &WarningBar::closeBar);
    }

   private:
    QString message_;
    int vOffset_;
    bool isClosing_;
    bool mousePressed_;
    QTimer* timer_;
    QGridLayout* grid_;
    QPointer<QPropertyAnimation> animation_;
};

}  // namespace Texxy

#endif  // WARNINGBAR_H
