// src/ui/ui/sidepane.cpp
/*
 * texxy/sidepane.cpp
 */

#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QToolTip>
#include <QScrollBar>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QHeaderView>
#include <QFileInfo>
#include <QMenu>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QUrl>
#include <QDir>
#include <algorithm>

#include "ui/sidepane.h"
#include "svgicons.h"

namespace Texxy {

bool ListWidgetItem::operator<(const QListWidgetItem& other) const {
    // treat dot as a separator for a natural sorting
    const QString txt1 = text();
    const QString txt2 = other.text();
    int start1 = 0, start2 = 0;
    for (;;) {
        const int end1 = txt1.indexOf(QLatin1Char('.'), start1);
        const int end2 = txt2.indexOf(QLatin1Char('.'), start2);
        const QString part1 = end1 == -1 ? txt1.sliced(start1) : txt1.sliced(start1, end1 - start1);
        const QString part2 = end2 == -1 ? txt2.sliced(start2) : txt2.sliced(start2, end2 - start2);
        int comp = collator_.compare(part1, part2);
        if (comp == 0)
            comp = part1.size() - part2.size();  // workaround for QCollator edge case
        if (comp != 0)
            return comp < 0;
        if (end1 == -1 || end2 == -1)
            return end1 < end2;
        start1 = end1 + 1;
        start2 = end2 + 1;
    }
}

/*************************/
ListWidget::ListWidget(QWidget* parent) : QListWidget(parent), locked_(false) {
    setAutoScroll(false);                                      // -> ListWidget::scrollToCurrentItem
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);  // smooth scroll
    setMouseTracking(true);                                    // for instant tooltips

    // try to have a current item as far as possible and announce it
    connect(this, &QListWidget::currentItemChanged, [this](QListWidgetItem* current, QListWidgetItem* previous) {
        if (current == previous)
            return;
        if (current == nullptr) {
            if (count() == 0)
                return;
            if (previous != nullptr) {
                const int prevRow = row(previous);
                if (prevRow < count() - 1)
                    setCurrentRow(prevRow + 1);
                else if (prevRow != 0)
                    setCurrentRow(0);
            }
            else {
                setCurrentRow(0);
            }
        }
        else {
            emit currentItemUpdated(current);
            scrollToCurrentItem();
            // with filtering, Qt may give focus to a hidden item but select a visible one
            QTimer::singleShot(0, this, [this]() {
                const auto index = currentIndex();
                if (index.isValid())
                    selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
            });
        }
    });
}

/*************************/
// prevent deselection by Ctrl+LeftClick; see qabstractitemview.cpp
QItemSelectionModel::SelectionFlags ListWidget::selectionCommand(const QModelIndex& index, const QEvent* event) const {
    Qt::KeyboardModifiers keyModifiers = event != nullptr && event->isInputEvent()
                                             ? static_cast<const QInputEvent*>(event)->modifiers()
                                             : Qt::NoModifier;

    if (selectionMode() == QAbstractItemView::SingleSelection) {
        if (!index.isValid())
            return QItemSelectionModel::NoUpdate;
        if ((keyModifiers & Qt::ControlModifier) && selectionModel()->isSelected(index))
            return QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows;
    }
    return QListWidget::selectionCommand(index, event);
}

/*************************/
// QListView::scrollTo has issues with hidden / variable-height items under per-item mode
void ListWidget::scrollToCurrentItem() {
    const QModelIndex index = currentIndex();
    if (!index.isValid())
        return;
    const QRect rect = visualRect(index);
    const QRect area = viewport()->rect();
    if (rect.isEmpty())
        return;

    const bool above = rect.top() < area.top();
    const bool below = rect.bottom() > area.bottom();
    if (!above && !below)
        return;

    int verticalValue = verticalScrollBar()->value();
    const QRect r = rect.adjusted(-spacing(), -spacing(), spacing(), spacing());
    if (above)
        verticalValue += r.top();
    else
        verticalValue += qMin(r.top(), r.bottom() - area.height() + 1);
    verticalScrollBar()->setValue(verticalValue);
}

/*************************/
void ListWidget::mousePressEvent(QMouseEvent* event) {
    if (locked_) {
        event->ignore();
        return;
    }
    if (selectionMode() == QAbstractItemView::SingleSelection) {
        if (event->button() == Qt::MiddleButton) {
            const QModelIndex index = indexAt(event->position().toPoint());
            if (QListWidgetItem* item = itemFromIndex(index))
                emit closeItem(item);
            else
                emit closeSidePane();
            return;
        }
        else if (event->button() == Qt::RightButton) {
            return;
        }
    }
    QListWidget::mousePressEvent(event);
}

/*************************/
void ListWidget::mouseMoveEvent(QMouseEvent* event) {
    QListWidget::mouseMoveEvent(event);
    if (event->button() == Qt::NoButton && !(event->buttons() & Qt::LeftButton)) {
        if (QListWidgetItem* item = itemAt(event->position().toPoint()))
            QToolTip::showText(event->globalPosition().toPoint(), item->toolTip(), this);
        else
            QToolTip::hideText();
    }
}

/*************************/
QListWidgetItem* ListWidget::getItemFromIndex(const QModelIndex& index) const {
    return itemFromIndex(index);
}

/*************************/
void ListWidget::rowsInserted(const QModelIndex& parent, int start, int end) {
    emit rowsAreInserted(start, end);
    QListView::rowsInserted(parent, start, end);
}

/*************************/
SidePane::SidePane(QWidget* parent) : QWidget(parent) {
    // --- container tabs ---
    tabs_ = new QTabWidget(this);

    // --- Tab 1: Open ---
    QWidget* listTab = new QWidget(this);
    auto* listGrid = new QGridLayout(listTab);
    listGrid->setVerticalSpacing(4);
    listGrid->setContentsMargins(0, 0, 0, 0);

    lw_ = new ListWidget(listTab);
    lw_->setSortingEnabled(true);
    lw_->setSelectionMode(QAbstractItemView::SingleSelection);
    lw_->setContextMenuPolicy(Qt::CustomContextMenu);
    listGrid->addWidget(lw_, 0, 0);

    le_ = new LineEdit(listTab);
    le_->setPlaceholderText(tr("Filter..."));
    listGrid->addWidget(le_, 1, 0);

    tabs_->addTab(listTab, tr("Open"));

    // --- Tab 2: Files ---
    fileTab_ = new QWidget(this);
    auto* filesGrid = new QGridLayout(fileTab_);
    filesGrid->setVerticalSpacing(4);
    filesGrid->setContentsMargins(0, 0, 0, 0);

    fileControls_ = new QWidget(fileTab_);
    auto* controlsLayout = new QHBoxLayout(fileControls_);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(4);

    rootLabel_ = new QLabel(fileControls_);
    rootLabel_->setObjectName(QStringLiteral("rootLabel"));
    rootLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rootLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    upButton_ = new QToolButton(fileControls_);
    upButton_->setIcon(
        QIcon::fromTheme(QStringLiteral("go-up"), symbolicIcon::icon(QStringLiteral(":icons/go-up.svg"))));
    upButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    upButton_->setAutoRaise(true);
    upButton_->setToolTip(tr("Go to parent directory"));

    homeButton_ = new QToolButton(fileControls_);
    homeButton_->setIcon(
        QIcon::fromTheme(QStringLiteral("go-home"), symbolicIcon::icon(QStringLiteral(":icons/texxy.svg"))));
    homeButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    homeButton_->setAutoRaise(true);
    homeButton_->setToolTip(tr("Go to home directory"));

    currentButton_ = new QToolButton(fileControls_);
    currentButton_->setIcon(
        QIcon::fromTheme(QStringLiteral("go-jump"), symbolicIcon::icon(QStringLiteral(":icons/go-jump.svg"))));
    currentButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    currentButton_->setAutoRaise(true);
    currentButton_->setToolTip(tr("Reveal current file"));

    refreshButton_ = new QToolButton(fileControls_);
    refreshButton_->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh"),
                                             symbolicIcon::icon(QStringLiteral(":icons/view-refresh.svg"))));
    refreshButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    refreshButton_->setAutoRaise(true);
    refreshButton_->setToolTip(tr("Refresh file list"));

    controlsLayout->addWidget(rootLabel_, /*stretch*/ 1);
    controlsLayout->addWidget(upButton_);
    controlsLayout->addWidget(homeButton_);
    controlsLayout->addWidget(currentButton_);
    controlsLayout->addWidget(refreshButton_);

    filesGrid->addWidget(fileControls_, 0, 0);

    fsModel_ = new QFileSystemModel(fileTab_);
    fsModel_->setResolveSymlinks(true);
    fsModel_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);

    proxy_ = new QSortFilterProxyModel(fileTab_);
    proxy_->setRecursiveFilteringEnabled(true);
    proxy_->setSourceModel(fsModel_);
    proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);

    tree_ = new QTreeView(fileTab_);
    tree_->setModel(proxy_);
    tree_->setUniformRowHeights(true);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);  // multi-select
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->header()->setStretchLastSection(true);
    // columns: 0=Name, 1=Size, 2=Type, 3=Modified
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    filesGrid->addWidget(tree_, 1, 0);

    fileFilter_ = new LineEdit(fileTab_);
    fileFilter_->setPlaceholderText(tr("Filter files..."));
    filesGrid->addWidget(fileFilter_, 2, 0);

    tabs_->addTab(fileTab_, tr("Files"));

    // host layout
    auto* mainGrid = new QGridLayout(this);
    mainGrid->setContentsMargins(0, 0, 0, 0);
    mainGrid->addWidget(tabs_, 0, 0);
    setLayout(mainGrid);

    // wiring: Open tab
    connect(le_, &QLineEdit::textChanged, this, &SidePane::filter);
    connect(lw_, &ListWidget::rowsAreInserted, this, &SidePane::onRowsInserted);
    connect(lw_, &ListWidget::itemChanged, [this](QListWidgetItem* item) {
        item->setHidden(!item->text().contains(le_->text(), Qt::CaseInsensitive));
    });
    lw_->installEventFilter(this);

    // wiring: Files tab filtering
    connect(fileFilter_, &QLineEdit::textChanged, [this](const QString& t) {
        QRegularExpression rx(QRegularExpression::escape(t), QRegularExpression::CaseInsensitiveOption);
        proxy_->setFilterRegularExpression(rx);
        if (tree_->rootIndex().isValid())
            tree_->expandToDepth(0);
    });
    connect(tree_, &QTreeView::doubleClicked, this, &SidePane::onTreeActivated);
    connect(tree_, &QTreeView::activated, this, [this](const QModelIndex& idx) {
        const QModelIndex src = proxy_->mapToSource(idx);
        if (src.isValid() && fsModel_->isDir(src))
            tree_->setExpanded(idx, !tree_->isExpanded(idx));  // only toggle directories
    });
    connect(tree_, &QTreeView::customContextMenuRequested, this, &SidePane::onTreeContextMenuRequested);
    connect(upButton_, &QAbstractButton::clicked, this, [this] { navigateRootUp(); });
    connect(homeButton_, &QAbstractButton::clicked, this, [this] { goHome(); });
    connect(currentButton_, &QAbstractButton::clicked, this, [this] { revealLastOpened(); });
    connect(refreshButton_, &QAbstractButton::clicked, this, [this] { refreshModel(); });

    // keyboard: Enter opens all selected
    tree_->installEventFilter(this);

    // >>> CRUCIAL: set a default root so filenames appear
    const QString defaultRoot = QDir::homePath();
    const QModelIndex srcRoot = fsModel_->setRootPath(defaultRoot);
    const QModelIndex proxyRoot = proxy_->mapFromSource(srcRoot);
    tree_->setRootIndex(proxyRoot);
    tree_->expand(proxyRoot);
    updateRootWidgets();

    // Optional: show only the Name column if you prefer a cleaner list
    tree_->setColumnHidden(1, true);
    tree_->setColumnHidden(2, true);
    tree_->setColumnHidden(3, true);

    // Make sure the first directory expansion happens after the model loads
    connect(fsModel_, &QFileSystemModel::directoryLoaded, this, [this](const QString& p) {
        const QModelIndex src = fsModel_->index(p);
        if (!src.isValid())
            return;
        const QModelIndex pr = proxy_->mapFromSource(src);
        if (pr.isValid())
            tree_->expand(pr);
        updateRootWidgets();
    });
}

/*************************/
SidePane::~SidePane() {
    if (filterTimer_) {
        disconnect(filterTimer_, &QTimer::timeout, this, &SidePane::reallyApplyFilter);
        filterTimer_->stop();
        delete filterTimer_;
        filterTimer_ = nullptr;
    }
}

/*************************/
void SidePane::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateRootWidgets();
}

/*************************/
bool SidePane::eventFilter(QObject* watched, QEvent* event) {
    // Open tab: type to filter
    if (watched == lw_ && event->type() == QEvent::KeyPress) {
        if (QKeyEvent* ke = static_cast<QKeyEvent*>(event)) {
            if (ke->key() < Qt::Key_Home || ke->key() > Qt::Key_PageDown) {
                le_->pressKey(ke);
                return true;  // don't change the selection
            }
        }
    }

    // Files tab: Enter opens selected files; Space toggles folders
    if (watched == tree_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            const auto sel = tree_->selectionModel()->selectedIndexes();
            QSet<QString> opened;
            for (const QModelIndex& idx : sel) {
                if (idx.column() != 0)
                    continue;  // process only the Name column
                const QModelIndex src = proxy_->mapToSource(idx);
                if (!src.isValid())
                    continue;
                if (!fsModel_->isDir(src)) {
                    const QString path = fsModel_->filePath(src);
                    if (!opened.contains(path)) {
                        opened.insert(path);
                        emit openFileRequested(path);
                    }
                }
            }
            return true;
        }
        if (ke->key() == Qt::Key_Space) {
            const QModelIndex idx = tree_->currentIndex();
            if (idx.isValid())
                tree_->setExpanded(idx, !tree_->isExpanded(idx));
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

/*************************/
void SidePane::filter(const QString& /*text*/) {
    if (!filterTimer_) {
        filterTimer_ = new QTimer(this);
        filterTimer_->setSingleShot(true);
        connect(filterTimer_, &QTimer::timeout, this, &SidePane::reallyApplyFilter);
    }
    filterTimer_->start(200);
}

/*************************/
// Simply hide items that don't contain the filter string
// Clearing items isn't an option because they are tracked
// for their correspondence with tab pages
void SidePane::reallyApplyFilter() {
    for (int i = lw_->count() - 1; i >= 0; --i) {  // end->start keeps scrollbar sane
        QListWidgetItem* wi = lw_->item(i);
        wi->setHidden(!wi->text().contains(le_->text(), Qt::CaseInsensitive));
    }
    lw_->scrollToCurrentItem();
}

/*************************/
void SidePane::onRowsInserted(int start, int end) {
    for (int i = start; i <= end; ++i) {
        if (lw_->item(i) && !lw_->item(i)->text().contains(le_->text(), Qt::CaseInsensitive))
            lw_->item(i)->setHidden(true);
    }
}

/*************************/
void SidePane::lockPane(bool lock) {
    lw_->lockListWidget(lock);
    le_->setEnabled(!lock);
    if (tree_)
        tree_->setEnabled(!lock);
    if (fileFilter_)
        fileFilter_->setEnabled(!lock);
    if (fileControls_)
        fileControls_->setEnabled(!lock);
    updateRootWidgets();
}

/*************************/
void SidePane::setProjectRoot(const QString& path) {
    if (!fsModel_ || !proxy_ || !tree_)
        return;

    QFileInfo fi(path);
    QString root = path;
    if (fi.exists()) {
        root = fi.isDir() ? fi.canonicalFilePath() : fi.absolutePath();
    }
    if (root.isEmpty())
        root = QDir::homePath();
    root = QDir(root).absolutePath();

    if (QDir::cleanPath(fsModel_->rootPath()) == QDir::cleanPath(root)) {
        updateRootWidgets();
        return;
    }

    const QModelIndex srcRoot = fsModel_->setRootPath(root);
    const QModelIndex proxyRoot = proxy_->mapFromSource(srcRoot);
    tree_->setRootIndex(proxyRoot);
    tree_->expand(proxyRoot);
    tree_->scrollToTop();
    updateRootWidgets();
}

/*************************/
void SidePane::revealFile(const QString& path) {
    lastOpenedFile_ = path;
    if (!fsModel_ || !proxy_ || !tree_)
        return;

    const QFileInfo fi(path);
    if (!fi.exists()) {
        updateRootWidgets();
        return;
    }

    QString canonical = fi.canonicalFilePath();
    if (canonical.isEmpty())
        canonical = fi.absoluteFilePath();

    const QString rootPath = QDir::cleanPath(fsModel_->rootPath());
    if (rootPath.isEmpty()) {
        setProjectRoot(fi.absolutePath());
    }
    else {
        const QString rel = QDir(rootPath).relativeFilePath(canonical);
        if (rel.startsWith(QLatin1String("..")))
            setProjectRoot(fi.absolutePath());
    }

    const QModelIndex src = fsModel_->index(canonical);
    if (!src.isValid())
        return;
    const QModelIndex idx = proxy_->mapFromSource(src);
    if (!idx.isValid())
        return;
    tree_->expand(proxy_->parent(idx));
    tree_->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    tree_->setCurrentIndex(idx);
    updateRootWidgets();
}

void SidePane::updateRootWidgets() {
    if (!rootLabel_ || !fsModel_)
        return;

    const QString rootPath = QDir::cleanPath(fsModel_->rootPath());
    QString display = rootPath;
    if (display.isEmpty())
        display = tr("No directory selected");

    rootLabel_->setToolTip(display);
    const int labelWidth = rootLabel_->width() > 0 ? rootLabel_->width() : width();
    const QString elided =
        rootLabel_->fontMetrics().elidedText(display, Qt::ElideMiddle, std::max(labelWidth - 12, 80));
    rootLabel_->setText(elided);

    const bool hasRoot = !rootPath.isEmpty();
    bool canGoUp = false;
    if (hasRoot) {
        QDir dir(rootPath);
        canGoUp = dir.cdUp();
    }

    const bool controlsEnabled = !fileControls_ || fileControls_->isEnabled();
    if (upButton_)
        upButton_->setEnabled(hasRoot && canGoUp && controlsEnabled);
    if (homeButton_)
        homeButton_->setEnabled(controlsEnabled);
    if (currentButton_)
        currentButton_->setEnabled(!lastOpenedFile_.isEmpty() && controlsEnabled);
    if (refreshButton_)
        refreshButton_->setEnabled(hasRoot && controlsEnabled);
}

/*************************/
void SidePane::navigateRootUp() {
    if (!fsModel_)
        return;
    const QString currentRoot = QDir::cleanPath(fsModel_->rootPath());
    if (currentRoot.isEmpty())
        return;
    QDir dir(currentRoot);
    if (!dir.cdUp())
        return;
    setProjectRoot(dir.absolutePath());
}

/*************************/
void SidePane::goHome() {
    setProjectRoot(QDir::homePath());
}

/*************************/
void SidePane::revealLastOpened() {
    if (lastOpenedFile_.isEmpty())
        return;
    revealFile(lastOpenedFile_);
}

/*************************/
void SidePane::refreshModel() {
    if (!fsModel_)
        return;
    const QString root = fsModel_->rootPath();
    if (!tree_) {
        if (!root.isEmpty())
            fsModel_->setRootPath(root);
        updateRootWidgets();
        return;
    }

    tree_->setUpdatesEnabled(false);
    if (!root.isEmpty())
        fsModel_->setRootPath(QString());
    const QModelIndex srcRoot = fsModel_->setRootPath(root);
    if (proxy_) {
        const QModelIndex proxyRoot = proxy_->mapFromSource(srcRoot);
        tree_->setRootIndex(proxyRoot);
        tree_->expand(proxyRoot);
    }
    tree_->setUpdatesEnabled(true);
    updateRootWidgets();
}

/*************************/
void SidePane::onTreeActivated(const QModelIndex& proxyIndex) {
    const QModelIndex src = proxy_->mapToSource(proxyIndex);
    if (!src.isValid())
        return;
    if (fsModel_->isDir(src)) {
        // (doubleClicked on a dir will still toggle here; harmless)
        tree_->setExpanded(proxyIndex, !tree_->isExpanded(proxyIndex));
    }
    else {
        emit openFileRequested(fsModel_->filePath(src));  // opens exactly once now
    }
}

/*************************/
void SidePane::onTreeContextMenuRequested(const QPoint& pos) {
    const QModelIndex idx = tree_->indexAt(pos);
    if (!idx.isValid())
        return;
    const QModelIndex src = proxy_->mapToSource(idx);
    const QString path = fsModel_->filePath(src);
    const bool isDir = fsModel_->isDir(src);

    QMenu menu(this);
    QAction* openAct = menu.addAction(tr("Open"));
    QAction* rootAct = isDir ? menu.addAction(tr("Set As Root")) : nullptr;
    QAction* revealAct = menu.addAction(tr("Reveal In File Manager"));

    QAction* chosen = menu.exec(tree_->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == openAct && !isDir) {
        emit openFileRequested(path);
    }
    else if (chosen == rootAct) {
        setProjectRoot(path);
    }
    else if (chosen == revealAct) {
        const QString toOpen = isDir ? path : QFileInfo(path).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(toOpen));
    }
}

}  // namespace Texxy
