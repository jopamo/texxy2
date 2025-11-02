

#ifndef TABWIDGET_H
#define TABWIDGET_H

#include <QTabWidget>
#include <QTimerEvent>
#include <QKeyEvent>
#include "ui/tabbar.h"

namespace Texxy {

class TabWidget : public QTabWidget {
    Q_OBJECT

   public:
    TabWidget(QWidget* parent = nullptr);
    ~TabWidget();
    /* make it public */
    TabBar* tabBar() const { return tb_; }
    QWidget* getLastActiveTab();
    void removeTab(int index);
    void selectLastActiveTab();

    void noTabDND() { tb_->noTabDND(); }

   signals:
    void currentTabChanged(int curIndx);
    void hasLastActiveTab(bool hasLastActive);

   protected:
    void timerEvent(QTimerEvent* event);
    void keyPressEvent(QKeyEvent* event);

   private slots:
    void tabSwitch(int index);

   private:
    TabBar* tb_;
    int timerId_;
    int curIndx_;
    /* This is the list of activated tabs, in the order of activation,
       and is used for finding the last active tab: */
    QList<QWidget*> activatedTabs_;
};

}  // namespace Texxy

#endif  // TABWIDGET_H
