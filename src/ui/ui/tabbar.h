

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
