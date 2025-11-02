/*
 * texxy/sidepane.h
 */

#ifndef SIDEPANE_H
#define SIDEPANE_H

#include <QEvent>
#include <QCollator>
#include <QLabel>
#include <QListWidget>
#include <QTabWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QToolButton>
#include <QResizeEvent>
#include "ui/lineedit.h"

namespace Texxy {

/* For having control over sorting. */
class ListWidgetItem : public QListWidgetItem {
   public:
    ListWidgetItem(const QIcon& icon,
                   const QString& text,
                   QListWidget* parent = nullptr,
                   int type = QListWidgetItem::Type)
        : QListWidgetItem(icon, text, parent, type) {
        collator_.setNumericMode(true);
    }
    ListWidgetItem(const QString& text, QListWidget* parent = nullptr, int type = QListWidgetItem::Type)
        : QListWidgetItem(text, parent, type) {
        collator_.setNumericMode(true);
    }

    bool operator<(const QListWidgetItem& other) const override;

   private:
    QCollator collator_;
};

class ListWidget : public QListWidget {
    Q_OBJECT
   public:
    ListWidget(QWidget* parent = nullptr);

    QListWidgetItem* getItemFromIndex(const QModelIndex& index) const;

    void scrollToCurrentItem();

    void lockListWidget(bool lock) { locked_ = lock; }

   signals:
    void closeItem(QListWidgetItem* item);
    void closeSidePane();
    void currentItemUpdated(QListWidgetItem* current);
    void rowsAreInserted(int start, int end);

   protected:
    QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& index,
                                                         const QEvent* event = nullptr) const override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;  // for instant tooltips

   protected slots:
    void rowsInserted(const QModelIndex& parent, int start, int end) override;

   private:
    bool locked_;
};

class SidePane : public QWidget {
    Q_OBJECT
   public:
    SidePane(QWidget* parent = nullptr);
    ~SidePane() override;

    ListWidget* listWidget() const { return lw_; }

    void lockPane(bool lock);

    // Files tab API
    void setProjectRoot(const QString& path);  // set the root directory shown in Files tab
    void revealFile(const QString& path);      // select & reveal a file in the tree

   signals:
    // Emitted when the user requests to open a file (or many; emitted once per file)
    void openFileRequested(const QString& path);

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

   private slots:
    // Open tab (existing behavior)
    void filter(const QString&);
    void reallyApplyFilter();
    void onRowsInserted(int start, int end);

    // Files tab
    void onTreeActivated(const QModelIndex& proxyIndex);
    void onTreeContextMenuRequested(const QPoint& pos);

   private:
    // Open tab widgets
    ListWidget* lw_ = nullptr;
    LineEdit* le_ = nullptr;
    QTimer* filterTimer_ = nullptr;

    // Container
    QTabWidget* tabs_ = nullptr;

    // Files tab widgets
    QWidget* fileTab_ = nullptr;
    QWidget* fileControls_ = nullptr;
    QLabel* rootLabel_ = nullptr;
    QToolButton* upButton_ = nullptr;
    QToolButton* homeButton_ = nullptr;
    QToolButton* currentButton_ = nullptr;
    QToolButton* refreshButton_ = nullptr;
    QTreeView* tree_ = nullptr;
    LineEdit* fileFilter_ = nullptr;
    QFileSystemModel* fsModel_ = nullptr;
    QSortFilterProxyModel* proxy_ = nullptr;
    QString lastOpenedFile_;

    void updateRootWidgets();
    void navigateRootUp();
    void goHome();
    void revealLastOpened();
    void refreshModel();
};

}  // namespace Texxy

#endif  // SIDEPANE_H
