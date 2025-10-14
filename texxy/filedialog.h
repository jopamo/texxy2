/*
  texxy/filedialog.h
*/

#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QFileDialog>
#include <QGuiApplication>
#include <QShortcut>
#include <QTimer>
#include <QTreeView>

namespace Texxy {

class FileDialog : public QFileDialog {
    Q_OBJECT
   public:
    explicit FileDialog(QWidget* parent, bool isNative = false)
        : QFileDialog(parent), tView(nullptr), p(parent), native(isNative) {
        setWindowModality(Qt::WindowModal);
        setViewMode(QFileDialog::Detail);
        if (!native) {
            setOption(QFileDialog::DontUseNativeDialog, true);
            if (showHidden_)
                setFilter(filter() | QDir::Hidden);
            auto* toggleHidden0 = new QShortcut(QKeySequence(tr("Ctrl+H", "Toggle showing hidden files")), this);
            auto* toggleHidden1 = new QShortcut(QKeySequence(tr("Alt+.", "Toggle showing hidden files")), this);
            connect(toggleHidden0, &QShortcut::activated, this, &FileDialog::toggleHidden);
            connect(toggleHidden1, &QShortcut::activated, this, &FileDialog::toggleHidden);
        }
    }

    ~FileDialog() override { showHidden_ = (filter() & QDir::Hidden); }

    void autoScroll() {
        if (native)
            return;
        tView = findChild<QTreeView*>("treeView");
        if (tView && tView->model())
            connect(tView->model(), &QAbstractItemModel::layoutChanged, this, &FileDialog::scrollToSelection,
                    Qt::UniqueConnection);
    }

   protected:
    void showEvent(QShowEvent* event) override {
        const auto platform = QGuiApplication::platformName();
        if (p && !native && platform != QStringLiteral("wayland"))
            QTimer::singleShot(0, this, &FileDialog::center);
        QFileDialog::showEvent(event);
    }

   private slots:
    void scrollToSelection() {
        if (!tView || !tView->selectionModel())
            return;
        // prefer selectedRows so we only get one index per row
        const auto rows = tView->selectionModel()->selectedRows();
        if (!rows.isEmpty())
            tView->scrollTo(rows.first(), QAbstractItemView::PositionAtCenter);
    }

    void center() { move(p->x() + p->width() / 2 - width() / 2, p->y() + p->height() / 2 - height() / 2); }

    void toggleHidden() {
        const bool nowHidden = (filter() & QDir::Hidden);
        showHidden_ = !nowHidden;
        setFilter(showHidden_ ? (filter() | QDir::Hidden) : (filter() & ~QDir::Hidden));
    }

   private:
    Q_DISABLE_COPY_MOVE(FileDialog)

    inline static bool showHidden_ = false;  // remember per-process without a global
    QTreeView* tView;
    QWidget* p;
    bool native;
};

}  // namespace Texxy

#endif  // FILEDIALOG_H
