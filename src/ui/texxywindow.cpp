// src/ui/texxywindow.cpp
/*
 * texxywindow.cpp
 */

#include "texxy_ui_prelude.h"
#include "ui_about.h"

namespace Texxy {

TexxyWindow::TexxyWindow(QWidget* parent) : QMainWindow(parent), dummyWidget(nullptr), ui(new Ui::TexxyWindow) {
    ui->setupUi(this);

    locked_ = false;
    shownBefore_ = false;
    closePreviousPages_ = false;
    loadingProcesses_ = 0;
    rightClicked_ = -1;

    autoSaver_ = nullptr;
    autoSaverRemainingTime_ = -1;
    inactiveTabModified_ = false;

    sidePane_ = nullptr;

    // jump to bar
    ui->spinBox->hide();
    ui->label->hide();
    ui->checkBox->hide();

    // status bar
    auto* statusLabel = new QLabel();
    statusLabel->setObjectName("statusLabel");
    statusLabel->setIndent(2);
    statusLabel->setMinimumWidth(100);
    statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* wordButton = new QToolButton();
    wordButton->setObjectName("wordButton");
    wordButton->setFocusPolicy(Qt::NoFocus);
    wordButton->setAutoRaise(true);
    wordButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    wordButton->setIconSize(QSize(16, 16));
    wordButton->setIcon(symbolicIcon::icon(":icons/view-refresh.svg"));
    wordButton->setToolTip(u"<p style='white-space:pre'>" + tr("Calculate number of words") + u"</p>");
    connect(wordButton, &QAbstractButton::clicked, [this] { updateWordInfo(); });
    ui->statusBar->addWidget(statusLabel);
    ui->statusBar->addWidget(wordButton);

    // text unlocking
    ui->actionEdit->setVisible(false);

    ui->actionRun->setVisible(false);

    // replace dock
    QWidget::setTabOrder(ui->lineEditFind, ui->lineEditReplace);
    QWidget::setTabOrder(ui->lineEditReplace, ui->toolButtonNext);
    // tooltips are set here for easier translation
    ui->toolButtonNext->setToolTip(tr("Next") + u" (" + QKeySequence(Qt::Key_F8).toString(QKeySequence::NativeText) +
                                   u")");
    ui->toolButtonPrv->setToolTip(tr("Previous") + u" (" + QKeySequence(Qt::Key_F9).toString(QKeySequence::NativeText) +
                                  u")");
    ui->toolButtonAll->setToolTip(tr("Replace all") + u" (" +
                                  QKeySequence(Qt::Key_F10).toString(QKeySequence::NativeText) + u")");
    ui->dockReplace->setVisible(false);

    // shortcuts should be reversed for rtl
    if (QApplication::layoutDirection() == Qt::RightToLeft) {
        ui->actionRightTab->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
        ui->actionLeftTab->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    }

    // get the default customizable shortcuts before any change
    static const QStringList excluded = {QStringLiteral("actionCut"), QStringLiteral("actionCopy"),
                                         QStringLiteral("actionPaste"), QStringLiteral("actionSelectAll")};
    const auto allMenus = ui->menuBar->findChildren<QMenu*>();
    for (const auto* thisMenu : allMenus) {
        const auto menuActions = thisMenu->actions();
        for (auto* menuAction : menuActions) {
            const QKeySequence seq = menuAction->shortcut();
            if (!seq.isEmpty() && !excluded.contains(menuAction->objectName()))
                defaultShortcuts_.insert(menuAction, seq);
        }
    }
    // exceptions
    defaultShortcuts_.insert(ui->actionSaveAllFiles, QKeySequence());
    defaultShortcuts_.insert(ui->actionSoftTab, QKeySequence());
    defaultShortcuts_.insert(ui->actionStartCase, QKeySequence());
    defaultShortcuts_.insert(ui->actionFont, QKeySequence());

    applyConfigOnStarting();

    auto* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->mainToolBar->insertWidget(ui->actionMenu, spacer);
    auto* menu = new QMenu(ui->mainToolBar);
    menu->addMenu(ui->menuFile);
    menu->addMenu(ui->menuEdit);
    menu->addMenu(ui->menuOptions);
    menu->addMenu(ui->menuSearch);
    menu->addMenu(ui->menuHelp);
    ui->actionMenu->setMenu(menu);
    const auto tbList = ui->mainToolBar->findChildren<QToolButton*>();
    if (!tbList.isEmpty())
        tbList.at(tbList.count() - 1)->setPopupMode(QToolButton::InstantPopup);

    newTab();

    aGroup_ = new QActionGroup(this);
    ui->actionUTF_8->setActionGroup(aGroup_);
    ui->actionUTF_16->setActionGroup(aGroup_);
    ui->actionISO_8859_1->setActionGroup(aGroup_);
    ui->actionOther->setActionGroup(aGroup_);

    ui->actionUTF_8->setChecked(true);
    ui->actionOther->setDisabled(true);

    if (static_cast<TexxyApplication*>(qApp)->isStandAlone())
        ui->tabWidget->noTabDND();

    connect(ui->actionQuit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionNew, &QAction::triggered, this, &TexxyWindow::newTab);
    connect(ui->tabWidget->tabBar(), &TabBar::addEmptyTab, this, &TexxyWindow::newTab);
    connect(ui->actionDetachTab, &QAction::triggered, this, &TexxyWindow::detachTab);
    connect(ui->actionRightTab, &QAction::triggered, this, &TexxyWindow::nextTab);
    connect(ui->actionLeftTab, &QAction::triggered, this, &TexxyWindow::previousTab);
    connect(ui->actionLastActiveTab, &QAction::triggered, this, &TexxyWindow::lastActiveTab);
    connect(ui->actionClose, &QAction::triggered, this, &TexxyWindow::closePage);
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &TexxyWindow::closeTabAtIndex);
    connect(ui->actionOpen, &QAction::triggered, this, &TexxyWindow::fileOpen);
    connect(ui->actionReload, &QAction::triggered, this, &TexxyWindow::reload);
    connect(aGroup_, &QActionGroup::triggered, this, &TexxyWindow::enforceEncoding);
    connect(ui->actionSave, &QAction::triggered, this, [this] { saveFile(false); });
    connect(ui->actionSaveAs, &QAction::triggered, this, [this] { saveFile(false); });
    connect(ui->actionSaveCodec, &QAction::triggered, this, [this] { saveFile(false); });
    connect(ui->actionSaveAllFiles, &QAction::triggered, this, [this] { saveAllFiles(true); });

    connect(ui->actionCut, &QAction::triggered, this, &TexxyWindow::cutText);
    connect(ui->actionCopy, &QAction::triggered, this, &TexxyWindow::copyText);
    connect(ui->actionPaste, &QAction::triggered, this, &TexxyWindow::pasteText);
    connect(ui->actionSoftTab, &QAction::triggered, this, &TexxyWindow::toSoftTabs);
    connect(ui->actionDate, &QAction::triggered, this, &TexxyWindow::insertDate);
    connect(ui->actionDelete, &QAction::triggered, this, &TexxyWindow::deleteText);
    connect(ui->actionSelectAll, &QAction::triggered, this, &TexxyWindow::selectAllText);

    connect(ui->actionUpperCase, &QAction::triggered, this, &TexxyWindow::upperCase);
    connect(ui->actionLowerCase, &QAction::triggered, this, &TexxyWindow::lowerCase);
    connect(ui->actionStartCase, &QAction::triggered, this, &TexxyWindow::startCase);

    connect(ui->menuEdit, &QMenu::aboutToShow, this, &TexxyWindow::showingEditMenu);
    connect(ui->menuEdit, &QMenu::aboutToHide, this, &TexxyWindow::hidngEditMenu);

    connect(ui->actionSortLines, &QAction::triggered, this, &TexxyWindow::sortLines);
    connect(ui->actionRSortLines, &QAction::triggered, this, &TexxyWindow::sortLines);

    connect(ui->ActionRmDupeSort, &QAction::triggered, this, &TexxyWindow::rmDupeSort);
    connect(ui->ActionRmDupeRSort, &QAction::triggered, this, &TexxyWindow::rmDupeSort);

    connect(ui->ActionSpaceDupeSort, &QAction::triggered, this, &TexxyWindow::spaceDupeSort);
    connect(ui->ActionSpaceDupeRSort, &QAction::triggered, this, &TexxyWindow::spaceDupeSort);

    connect(ui->actionEdit, &QAction::triggered, this, &TexxyWindow::makeEditable);

    connect(ui->actionSession, &QAction::triggered, this, &TexxyWindow::manageSessions);

    connect(ui->actionRun, &QAction::triggered, this, &TexxyWindow::executeProcess);

    connect(ui->actionUndo, &QAction::triggered, this, &TexxyWindow::undoing);
    connect(ui->actionRedo, &QAction::triggered, this, &TexxyWindow::redoing);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &TexxyWindow::onTabChanged);
    connect(ui->tabWidget, &TabWidget::currentTabChanged, this, &TexxyWindow::tabSwitch);
    connect(ui->tabWidget, &TabWidget::hasLastActiveTab,
            [this](bool hasLastActive) { ui->actionLastActiveTab->setEnabled(hasLastActive); });

    // the tab will be detached after the DND is finished
    connect(ui->tabWidget->tabBar(), &TabBar::tabDetached, this, &TexxyWindow::detachTab, Qt::QueuedConnection);

    connect(ui->tabWidget->tabBar(), &TabBar::hideTabBar, this, &TexxyWindow::toggleSidePane);
    ui->tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tabWidget->tabBar(), &QWidget::customContextMenuRequested, this, &TexxyWindow::tabContextMenu);
    connect(ui->actionCopyName, &QAction::triggered, this, &TexxyWindow::copyTabFileName);
    connect(ui->actionCopyPath, &QAction::triggered, this, &TexxyWindow::copyTabFilePath);
    connect(ui->actionCloseAll, &QAction::triggered, this, &TexxyWindow::closeAllPages);
    connect(ui->actionCloseRight, &QAction::triggered, this, &TexxyWindow::closeNextPages);
    connect(ui->actionCloseLeft, &QAction::triggered, this, &TexxyWindow::closePreviousPages);
    connect(ui->actionCloseOther, &QAction::triggered, this, &TexxyWindow::closeOtherPages);

    connect(ui->actionFont, &QAction::triggered, this, &TexxyWindow::fontDialog);

    connect(ui->actionFind, &QAction::triggered, this, &TexxyWindow::showHideSearch);
    connect(ui->actionJump, &QAction::triggered, this, &TexxyWindow::jumpTo);
    connect(ui->spinBox, &QAbstractSpinBox::editingFinished, this, &TexxyWindow::goTo);

    connect(ui->actionLineNumbers, &QAction::toggled, this, &TexxyWindow::showLN);
    connect(ui->actionWrap, &QAction::triggered, this, &TexxyWindow::toggleWrapping);
    connect(ui->actionSyntax, &QAction::triggered, this, &TexxyWindow::toggleSyntaxHighlighting);
    connect(ui->actionIndent, &QAction::triggered, this, &TexxyWindow::toggleIndent);

    connect(ui->actionPreferences, &QAction::triggered, this, &TexxyWindow::prefDialog);

    connect(ui->actionReplace, &QAction::triggered, this, &TexxyWindow::replaceDock);
    connect(ui->toolButtonNext, &QAbstractButton::clicked, this, &TexxyWindow::replace);
    connect(ui->toolButtonPrv, &QAbstractButton::clicked, this, &TexxyWindow::replace);
    connect(ui->toolButtonAll, &QAbstractButton::clicked, this, &TexxyWindow::replaceAll);
    connect(ui->dockReplace, &QDockWidget::visibilityChanged, this, &TexxyWindow::dockVisibilityChanged);
    connect(ui->dockReplace, &QDockWidget::topLevelChanged, this, &TexxyWindow::resizeDock);

    connect(ui->actionDoc, &QAction::triggered, this, &TexxyWindow::docProp);

    connect(ui->actionAbout, &QAction::triggered, this, &TexxyWindow::aboutDialog);

    connect(this, &TexxyWindow::finishedLoading, [this] {
        if (sidePane_)
            sidePane_->listWidget()->scrollToCurrentItem();
    });
    ui->actionSidePane->setAutoRepeat(false);  // don't let UI change too rapidly
    connect(ui->actionSidePane, &QAction::triggered, [this] { toggleSidePane(); });

    // see comment block explaining KDE KAcceleratorManager mnemonic behavior
    ui->toolButtonNext->setShortcut(QKeySequence(Qt::Key_F8));
    ui->toolButtonPrv->setShortcut(QKeySequence(Qt::Key_F9));
    ui->toolButtonAll->setShortcut(QKeySequence(Qt::Key_F10));

    auto* zoomin = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);
    auto* zoominPlus = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), this);
    auto* zoomout = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
    auto* zoomzero = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this);
    connect(zoomin, &QShortcut::activated, this, &TexxyWindow::zoomIn);
    connect(zoominPlus, &QShortcut::activated, this, &TexxyWindow::zoomIn);
    connect(zoomout, &QShortcut::activated, this, &TexxyWindow::zoomOut);
    connect(zoomzero, &QShortcut::activated, this, &TexxyWindow::zoomZero);

    auto* fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreen, &QShortcut::activated, [this] { setWindowState(windowState() ^ Qt::WindowFullScreen); });

    auto* focusView = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(focusView, &QShortcut::activated, this, &TexxyWindow::focusView);

    auto* focusSidePane = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Escape), this);
    connect(focusSidePane, &QShortcut::activated, this, &TexxyWindow::focusSidePane);

    // exiting a process
    auto* kill = new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_E), this);
    connect(kill, &QShortcut::activated, this, &TexxyWindow::exitProcess);

    dummyWidget = new QWidget();
    setAcceptDrops(true);
    setAttribute(Qt::WA_AlwaysShowToolTips);
    setAttribute(Qt::WA_DeleteOnClose, false);  // we delete windows in singleton
}
/*************************/
TexxyWindow::~TexxyWindow() {
    startAutoSaving(false);
    delete dummyWidget;
    dummyWidget = nullptr;
    delete aGroup_;
    aGroup_ = nullptr;
    delete ui;
    ui = nullptr;
}
/*************************/
void TexxyWindow::closeEvent(QCloseEvent* event) {
    auto* singleton = static_cast<TexxyApplication*>(qApp);
    // with Qt6, QCoreApplication::quit() calls closeEvent when visible
    // when a quit signal is received we accept without prompt
    if (singleton->isQuitSignalReceived()) {
        event->accept();
        return;
    }

    const bool keep = locked_ || closePages(-1, -1, true);
    if (keep) {
        event->ignore();
        if (!locked_)
            lastWinFilesCur_.clear();  // precaution
    }
    else {
        Config& config = singleton->getConfig();
        if (!isMaximized() && !isFullScreen()) {
            if (config.getRemSize())
                config.setWinSize(size());
            if (config.getRemPos() && !static_cast<TexxyApplication*>(qApp)->isWayland())
                config.setWinPos(geometry().topLeft());
        }
        if (sidePane_ && config.getRemSplitterPos())
            config.setSplitterPos(ui->splitter->sizes().at(0));
        config.setLastFileCursorPos(lastWinFilesCur_);
        singleton->removeWin(this);
        event->accept();
    }
}
/*************************/
// should be called only when the app quits without closing its windows
// it saves info that can be queried only at session end like cursor positions
void TexxyWindow::cleanUpOnTerminating(Config& config, bool isLastWin) {
    // Qt5 crash workaround also fine on Qt6
    disconnect(ui->dockReplace, &QDockWidget::visibilityChanged, this, &TexxyWindow::dockVisibilityChanged);

    lastWinFilesCur_.clear();
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i))) {
            const QPointer<TextEdit> textEditPtr = tabPage->textEdit();
            TextEdit* textEdit = textEditPtr.data();
            if (!textEdit)
                continue;
            const QString fileName = textEdit->getFileName();
            if (!fileName.isEmpty()) {
                if (textEdit->getSaveCursor())
                    config.saveCursorPos(fileName, textEdit->textCursor().position());
                if (isLastWin && config.getSaveLastFilesList() && lastWinFilesCur_.size() < kMaxLastWinFiles &&
                    QFile::exists(fileName)) {
                    lastWinFilesCur_.insert(fileName, textEdit->textCursor().position());
                }
            }
        }
    }
    config.setLastFileCursorPos(lastWinFilesCur_);
}
/*************************/
void TexxyWindow::menubarTitle(bool add, bool setTitle) {
    QWidget* cw = ui->menuBar->cornerWidget();

    if (!add) {
        if (cw == nullptr)
            return;
        ui->menuBar->setCornerWidget(nullptr);
        delete cw;
        return;
    }

    if (cw != nullptr || ui->menuBar->isHidden())
        return;
    auto* mbTitle = new MenuBarTitle();
    ui->menuBar->setCornerWidget(mbTitle);
    const auto menubarActions = ui->menuBar->actions();
    if (!menubarActions.isEmpty()) {
        const QRect g = ui->menuBar->actionGeometry(menubarActions.last());
        mbTitle->setStart(QApplication::layoutDirection() == Qt::RightToLeft ? ui->menuBar->width() - g.left()
                                                                             : g.right() + 1);
        mbTitle->setHeightOverride(g.height());
    }
    mbTitle->show();
    connect(mbTitle, &QWidget::customContextMenuRequested, this, &TexxyWindow::tabContextMenu);
    connect(mbTitle, &MenuBarTitle::doubleClicked, this, [this] {
        if (windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen))
            showNormal();
        else
            setWindowState(Qt::WindowMaximized);
    });

    if (setTitle && ui->tabWidget->currentIndex() > -1)
        mbTitle->setTitle(windowTitle());
}
/*************************/
void TexxyWindow::applyConfigOnStarting() {
    Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();

    // geometry and window state
    if (config.getRemSize()) {
        resize(config.getWinSize());
        // on Wayland or when position isn't remembered, apply state now, otherwise showEvent handles it
        if (!config.getRemPos() || static_cast<TexxyApplication*>(qApp)->isWayland()) {
            Qt::WindowStates st = {};
            if (config.getIsMaxed())
                st |= Qt::WindowMaximized;
            if (config.getIsFull())
                st |= Qt::WindowFullScreen;
            if (st)
                setWindowState(st);
        }
    }
    else {
        QSize startSize = config.getStartSize();
        if (startSize.isEmpty()) {
            startSize = QSize(700, 500);     // sane default
            config.setStartSize(startSize);  // persist default so later writes are minimized
        }
        resize(startSize);
    }

    // basic UI visibility
    ui->mainToolBar->setVisible(!config.getNoToolbar());
    ui->menuBar->setVisible(!config.getNoMenubar());
    ui->actionMenu->setVisible(config.getNoMenubar());

    if (config.getMenubarTitle())
        menubarTitle();

    ui->actionDoc->setVisible(!config.getShowStatusbar());
    ui->actionWrap->setChecked(config.getWrapByDefault());
    ui->actionIndent->setChecked(config.getIndentByDefault());
    ui->actionLineNumbers->setChecked(config.getLineByDefault());
    ui->actionLineNumbers->setDisabled(config.getLineByDefault());
    ui->actionSyntax->setChecked(config.getSyntaxByDefault());

    // statusbar and optional widgets
    if (!config.getShowStatusbar()) {
        ui->statusBar->hide();
    }
    else {
        if (config.getShowCursorPos())
            addCursorPosLabel();
    }
    if (config.getShowLangSelector() && config.getSyntaxByDefault())
        addRemoveLangBtn(true);

    // tab position and side pane behavior
    if (config.getTabPosition() != 0)
        ui->tabWidget->setTabPosition(static_cast<QTabWidget::TabPosition>(config.getTabPosition()));

    if (!config.getSidePaneMode()) {
        // hideSingle should not be active when side pane mode is on
        ui->tabWidget->tabBar()->hideSingle(config.getHideSingleTab());
        // connect with UniqueConnection to avoid duplicates on repeated calls
        QObject::connect(ui->actionLastTab, &QAction::triggered, this, &TexxyWindow::lastTab, Qt::UniqueConnection);
        QObject::connect(ui->actionFirstTab, &QAction::triggered, this, &TexxyWindow::firstTab, Qt::UniqueConnection);
    }
    else {
        toggleSidePane();
    }

    // recently opened menu
    if (config.getRecentOpened())
        ui->menuOpenRecently->setTitle(tr("&Recently Opened"));
    const int recentNumber = config.getCurRecentFilesNumber();
    if (recentNumber <= 0) {
        ui->menuOpenRecently->setEnabled(false);
    }
    else {
        for (int i = 0; i < recentNumber; ++i) {
            auto* recentAction = new QAction(this);
            recentAction->setVisible(false);
            QObject::connect(recentAction, &QAction::triggered, this, &TexxyWindow::newTabFromRecent,
                             Qt::UniqueConnection);
            ui->menuOpenRecently->addAction(recentAction);
        }
        ui->menuOpenRecently->addSeparator();
        ui->menuOpenRecently->addAction(ui->actionClearRecent);
        QObject::connect(ui->menuOpenRecently, &QMenu::aboutToShow, this, &TexxyWindow::updateRecenMenu,
                         Qt::UniqueConnection);
        QObject::connect(ui->actionClearRecent, &QAction::triggered, this, &TexxyWindow::clearRecentMenu,
                         Qt::UniqueConnection);
    }

    // saving behavior
    ui->actionSave->setEnabled(config.getSaveUnmodified());  // newTab will adjust per tab state

    // icons
    const bool useSys = config.getSysIcons();
    const bool rtl = QApplication::layoutDirection() == Qt::RightToLeft;

    auto setIconTheme = [&](QAction* act, const char* name, const QIcon& fallback = QIcon()) {
        if (!act)
            return;
        act->setIcon(QIcon::fromTheme(name, fallback));
    };
    auto setIconRes = [&](QAction* act, const char* res) {
        if (!act)
            return;
        act->setIcon(symbolicIcon::icon(res));
    };

    if (useSys) {
        setIconTheme(ui->actionNew, "document-new");
        setIconTheme(ui->actionOpen, "document-open");
        setIconTheme(ui->actionSession, "bookmark-new");
        setIconTheme(ui->menuOpenRecently->menuAction(), "document-open-recent");
        setIconTheme(ui->actionClearRecent, "edit-clear");
        setIconTheme(ui->actionSave, "document-save");
        setIconTheme(ui->actionSaveAs, "document-save-as");
        setIconTheme(ui->actionSaveAllFiles, "document-save-all");
        setIconTheme(ui->actionSaveCodec, "document-save-as");
        setIconTheme(ui->actionDoc, "document-properties");
        setIconTheme(ui->actionUndo, "edit-undo");
        setIconTheme(ui->actionRedo, "edit-redo");
        setIconTheme(ui->actionCut, "edit-cut");
        setIconTheme(ui->actionCopy, "edit-copy");
        setIconTheme(ui->actionPaste, "edit-paste");
        setIconTheme(ui->actionDate, "appointment-new");
        setIconTheme(ui->actionDelete, "edit-delete");
        setIconTheme(ui->actionSelectAll, "edit-select-all");
        setIconTheme(ui->actionReload, "view-refresh");
        setIconTheme(ui->actionFind, "edit-find");
        setIconTheme(ui->actionReplace, "edit-find-replace");
        setIconTheme(ui->actionClose, "window-close");
        setIconTheme(ui->actionQuit, "application-exit");
        setIconTheme(ui->actionFont, "preferences-desktop-font");
        setIconTheme(ui->actionPreferences, "preferences-system");
        setIconTheme(ui->actionAbout, "help-about");
        setIconTheme(ui->actionJump, "go-jump");
        setIconTheme(ui->actionSidePane, "sidebar-expand-left", symbolicIcon::icon(":icons/side-pane.svg"));
        setIconTheme(ui->actionEdit, "document-edit");
        setIconTheme(ui->actionRun, "system-run");
        setIconTheme(ui->actionCopyName, "edit-copy");
        setIconTheme(ui->actionCopyPath, "edit-copy");
        setIconTheme(ui->actionCloseOther, "window-close");
        setIconTheme(ui->actionMenu, "application-menu");

        setIconTheme(ui->actionCloseRight, rtl ? "go-previous" : "go-next");
        setIconTheme(ui->actionCloseLeft, rtl ? "go-next" : "go-previous");
        setIconTheme(ui->actionRightTab, rtl ? "go-previous" : "go-next");
        setIconTheme(ui->actionLeftTab, rtl ? "go-next" : "go-previous");
    }
    else {
        setIconRes(ui->actionNew, ":icons/document-new.svg");
        setIconRes(ui->actionOpen, ":icons/document-open.svg");
        setIconRes(ui->actionSession, ":icons/session.svg");
        setIconRes(ui->menuOpenRecently->menuAction(), ":icons/document-open-recent.svg");
        setIconRes(ui->actionClearRecent, ":icons/edit-clear.svg");
        setIconRes(ui->actionSave, ":icons/document-save.svg");
        setIconRes(ui->actionSaveAs, ":icons/document-save-as.svg");
        setIconRes(ui->actionSaveAllFiles, ":icons/document-save-all.svg");
        setIconRes(ui->actionSaveCodec, ":icons/document-save-as.svg");
        setIconRes(ui->actionDoc, ":icons/document-properties.svg");
        setIconRes(ui->actionUndo, ":icons/edit-undo.svg");
        setIconRes(ui->actionRedo, ":icons/edit-redo.svg");
        setIconRes(ui->actionCut, ":icons/edit-cut.svg");
        setIconRes(ui->actionCopy, ":icons/edit-copy.svg");
        setIconRes(ui->actionPaste, ":icons/edit-paste.svg");
        setIconRes(ui->actionDate, ":icons/document-open-recent.svg");
        setIconRes(ui->actionDelete, ":icons/edit-delete.svg");
        setIconRes(ui->actionSelectAll, ":icons/edit-select-all.svg");
        setIconRes(ui->actionReload, ":icons/view-refresh.svg");
        setIconRes(ui->actionFind, ":icons/edit-find.svg");
        setIconRes(ui->actionReplace, ":icons/edit-find-replace.svg");
        setIconRes(ui->actionClose, ":icons/window-close.svg");
        setIconRes(ui->actionQuit, ":icons/application-exit.svg");
        setIconRes(ui->actionFont, ":icons/preferences-desktop-font.svg");
        setIconRes(ui->actionPreferences, ":icons/preferences-system.svg");
        setIconRes(ui->actionAbout, ":icons/help-about.svg");
        setIconRes(ui->actionJump, ":icons/go-jump.svg");
        setIconRes(ui->actionSidePane, ":icons/side-pane.svg");
        setIconRes(ui->actionEdit, ":icons/document-edit.svg");
        setIconRes(ui->actionRun, ":icons/system-run.svg");
        setIconRes(ui->actionCopyName, ":icons/edit-copy.svg");
        setIconRes(ui->actionCopyPath, ":icons/edit-copy.svg");
        setIconRes(ui->actionCloseOther, ":icons/tab-close-other.svg");
        setIconRes(ui->actionMenu, ":icons/application-menu.svg");

        setIconRes(ui->actionCloseRight, rtl ? ":icons/go-previous.svg" : ":icons/go-next.svg");
        setIconRes(ui->actionCloseLeft, rtl ? ":icons/go-next.svg" : ":icons/go-previous.svg");
        setIconRes(ui->actionRightTab, rtl ? ":icons/go-previous.svg" : ":icons/go-next.svg");
        setIconRes(ui->actionLeftTab, rtl ? ":icons/go-next.svg" : ":icons/go-previous.svg");
    }

    // small toolbar icons
    ui->toolButtonNext->setIcon(symbolicIcon::icon(":icons/go-down.svg"));
    ui->toolButtonPrv->setIcon(symbolicIcon::icon(":icons/go-up.svg"));
    ui->toolButtonAll->setIcon(symbolicIcon::icon(":icons/arrow-down-double.svg"));

    // window icon
    setWindowIcon(QIcon::fromTheme("texxy", QIcon(":icons/texxy.svg")));

    // reserved shortcuts initialization
    if (!config.hasReservedShortcuts()) {
        QStringList reserved;
        // QPlainTextEdit
        reserved << QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_Z).toString() << QKeySequence(Qt::CTRL | Qt::Key_X).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_C).toString() << QKeySequence(Qt::CTRL | Qt::Key_V).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_A).toString() << QKeySequence(Qt::SHIFT | Qt::Key_Insert).toString()
                 << QKeySequence(Qt::SHIFT | Qt::Key_Delete).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_Insert).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_Left).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_Right).toString() << QKeySequence(Qt::CTRL | Qt::Key_Up).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_Down).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_PageUp).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_PageDown).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_Home).toString() << QKeySequence(Qt::CTRL | Qt::Key_End).toString()
                 << QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Up).toString()
                 << QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down).toString()
                 << QKeySequence(Qt::META | Qt::Key_Up).toString() << QKeySequence(Qt::META | Qt::Key_Down).toString()
                 << QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_Up).toString()
                 << QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_Down).toString()
                 // search and replacement
                 << QKeySequence(Qt::Key_F3).toString() << QKeySequence(Qt::Key_F4).toString()
                 << QKeySequence(Qt::Key_F5).toString() << QKeySequence(Qt::Key_F6).toString()
                 << QKeySequence(Qt::Key_F7).toString() << QKeySequence(Qt::Key_F8).toString()
                 << QKeySequence(Qt::Key_F9).toString() << QKeySequence(Qt::Key_F10).toString()
                 << QKeySequence(Qt::Key_F11).toString()
                 // side pane focusing
                 << QKeySequence(Qt::CTRL | Qt::Key_Escape).toString()
                 // zooming
                 << QKeySequence(Qt::CTRL | Qt::Key_Equal).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_Plus).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_Minus).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_0).toString()
                 // exiting a process
                 << QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_E).toString()
                 // text tabulation
                 << QKeySequence(Qt::SHIFT | Qt::Key_Enter).toString()
                 << QKeySequence(Qt::SHIFT | Qt::Key_Return).toString() << QKeySequence(Qt::Key_Tab).toString()
                 << QKeySequence(Qt::CTRL | Qt::Key_Tab).toString()
                 << QKeySequence(Qt::CTRL | Qt::META | Qt::Key_Tab).toString()
                 // select text on jumping
                 << QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_J).toString()
                 // used by LineEdit as well as QPlainTextEdit
                 << QKeySequence(Qt::CTRL | Qt::Key_K).toString();

        config.setReservedShortcuts(reserved);
        config.readShortcuts();
    }

    // apply custom shortcuts
    const auto ca = config.customShortcutActions();
    for (auto it = ca.constBegin(); it != ca.constEnd(); ++it) {
        if (auto* action = findChild<QAction*>(it.key()))
            action->setShortcut(QKeySequence(it.value(), QKeySequence::PortableText));
    }

    // autosave
    if (config.getAutoSave())
        startAutoSaving(true, config.getAutoSaveInterval());

    // optionally strip menubar accelerators
    if (config.getDisableMenubarAccel()) {
        const auto menubarActions = ui->menuBar->actions();
        for (auto* action : menubarActions) {
            QString txt = action->text();
            txt.remove(QRegularExpression(QStringLiteral("\\s*\\(&[a-zA-Z0-9]\\)\\s*")));  // zh ja and similar
            txt.remove(QLatin1Char('&'));                                                  // other locales
            action->setText(txt);
        }
    }
}

/*************************/
void TexxyWindow::addCursorPosLabel() {
    if (ui->statusBar->findChild<QLabel*>("posLabel"))
        return;
    auto* posLabel = new QLabel();
    posLabel->setObjectName("posLabel");
    posLabel->setText(u"<b>" + tr("Position:") + u"</b>");
    posLabel->setIndent(2);
    posLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->statusBar->addPermanentWidget(posLabel);
}
/*************************/
// we want all dialogs to be window-modal as far as possible
// keep window-modality but prevent two window-modal dialogs at once
bool TexxyWindow::hasAnotherDialog() {
    closeWarningBar();
    bool res = false;
    auto* singleton = static_cast<TexxyApplication*>(qApp);
    for (int i = 0; i < singleton->Wins.count(); ++i) {
        TexxyWindow* win = singleton->Wins.at(i);
        if (win != this) {
            const QList<QDialog*> dialogs = win->findChildren<QDialog*>();
            for (auto* dlg : dialogs) {
                if (dlg->isModal()) {
                    res = true;
                    break;
                }
            }
            if (res)
                break;
        }
    }
    if (res) {
        showWarningBar("<center><b><big>" + tr("Another Texxy window has a modal dialog!") + "</big></b></center>" +
                           "<center><i>" + tr("Please attend to that window or just close its dialog!") +
                           "</i></center>",
                       15);
    }
    return res;
}
/*************************/
void TexxyWindow::updateGUIForSingleTab(bool single) {
    ui->actionDetachTab->setEnabled(!single && !static_cast<TexxyApplication*>(qApp)->isStandAlone());
    ui->actionRightTab->setEnabled(!single);
    ui->actionLeftTab->setEnabled(!single);
    ui->actionLastTab->setEnabled(!single);
    ui->actionFirstTab->setEnabled(!single);
}
/*************************/
void TexxyWindow::updateCustomizableShortcuts(bool disable) {
    auto iter = defaultShortcuts_.constBegin();
    if (disable) {
        while (iter != defaultShortcuts_.constEnd()) {
            iter.key()->setShortcut(QKeySequence());
            ++iter;
        }
    }
    else {
        QHash<QString, QString> ca = static_cast<TexxyApplication*>(qApp)->getConfig().customShortcutActions();
        const QList<QString> cn = ca.keys();

        while (iter != defaultShortcuts_.constEnd()) {
            const QString name = iter.key()->objectName();
            iter.key()->setShortcut(cn.contains(name) ? QKeySequence(ca.value(name), QKeySequence::PortableText)
                                                      : iter.value());
            ++iter;
        }
    }
}
/*************************/
// disable all shortcuts on showing a dialog and enable them again on hiding it
// also updates shortcuts after customization in Preferences
void TexxyWindow::updateShortcuts(bool disable, bool page) {
    if (disable) {
        ui->actionCut->setShortcut(QKeySequence());
        ui->actionCopy->setShortcut(QKeySequence());
        ui->actionPaste->setShortcut(QKeySequence());
        ui->actionSelectAll->setShortcut(QKeySequence());

        ui->toolButtonNext->setShortcut(QKeySequence());
        ui->toolButtonPrv->setShortcut(QKeySequence());
        ui->toolButtonAll->setShortcut(QKeySequence());
    }
    else {
        ui->actionCut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
        ui->actionCopy->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
        ui->actionPaste->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
        ui->actionSelectAll->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));

        ui->toolButtonNext->setShortcut(QKeySequence(Qt::Key_F8));
        ui->toolButtonPrv->setShortcut(QKeySequence(Qt::Key_F9));
        ui->toolButtonAll->setShortcut(QKeySequence(Qt::Key_F10));
    }
    updateCustomizableShortcuts(disable);

    if (page) {
        if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
            tabPage->updateShortcuts(disable);
    }
}
/*************************/
void TexxyWindow::displayMessage(bool error) {
    auto* process = qobject_cast<QProcess*>(QObject::sender());
    if (!process)
        return;
    QByteArray msg;
    if (error) {
        process->setReadChannel(QProcess::StandardError);
        msg = process->readAllStandardError();
    }
    else {
        process->setReadChannel(QProcess::StandardOutput);
        msg = process->readAllStandardOutput();
    }
    if (msg.isEmpty())
        return;

    QPointer<QDialog> msgDlg = nullptr;
    const QList<QDialog*> dialogs = findChildren<QDialog*>();
    for (auto* d : dialogs) {
        if (d->parent() == process->parent()) {
            msgDlg = d;
            break;
        }
    }
    if (msgDlg) {
        if (auto* tEdit = msgDlg->findChild<QPlainTextEdit*>()) {
            tEdit->setPlainText(tEdit->toPlainText() + u"\n" + QString::fromUtf8(msg));
            QTextCursor cur = tEdit->textCursor();
            cur.movePosition(QTextCursor::End);
            tEdit->setTextCursor(cur);
            stealFocus(msgDlg);
        }
    }
    else {
        msgDlg = new QDialog(qobject_cast<QWidget*>(process->parent()));
        msgDlg->setWindowTitle(tr("Script Output"));
        msgDlg->setSizeGripEnabled(true);
        auto* grid = new QGridLayout;
        auto* label = new QLabel(msgDlg);
        label->setText(u"<center><b>" + tr("Script File") + u": </b></center><i>" + process->objectName() + u"</i>");
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setWordWrap(true);
        label->setMargin(5);
        grid->addWidget(label, 0, 0, 1, 2);
        auto* tEdit = new QPlainTextEdit(msgDlg);
        tEdit->setTextInteractionFlags(Qt::TextSelectableByMouse);
        tEdit->ensureCursorVisible();
        grid->addWidget(tEdit, 1, 0, 1, 2);
        auto* closeButton = new QPushButton(QIcon::fromTheme("edit-delete"), tr("Close"));
        connect(closeButton, &QAbstractButton::clicked, msgDlg, &QDialog::reject);
        grid->addWidget(closeButton, 2, 1, Qt::AlignRight);
        auto* clearButton = new QPushButton(QIcon::fromTheme("edit-clear"), tr("Clear"));
        connect(clearButton, &QAbstractButton::clicked, tEdit, &QPlainTextEdit::clear);
        grid->addWidget(clearButton, 2, 0, Qt::AlignLeft);
        msgDlg->setLayout(grid);
        tEdit->setPlainText(QString::fromUtf8(msg));
        QTextCursor cur = tEdit->textCursor();
        cur.movePosition(QTextCursor::End);
        tEdit->setTextCursor(cur);
        msgDlg->setAttribute(Qt::WA_DeleteOnClose);
        msgDlg->show();
        msgDlg->raise();
        msgDlg->activateWindow();
    }
}
/*************************/
void TexxyWindow::makeBusy() {
    if (QGuiApplication::overrideCursor() == nullptr)
        QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
}
/*************************/
void TexxyWindow::unbusy() {
    if (QGuiApplication::overrideCursor() != nullptr)
        QGuiApplication::restoreOverrideCursor();
}
/*************************/
void TexxyWindow::showWarningBar(const QString& message, int timeout, bool startupBar) {
    // don't show this warning bar if the window is locked at this moment
    if (locked_)
        return;
    if (timeout > 0) {
        // don't show the temporary warning bar when there's a modal dialog
        const QList<QDialog*> dialogs = findChildren<QDialog*>();
        for (auto* d : dialogs) {
            if (d->isModal())
                return;
        }
    }

    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    TextEdit* curEdit = nullptr;
    if (tabPage) {
        const QPointer<TextEdit> te = tabPage->textEdit();
        curEdit = te.data();
    }

    // don't close and show the same warning bar
    if (auto* prevBar = ui->tabWidget->findChild<WarningBar*>()) {
        if (!prevBar->isClosing() && prevBar->getMessage() == message) {
            prevBar->setTimeout(timeout);
            if (curEdit && timeout > 0) {
                disconnect(curEdit, &QPlainTextEdit::updateRequest, prevBar, &WarningBar::closeBarOnScrolling);
                connect(curEdit, &QPlainTextEdit::updateRequest, prevBar, &WarningBar::closeBarOnScrolling);
            }
            return;
        }
    }

    int vOffset = 0;
    if (tabPage && curEdit)
        vOffset = tabPage->height() - curEdit->height();
    auto* bar = new WarningBar(message, vOffset, timeout, ui->tabWidget);
    if (startupBar)
        bar->setObjectName("startupBar");
    // close the bar when the text is scrolled
    if (curEdit && timeout > 0)
        connect(curEdit, &QPlainTextEdit::updateRequest, bar, &WarningBar::closeBarOnScrolling);
}
/*************************/
void TexxyWindow::showRootWarning() {
    QTimer::singleShot(
        0, this, [=]() { showWarningBar("<center><b><big>" + tr("Root Instance") + "</big></b></center>", 10, true); });
}
/*************************/
void TexxyWindow::closeWarningBar(bool keepOnStartup) {
    const QList<WarningBar*> warningBars = ui->tabWidget->findChildren<WarningBar*>();
    for (auto* wb : warningBars) {
        if (!keepOnStartup || wb->objectName() != "startupBar")
            wb->closeBar();
    }
}
/*************************/
void TexxyWindow::changeEvent(QEvent* event) {
    Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();
    if (event->type() == QEvent::WindowStateChange) {
        if (config.getRemSize()) {
            if (windowState() == Qt::WindowFullScreen) {
                config.setIsFull(true);
                config.setIsMaxed(false);
            }
            else if (windowState() == (Qt::WindowFullScreen ^ Qt::WindowMaximized)) {
                config.setIsFull(true);
                config.setIsMaxed(true);
            }
            else {
                config.setIsFull(false);
                if (windowState() == Qt::WindowMaximized)
                    config.setIsMaxed(true);
                else
                    config.setIsMaxed(false);
            }
        }
        // if the window gets maximized/fullscreen, remember its position and size
        if ((windowState() & Qt::WindowMaximized) || (windowState() & Qt::WindowFullScreen)) {
            if (auto* stateEvent = static_cast<QWindowStateChangeEvent*>(event)) {
                if (!(stateEvent->oldState() & Qt::WindowMaximized) &&
                    !(stateEvent->oldState() & Qt::WindowFullScreen)) {
                    if (config.getRemPos() && !static_cast<TexxyApplication*>(qApp)->isWayland())
                        config.setWinPos(geometry().topLeft());
                    if (config.getRemSize())
                        config.setWinSize(size());
                }
            }
        }
    }
    QWidget::changeEvent(event);
}
/*************************/
void TexxyWindow::showEvent(QShowEvent* event) {
    // to position the main window correctly when it's shown the first time
    if (!shownBefore_ && !event->spontaneous()) {
        shownBefore_ = true;
        Config config = static_cast<TexxyApplication*>(qApp)->getConfig();
        if (config.getRemPos() && !static_cast<TexxyApplication*>(qApp)->isWayland()) {
            const QSize theSize = (config.getRemSize() ? config.getWinSize() : config.getStartSize());
            setGeometry(QRect(config.getWinPos(), theSize));
            if (config.getIsFull() && config.getIsMaxed())
                setWindowState(Qt::WindowMaximized | Qt::WindowFullScreen);
            else if (config.getIsMaxed())
                setWindowState(Qt::WindowMaximized);
            else if (config.getIsFull())
                setWindowState(Qt::WindowFullScreen);
        }
    }
    QWidget::showEvent(event);
}
/*************************/
bool TexxyWindow::event(QEvent* event) {
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
            const QPointer<TextEdit> textEditPtr = tabPage->textEdit();
            TextEdit* textEdit = textEditPtr.data();
            if (!textEdit)
                return QMainWindow::event(event);
            const QString fname = textEdit->getFileName();
            if (!fname.isEmpty()) {
                if (!QFile::exists(fname)) {
                    if (isLoading())
                        connect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onOpeningNonexistent,
                                Qt::UniqueConnection);
                    else
                        onOpeningNonexistent();
                }
                else if (textEdit->getLastModified() != QFileInfo(fname).lastModified()) {
                    showWarningBar("<center><b><big>" + tr("This file has been modified elsewhere or in another way!") +
                                       "</big></b></center>\n" + "<center>" +
                                       tr("Please be careful about reloading or saving this document!") + "</center>",
                                   15);
                }
            }
        }
    }
    return QMainWindow::event(event);
}
/*************************/
void TexxyWindow::prefDialog() {
    if (isLoading())
        return;
    if (hasAnotherDialog())
        return;

    updateShortcuts(true);
    PrefDialog dlg(this);
    dlg.exec();
    updateShortcuts(false);
}
/*************************/
void TexxyWindow::aboutDialog() {
    if (isLoading())
        return;

    if (hasAnotherDialog())
        return;
    updateShortcuts(true);

    class AboutDialog : public QDialog {
       public:
        explicit AboutDialog(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QDialog(parent, f) {
            aboutUi.setupUi(this);
            aboutUi.textLabel->setOpenExternalLinks(true);
        }
        void setTabTexts(const QString& first, const QString& sec) {
            aboutUi.tabWidget->setTabText(0, first);
            aboutUi.tabWidget->setTabText(1, sec);
        }
        void setMainIcon(const QIcon& icn) { aboutUi.iconLabel->setPixmap(icn.pixmap(64, 64)); }
        void settMainTitle(const QString& title) { aboutUi.titleLabel->setText(title); }
        void setMainText(const QString& txt) { aboutUi.textLabel->setText(txt); }

       private:
        Ui::AboutDialog aboutUi;
    };

    AboutDialog dialog(this);
    dialog.setMainIcon(QIcon::fromTheme("texxy", QIcon(":icons/texxy.svg")));
    dialog.settMainTitle(QString(u"<center><b><big>%1 %2</big></b></center><br>")
                             .arg(qApp->applicationName(), qApp->applicationVersion()));
    dialog.setMainText(u"<center> " + tr("A lightweight, tabbed, plain-text editor") + u" </center>\n<center> " +
                       tr("based on Qt") + u" </center><br><center> " + tr("Author") +
                       u": <a href='mailto:share@1g4.org?Subject=My%20Subject'>jopamo</a> </center><p></p>");
    dialog.setTabTexts(tr("About Texxy"), tr("Whatever"));
    dialog.setWindowTitle(tr("About Texxy"));
    dialog.setWindowModality(Qt::WindowModal);
    dialog.exec();
    updateShortcuts(false);
}
}  // namespace Texxy
