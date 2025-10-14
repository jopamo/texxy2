/*
 * texxy/fpwin.cpp
 */

#include "singleton.h"
#include "ui_fp.h"
#include "ui_about.h"
#include "encoding.h"
#include "filedialog.h"
#include "messagebox.h"
#include "pref.h"
#include "spellChecker.h"
#include "spellDialog.h"
#include "session.h"
#include "fontDialog.h"
#include "loading.h"
#include "printing.h"
#include "warningbar.h"
#include "menubartitle.h"
#include "svgicons.h"

#include <QMimeDatabase>
#include <QPrintDialog>
#include <QToolTip>
#include <QScreen>
#include <QWindow>
#include <QScrollBar>
#include <QWidgetAction>
#include <fstream>  // std::ofstream
#include <QPrinter>
#include <QClipboard>
#include <QProcess>
#include <QTextDocumentWriter>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QPushButton>
#include <QDBusConnection>  // for opening containing folder
#include <QDBusMessage>     // for opening containing folder
#include <QStringDecoder>

#ifdef HAS_X11
#include "x11.h"
#endif

#define MAX_LAST_WIN_FILES 50

namespace FeatherPad {

FPwin::FPwin(QWidget* parent) : QMainWindow(parent), dummyWidget(nullptr), ui(new Ui::FPwin) {
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

    /* "Jump to" bar */
    ui->spinBox->hide();
    ui->label->hide();
    ui->checkBox->hide();

    /* status bar */
    QLabel* statusLabel = new QLabel();
    statusLabel->setObjectName("statusLabel");
    statusLabel->setIndent(2);
    statusLabel->setMinimumWidth(100);
    statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QToolButton* wordButton = new QToolButton();
    wordButton->setObjectName("wordButton");
    wordButton->setFocusPolicy(Qt::NoFocus);
    wordButton->setAutoRaise(true);
    wordButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    wordButton->setIconSize(QSize(16, 16));
    wordButton->setIcon(symbolicIcon::icon(":icons/view-refresh.svg"));
    wordButton->setToolTip("<p style='white-space:pre'>" + tr("Calculate number of words") + "</p>");
    connect(wordButton, &QAbstractButton::clicked, [this] { updateWordInfo(); });
    ui->statusBar->addWidget(statusLabel);
    ui->statusBar->addWidget(wordButton);

    /* text unlocking */
    ui->actionEdit->setVisible(false);

    ui->actionRun->setVisible(false);

    /* replace dock */
    QWidget::setTabOrder(ui->lineEditFind, ui->lineEditReplace);
    QWidget::setTabOrder(ui->lineEditReplace, ui->toolButtonNext);
    /* tooltips are set here for easier translation */
    ui->toolButtonNext->setToolTip(tr("Next") + " (" + QKeySequence(Qt::Key_F8).toString(QKeySequence::NativeText) +
                                   ")");
    ui->toolButtonPrv->setToolTip(tr("Previous") + " (" + QKeySequence(Qt::Key_F9).toString(QKeySequence::NativeText) +
                                  ")");
    ui->toolButtonAll->setToolTip(tr("Replace all") + " (" +
                                  QKeySequence(Qt::Key_F10).toString(QKeySequence::NativeText) + ")");
    ui->dockReplace->setVisible(false);

    /* shortcuts should be reversed for rtl */
    if (QApplication::layoutDirection() == Qt::RightToLeft) {
        ui->actionRightTab->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
        ui->actionLeftTab->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    }

    /* get the default (customizable) shortcuts before any change */
    static const QStringList excluded = {"actionCut", "actionCopy", "actionPaste", "actionSelectAll"};
    const auto allMenus = ui->menuBar->findChildren<QMenu*>();
    for (const auto& thisMenu : allMenus) {
        const auto menuActions = thisMenu->actions();
        for (const auto& menuAction : menuActions) {
            QKeySequence seq = menuAction->shortcut();
            if (!seq.isEmpty() && !excluded.contains(menuAction->objectName()))
                defaultShortcuts_.insert(menuAction, seq);
        }
    }
    /* exceptions */
    defaultShortcuts_.insert(ui->actionSaveAllFiles, QKeySequence());
    defaultShortcuts_.insert(ui->actionSoftTab, QKeySequence());
    defaultShortcuts_.insert(ui->actionStartCase, QKeySequence());
    defaultShortcuts_.insert(ui->actionUserDict, QKeySequence());
    defaultShortcuts_.insert(ui->actionFont, QKeySequence());

    applyConfigOnStarting();

    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->mainToolBar->insertWidget(ui->actionMenu, spacer);
    QMenu* menu = new QMenu(ui->mainToolBar);
    menu->addMenu(ui->menuFile);
    menu->addMenu(ui->menuEdit);
    menu->addMenu(ui->menuOptions);
    menu->addMenu(ui->menuSearch);
    menu->addMenu(ui->menuHelp);
    ui->actionMenu->setMenu(menu);
    QList<QToolButton*> tbList = ui->mainToolBar->findChildren<QToolButton*>();
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

    if (static_cast<FPsingleton*>(qApp)->isStandAlone())
        ui->tabWidget->noTabDND();

    connect(ui->actionQuit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionNew, &QAction::triggered, this, &FPwin::newTab);
    connect(ui->tabWidget->tabBar(), &TabBar::addEmptyTab, this, &FPwin::newTab);
    connect(ui->actionDetachTab, &QAction::triggered, this, &FPwin::detachTab);
    connect(ui->actionRightTab, &QAction::triggered, this, &FPwin::nextTab);
    connect(ui->actionLeftTab, &QAction::triggered, this, &FPwin::previousTab);
    connect(ui->actionLastActiveTab, &QAction::triggered, this, &FPwin::lastActiveTab);
    connect(ui->actionClose, &QAction::triggered, this, &FPwin::closePage);
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &FPwin::closeTabAtIndex);
    connect(ui->actionOpen, &QAction::triggered, this, &FPwin::fileOpen);
    connect(ui->actionReload, &QAction::triggered, this, &FPwin::reload);
    connect(aGroup_, &QActionGroup::triggered, this, &FPwin::enforceEncoding);
    connect(ui->actionSave, &QAction::triggered, this, [this] { saveFile(false); });
    connect(ui->actionSaveAs, &QAction::triggered, this, [this] { saveFile(false); });
    connect(ui->actionSaveCodec, &QAction::triggered, this, [this] { saveFile(false); });
    connect(ui->actionSaveAllFiles, &QAction::triggered, this, [this] { saveAllFiles(true); });

    connect(ui->actionCut, &QAction::triggered, this, &FPwin::cutText);
    connect(ui->actionCopy, &QAction::triggered, this, &FPwin::copyText);
    connect(ui->actionPaste, &QAction::triggered, this, &FPwin::pasteText);
    connect(ui->actionSoftTab, &QAction::triggered, this, &FPwin::toSoftTabs);
    connect(ui->actionDate, &QAction::triggered, this, &FPwin::insertDate);
    connect(ui->actionDelete, &QAction::triggered, this, &FPwin::deleteText);
    connect(ui->actionSelectAll, &QAction::triggered, this, &FPwin::selectAllText);

    connect(ui->actionUpperCase, &QAction::triggered, this, &FPwin::upperCase);
    connect(ui->actionLowerCase, &QAction::triggered, this, &FPwin::lowerCase);
    connect(ui->actionStartCase, &QAction::triggered, this, &FPwin::startCase);

    connect(ui->menuEdit, &QMenu::aboutToShow, this, &FPwin::showingEditMenu);
    connect(ui->menuEdit, &QMenu::aboutToHide, this, &FPwin::hidngEditMenu);

    connect(ui->actionSortLines, &QAction::triggered, this, &FPwin::sortLines);
    connect(ui->actionRSortLines, &QAction::triggered, this, &FPwin::sortLines);

    connect(ui->ActionRmDupeSort, &QAction::triggered, this, &FPwin::rmDupeSort);
    connect(ui->ActionRmDupeRSort, &QAction::triggered, this, &FPwin::rmDupeSort);

    connect(ui->ActionSpaceDupeSort, &QAction::triggered, this, &FPwin::spaceDupeSort);
    connect(ui->ActionSpaceDupeRSort, &QAction::triggered, this, &FPwin::spaceDupeSort);

    connect(ui->actionEdit, &QAction::triggered, this, &FPwin::makeEditable);

    connect(ui->actionSession, &QAction::triggered, this, &FPwin::manageSessions);

    connect(ui->actionRun, &QAction::triggered, this, &FPwin::executeProcess);

    connect(ui->actionUndo, &QAction::triggered, this, &FPwin::undoing);
    connect(ui->actionRedo, &QAction::triggered, this, &FPwin::redoing);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &FPwin::onTabChanged);
    connect(ui->tabWidget, &TabWidget::currentTabChanged, this, &FPwin::tabSwitch);
    connect(ui->tabWidget, &TabWidget::hasLastActiveTab,
            [this](bool hasLastActive) { ui->actionLastActiveTab->setEnabled(hasLastActive); });

    /* the tab will be detached after the DND is finished */
    connect(ui->tabWidget->tabBar(), &TabBar::tabDetached, this, &FPwin::detachTab, Qt::QueuedConnection);

    connect(ui->tabWidget->tabBar(), &TabBar::hideTabBar, this, &FPwin::toggleSidePane);
    ui->tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tabWidget->tabBar(), &QWidget::customContextMenuRequested, this, &FPwin::tabContextMenu);
    connect(ui->actionCopyName, &QAction::triggered, this, &FPwin::copyTabFileName);
    connect(ui->actionCopyPath, &QAction::triggered, this, &FPwin::copyTabFilePath);
    connect(ui->actionCloseAll, &QAction::triggered, this, &FPwin::closeAllPages);
    connect(ui->actionCloseRight, &QAction::triggered, this, &FPwin::closeNextPages);
    connect(ui->actionCloseLeft, &QAction::triggered, this, &FPwin::closePreviousPages);
    connect(ui->actionCloseOther, &QAction::triggered, this, &FPwin::closeOtherPages);

    connect(ui->actionFont, &QAction::triggered, this, &FPwin::fontDialog);

    connect(ui->actionFind, &QAction::triggered, this, &FPwin::showHideSearch);
    connect(ui->actionJump, &QAction::triggered, this, &FPwin::jumpTo);
    connect(ui->spinBox, &QAbstractSpinBox::editingFinished, this, &FPwin::goTo);

    connect(ui->actionLineNumbers, &QAction::toggled, this, &FPwin::showLN);
    connect(ui->actionWrap, &QAction::triggered, this, &FPwin::toggleWrapping);
    connect(ui->actionSyntax, &QAction::triggered, this, &FPwin::toggleSyntaxHighlighting);
    connect(ui->actionIndent, &QAction::triggered, this, &FPwin::toggleIndent);

    connect(ui->actionPreferences, &QAction::triggered, this, &FPwin::prefDialog);

    connect(ui->actionCheckSpelling, &QAction::triggered, this, &FPwin::checkSpelling);
    connect(ui->actionUserDict, &QAction::triggered, this, &FPwin::userDict);

    connect(ui->actionReplace, &QAction::triggered, this, &FPwin::replaceDock);
    connect(ui->toolButtonNext, &QAbstractButton::clicked, this, &FPwin::replace);
    connect(ui->toolButtonPrv, &QAbstractButton::clicked, this, &FPwin::replace);
    connect(ui->toolButtonAll, &QAbstractButton::clicked, this, &FPwin::replaceAll);
    connect(ui->dockReplace, &QDockWidget::visibilityChanged, this, &FPwin::dockVisibilityChanged);
    connect(ui->dockReplace, &QDockWidget::topLevelChanged, this, &FPwin::resizeDock);

    connect(ui->actionDoc, &QAction::triggered, this, &FPwin::docProp);
    connect(ui->actionPrint, &QAction::triggered, this, &FPwin::filePrint);

    connect(ui->actionAbout, &QAction::triggered, this, &FPwin::aboutDialog);
    connect(ui->actionHelp, &QAction::triggered, this, &FPwin::helpDoc);

    connect(this, &FPwin::finishedLoading, [this] {
        if (sidePane_)
            sidePane_->listWidget()->scrollToCurrentItem();
    });
    ui->actionSidePane->setAutoRepeat(false);  // don't let UI change too rapidly
    connect(ui->actionSidePane, &QAction::triggered, [this] { toggleSidePane(); });

    /***************************************************************************
     *****     KDE (KAcceleratorManager) has a nasty "feature" that        *****
     *****   "smartly" gives mnemonics to tab and tool button texts so     *****
     *****   that, sometimes, the same mnemonics are disabled in the GUI   *****
     *****     and, as a result, their corresponding action shortcuts      *****
     *****     become disabled too. As a workaround, we don't set text     *****
     *****     for tool buttons on the search bar and replacement dock.    *****
     ***** The toolbar buttons and menu items aren't affected by this bug. *****
     ***************************************************************************/
    ui->toolButtonNext->setShortcut(QKeySequence(Qt::Key_F8));
    ui->toolButtonPrv->setShortcut(QKeySequence(Qt::Key_F9));
    ui->toolButtonAll->setShortcut(QKeySequence(Qt::Key_F10));

    QShortcut* zoomin = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);
    QShortcut* zoominPlus = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), this);
    QShortcut* zoomout = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
    QShortcut* zoomzero = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this);
    connect(zoomin, &QShortcut::activated, this, &FPwin::zoomIn);
    connect(zoominPlus, &QShortcut::activated, this, &FPwin::zoomIn);
    connect(zoomout, &QShortcut::activated, this, &FPwin::zoomOut);
    connect(zoomzero, &QShortcut::activated, this, &FPwin::zoomZero);

    QShortcut* fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreen, &QShortcut::activated, [this] { setWindowState(windowState() ^ Qt::WindowFullScreen); });

    QShortcut* focusView = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(focusView, &QShortcut::activated, this, &FPwin::focusView);

    QShortcut* focusSidePane = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Escape), this);
    connect(focusSidePane, &QShortcut::activated, this, &FPwin::focusSidePane);

    /* exiting a process */
    QShortcut* kill = new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_E), this);
    connect(kill, &QShortcut::activated, this, &FPwin::exitProcess);

    dummyWidget = new QWidget();
    setAcceptDrops(true);
    setAttribute(Qt::WA_AlwaysShowToolTips);
    setAttribute(Qt::WA_DeleteOnClose, false);  // we delete windows in singleton
}
/*************************/
FPwin::~FPwin() {
    startAutoSaving(false);
    delete dummyWidget;
    dummyWidget = nullptr;
    delete aGroup_;
    aGroup_ = nullptr;
    delete ui;
    ui = nullptr;
}
/*************************/
void FPwin::closeEvent(QCloseEvent* event) {
    FPsingleton* singleton = static_cast<FPsingleton*>(qApp);
    /* NOTE: With Qt6, "QCoreApplication::quit()" calls "closeEvent()" when the window is
             visible. But we want the app to quit without any prompt when receiving SIGTERM
             and similar signals. Here, we handle the situation by checking if a quit signal
             is received. This is also safe with Qt5. */
    if (singleton->isQuitSignalReceived()) {
        event->accept();
        return;
    }

    bool keep = locked_ || closePages(-1, -1, true);
    if (keep) {
        event->ignore();
        if (!locked_)
            lastWinFilesCur_.clear();  // just a precaution; it's done at closePages()
    }
    else {
        Config& config = singleton->getConfig();
        if (!isMaximized() && !isFullScreen()) {
            if (config.getRemSize())
                config.setWinSize(size());
            if (config.getRemPos() && !static_cast<FPsingleton*>(qApp)->isWayland())
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
// This method should be called only when the app quits without closing its windows
// (e.g., with SIGTERM). It saves the important info that can be queried only at the
// session end and, for now, covers cursor positions of sessions and last files.
void FPwin::cleanUpOnTerminating(Config& config, bool isLastWin) {
    /* WARNING: Qt5 has a bug that will cause a crash if "QDockWidget::visibilityChanged"
                isn't disconnected here. This is also good with Qt6. */
    disconnect(ui->dockReplace, &QDockWidget::visibilityChanged, this, &FPwin::dockVisibilityChanged);

    lastWinFilesCur_.clear();
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i))) {
            TextEdit* textEdit = tabPage->textEdit();
            QString fileName = textEdit->getFileName();
            if (!fileName.isEmpty()) {
                if (textEdit->getSaveCursor())
                    config.saveCursorPos(fileName, textEdit->textCursor().position());
                if (isLastWin && config.getSaveLastFilesList() && lastWinFilesCur_.size() < MAX_LAST_WIN_FILES &&
                    QFile::exists(fileName)) {
                    lastWinFilesCur_.insert(fileName, textEdit->textCursor().position());
                }
            }
        }
    }
    config.setLastFileCursorPos(lastWinFilesCur_);
}
/*************************/
void FPwin::toggleSidePane() {
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (sidePane_ == nullptr) {
        sidePane_ = new SidePane();
        ui->splitter->insertWidget(0, sidePane_);
        sidePane_->listWidget()->setFocus();
        ui->splitter->setStretchFactor(1, 1);  // only the text view can be

        connect(sidePane_, &SidePane::openFileRequested, this, [this](const QString& path) {
            const bool multiple = true;  // same behavior as opening several at once
            newTabFromName(path, /*line*/ 0, /*col*/ 0, multiple);
        });

        sidePane_->listWidget()->setFocus();

        QList<int> sizes;
        if (config.getRemSplitterPos()) {
            /* make sure that the side pane is visible and
               its width isn't greater than that of the view */
            sizes.append(std::min(std::max(16, config.getSplitterPos()), size().width() / 2));
            sizes.append(100);  // an arbitrary integer, because of stretching
        }
        else {
            /* don't let the side pane be wider than 1/5 of the window width */
            int s = std::min(size().width() / 5, 40 * sidePane_->fontMetrics().horizontalAdvance(' '));
            sizes << s << size().width() - s;
        }
        ui->splitter->setSizes(sizes);
        connect(sidePane_->listWidget(), &QWidget::customContextMenuRequested, this, &FPwin::listContextMenu);
        connect(sidePane_->listWidget(), &ListWidget::currentItemUpdated, this, &FPwin::changeTab);
        connect(sidePane_->listWidget(), &ListWidget::closeSidePane, this, &FPwin::toggleSidePane);
        connect(sidePane_->listWidget(), &ListWidget::closeItem, [this](QListWidgetItem* item) {
            if (!sideItems_.isEmpty())
                closeTabAtIndex(ui->tabWidget->indexOf(sideItems_.value(item)));
        });

        if (ui->tabWidget->count() > 0) {
            updateShortcuts(true);
            int curIndex = ui->tabWidget->currentIndex();
            ListWidget* lw = sidePane_->listWidget();
            for (int i = 0; i < ui->tabWidget->count(); ++i) {
                TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
                /* tab text can't be used because, on the one hand, it may be elided
                   and, on the other hand, KDE's auto-mnemonics may interfere */
                QString fname = tabPage->textEdit()->getFileName();
                bool isLink(false);
                bool hasFinalTarget(false);
                if (fname.isEmpty()) {
                    if (tabPage->textEdit()->getProg() == "help")
                        fname = "** " + tr("Help") + " **";
                    else
                        fname = tr("Untitled");
                }
                else {
                    QFileInfo info(fname);
                    isLink = info.isSymLink();
                    if (!isLink) {
                        const QString finalTarget = info.canonicalFilePath();
                        hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != fname);
                    }
                    fname = fname.section('/', -1);
                }
                if (tabPage->textEdit()->document()->isModified())
                    fname.append("*");
                fname.replace("\n", " ");
                ListWidgetItem* lwi = new ListWidgetItem(isLink           ? QIcon(":icons/link.svg")
                                                         : hasFinalTarget ? QIcon(":icons/hasTarget.svg")
                                                                          : QIcon(),
                                                         fname, lw);
                lwi->setToolTip(ui->tabWidget->tabToolTip(i));
                sideItems_.insert(lwi, tabPage);
                lw->addItem(lwi);
                if (i == curIndex)
                    lw->setCurrentItem(lwi);
            }
            sidePane_->listWidget()->scrollToCurrentItem();
            updateShortcuts(false);
        }

        disconnect(ui->actionLastTab, nullptr, this, nullptr);
        disconnect(ui->actionFirstTab, nullptr, this, nullptr);
        QString txt = ui->actionFirstTab->text();
        ui->actionFirstTab->setText(ui->actionLastTab->text());
        ui->actionLastTab->setText(txt);
        connect(ui->actionFirstTab, &QAction::triggered, this, &FPwin::lastTab);
        connect(ui->actionLastTab, &QAction::triggered, this, &FPwin::firstTab);
    }
    else {
        QList<int> sizes = ui->splitter->sizes();
        if (config.getRemSplitterPos())  // remember the position also when the side-pane is removed
            config.setSplitterPos(sizes.at(0));
        sideItems_.clear();
        delete sidePane_;
        sidePane_ = nullptr;
        bool hideSingleTab = config.getHideSingleTab();
        ui->tabWidget->tabBar()->hideSingle(hideSingleTab);
        if (!hideSingleTab || ui->tabWidget->count() > 1)
            ui->tabWidget->tabBar()->show();
        /* return focus to the document */
        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
            tabPage->textEdit()->setFocus();

        disconnect(ui->actionLastTab, nullptr, this, nullptr);
        disconnect(ui->actionFirstTab, nullptr, this, nullptr);
        QString txt = ui->actionFirstTab->text();
        ui->actionFirstTab->setText(ui->actionLastTab->text());
        ui->actionLastTab->setText(txt);
        connect(ui->actionLastTab, &QAction::triggered, this, &FPwin::lastTab);
        connect(ui->actionFirstTab, &QAction::triggered, this, &FPwin::firstTab);
    }
}
/*************************/
void FPwin::menubarTitle(bool add, bool setTitle) {
    QWidget* cw = ui->menuBar->cornerWidget();

    if (!add)  // removing the corner widget
    {
        if (cw == nullptr)
            return;
        ui->menuBar->setCornerWidget(nullptr);
        delete cw;
        return;
    }

    if (cw != nullptr || ui->menuBar->isHidden())
        return;
    MenuBarTitle* mbTitle = new MenuBarTitle();
    ui->menuBar->setCornerWidget(mbTitle);
    const auto menubarActions = ui->menuBar->actions();
    if (!menubarActions.isEmpty()) {
        QRect g = ui->menuBar->actionGeometry(menubarActions.last());
        mbTitle->setStart(QApplication::layoutDirection() == Qt::RightToLeft ? ui->menuBar->width() - g.left()
                                                                             : g.right() + 1);
        mbTitle->setHeight(g.height());
    }
    mbTitle->show();  // needed if the menubar is already visible, i.e., not at the startup
    connect(mbTitle, &QWidget::customContextMenuRequested, this, &FPwin::tabContextMenu);
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
void FPwin::applyConfigOnStarting() {
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();

    // geometry and window state
    if (config.getRemSize()) {
        resize(config.getWinSize());
        // on Wayland or when position isn't remembered, apply state now, otherwise showEvent handles it
        if (!config.getRemPos() || static_cast<FPsingleton*>(qApp)->isWayland()) {
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
        QObject::connect(ui->actionLastTab, &QAction::triggered, this, &FPwin::lastTab, Qt::UniqueConnection);
        QObject::connect(ui->actionFirstTab, &QAction::triggered, this, &FPwin::firstTab, Qt::UniqueConnection);
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
            QObject::connect(recentAction, &QAction::triggered, this, &FPwin::newTabFromRecent, Qt::UniqueConnection);
            ui->menuOpenRecently->addAction(recentAction);
        }
        ui->menuOpenRecently->addSeparator();
        ui->menuOpenRecently->addAction(ui->actionClearRecent);
        QObject::connect(ui->menuOpenRecently, &QMenu::aboutToShow, this, &FPwin::updateRecenMenu,
                         Qt::UniqueConnection);
        QObject::connect(ui->actionClearRecent, &QAction::triggered, this, &FPwin::clearRecentMenu,
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
        setIconTheme(ui->actionPrint, "document-print");
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
        setIconTheme(ui->actionHelp, "help-contents");
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
        setIconRes(ui->actionPrint, ":icons/document-print.svg");
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
        setIconRes(ui->actionHelp, ":icons/help-contents.svg");
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
    setWindowIcon(QIcon::fromTheme("featherpad", QIcon(":icons/featherpad.svg")));

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
        for (const auto& action : menubarActions) {
            QString txt = action->text();
            txt.remove(QRegularExpression(QStringLiteral("\\s*\\(&[a-zA-Z0-9]\\)\\s*")));  // zh ja and similar
            txt.remove(QLatin1Char('&'));                                                  // other locales
            action->setText(txt);
        }
    }
}

/*************************/
void FPwin::addCursorPosLabel() {
    if (ui->statusBar->findChild<QLabel*>("posLabel"))
        return;
    QLabel* posLabel = new QLabel();
    posLabel->setObjectName("posLabel");
    posLabel->setText("<b>" + tr("Position:") + "</b>");
    posLabel->setIndent(2);
    posLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->statusBar->addPermanentWidget(posLabel);
}
/*************************/
void FPwin::addRemoveLangBtn(bool add) {
    static QStringList langList;
    if (langList.isEmpty()) {  // no "url" for the language button
        langList << "c" << "cmake" << "config" << "cpp" << "css"
                 << "dart" << "deb" << "diff" << "fountain" << "html"
                 << "java" << "javascript" << "json" << "LaTeX" << "go"
                 << "log" << "lua" << "m3u" << "markdown" << "makefile"
                 << "pascal" << "perl" << "php" << "python" << "qmake"
                 << "qml" << "reST" << "ruby" << "rust" << "scss"
                 << "sh" << "tcl" << "toml" << "troff" << "xml" << "yaml";
        langList.sort(Qt::CaseInsensitive);
    }

    QToolButton* langButton = ui->statusBar->findChild<QToolButton*>("langButton");
    if (!add) {
        langs_.clear();
        if (langButton) {
            delete langButton;  // deletes the menu and its actions
            langButton = nullptr;
        }

        for (int i = 0; i < ui->tabWidget->count(); ++i) {
            TextEdit* textEdit = qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit();
            if (!textEdit->getLang().isEmpty()) {
                textEdit->setLang(QString());  // remove the enforced syntax
                if (ui->actionSyntax->isChecked()) {
                    syntaxHighlighting(textEdit, false);
                    syntaxHighlighting(textEdit);
                }
            }
        }
    }
    else if (!langButton && langs_.isEmpty())  // not needed; we clear it on removing the button
    {
        QString normal = tr("Normal");
        langButton = new QToolButton();
        langButton->setObjectName("langButton");
        langButton->setFocusPolicy(Qt::NoFocus);
        langButton->setAutoRaise(true);
        langButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
        langButton->setText(normal);
        langButton->setPopupMode(QToolButton::InstantPopup);

        /* a searchable menu */
        class Menu : public QMenu {
           public:
            Menu(QWidget* parent = nullptr) : QMenu(parent) { selectionTimer_ = nullptr; }
            ~Menu() {
                if (selectionTimer_) {
                    if (selectionTimer_->isActive())
                        selectionTimer_->stop();
                    delete selectionTimer_;
                }
            }

           protected:
            void keyPressEvent(QKeyEvent* e) override {
                if (selectionTimer_ == nullptr) {
                    selectionTimer_ = new QTimer();
                    connect(selectionTimer_, &QTimer::timeout, this, [this] {
                        if (txt_.isEmpty())
                            return;
                        const auto allActions = actions();
                        for (const auto& a : allActions) {  // search in starting strings first
                            QString aTxt = a->text();
                            aTxt.remove('&');
                            if (aTxt.startsWith(txt_, Qt::CaseInsensitive)) {
                                setActiveAction(a);
                                txt_.clear();
                                return;
                            }
                        }
                        for (const auto& a : allActions) {  // now, search for containing strings
                            QString aTxt = a->text();
                            aTxt.remove('&');
                            if (aTxt.contains(txt_, Qt::CaseInsensitive)) {
                                setActiveAction(a);
                                break;
                            }
                        }
                        txt_.clear();
                    });
                }
                selectionTimer_->start(600);
                txt_ += e->text().simplified();
                QMenu::keyPressEvent(e);
            }

           private:
            QTimer* selectionTimer_;
            QString txt_;
        };

        Menu* menu = new Menu(langButton);
        QActionGroup* aGroup = new QActionGroup(langButton);
        QAction* action;
        for (int i = 0; i < langList.count(); ++i) {
            QString lang = langList.at(i);
            action = menu->addAction(lang);
            action->setCheckable(true);
            action->setActionGroup(aGroup);
            langs_.insert(lang, action);
        }
        menu->addSeparator();
        action = menu->addAction(normal);
        action->setCheckable(true);
        action->setActionGroup(aGroup);
        langs_.insert(normal, action);

        langButton->setMenu(menu);

        ui->statusBar->insertPermanentWidget(2, langButton);
        connect(aGroup, &QActionGroup::triggered, this, &FPwin::enforceLang);

        /* update the language button if this is called from outside c-tor
           (otherwise, tabswitch() will do it) */
        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
            updateLangBtn(tabPage->textEdit());
    }
}
/*************************/
// We want all dialogs to be window-modal as far as possible. However there is a problem:
// If a dialog is opened in a FeatherPad window and is closed after another dialog is
// opened in another window, the second dialog will be seen as a child of the first window.
// This could cause a crash if the dialog is closed after closing the first window.
// As a workaround, we keep window-modality but don't let the user open two window-modal dialogs.
bool FPwin::hasAnotherDialog() {
    closeWarningBar();
    bool res = false;
    FPsingleton* singleton = static_cast<FPsingleton*>(qApp);
    for (int i = 0; i < singleton->Wins.count(); ++i) {
        FPwin* win = singleton->Wins.at(i);
        if (win != this) {
            QList<QDialog*> dialogs = win->findChildren<QDialog*>();
            for (int j = 0; j < dialogs.count(); ++j) {
                if (dialogs.at(j)->isModal()) {
                    res = true;
                    break;
                }
            }
            if (res)
                break;
        }
    }
    if (res) {
        showWarningBar("<center><b><big>" + tr("Another FeatherPad window has a modal dialog!") +
                           "</big></b></center>" + "<center><i>" +
                           tr("Please attend to that window or just close its dialog!") + "</i></center>",
                       15);
    }
    return res;
}
/*************************/
void FPwin::updateGUIForSingleTab(bool single) {
    ui->actionDetachTab->setEnabled(!single && !static_cast<FPsingleton*>(qApp)->isStandAlone());
    ui->actionRightTab->setEnabled(!single);
    ui->actionLeftTab->setEnabled(!single);
    ui->actionLastTab->setEnabled(!single);
    ui->actionFirstTab->setEnabled(!single);
}
/*************************/
void FPwin::deleteTabPage(int tabIndex, bool saveToList, bool closeWithLastTab) {
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(tabIndex));
    if (tabPage == nullptr)
        return;
    if (sidePane_ && !sideItems_.isEmpty()) {
        if (QListWidgetItem* wi = sideItems_.key(tabPage)) {
            sideItems_.remove(wi);
            delete sidePane_->listWidget()->takeItem(sidePane_->listWidget()->row(wi));
        }
    }
    TextEdit* textEdit = tabPage->textEdit();
    QString fileName = textEdit->getFileName();
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (!fileName.isEmpty()) {
        if (textEdit->getSaveCursor())
            config.saveCursorPos(fileName, textEdit->textCursor().position());
        if (saveToList && config.getSaveLastFilesList() && QFile::exists(fileName))
            lastWinFilesCur_.insert(fileName, textEdit->textCursor().position());
    }
    /* because deleting the syntax highlighter changes the text,
       it is better to disconnect contentsChange() here to prevent a crash */
    disconnect(textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
    disconnect(textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
    if (config.getSelectionHighlighting())
        disconnect(textEdit->document(), &QTextDocument::contentsChange, textEdit, &TextEdit::onContentsChange);
    syntaxHighlighting(textEdit, false);
    ui->tabWidget->removeTab(tabIndex);
    delete tabPage;
    tabPage = nullptr;
    if (closeWithLastTab && config.getCloseWithLastTab() && ui->tabWidget->count() == 0)
        close();
}
/*************************/
// Here, "first" is the index/row, to whose right/bottom all tabs/rows are to be closed
// Similarly, "last" is the index/row, to whose left/top all tabs/rows are to be closed
// A negative value means including the start for "first" and the end for "last"
// If both "first" and "last" are negative, all tabs will be closed
// The case, when they're both greater than -1, is covered but not used anywhere
// Tabs/rows are always closed from right/bottom to left/top
bool FPwin::closePages(int first, int last, bool saveFilesList) {
    if (!isReady()) {
        closePreviousPages_ = false;
        return true;
    }

    // RAII guards for autosave pause and making sure we unbusy on all paths
    struct AutoPause {
        FPwin* self;
        explicit AutoPause(FPwin* s) : self(s) { self->pauseAutoSaving(true); }
        ~AutoPause() { self->pauseAutoSaving(false); }
    } autoPause(this);

    struct FinallyUnbusy {
        FPwin* self;
        explicit FinallyUnbusy(FPwin* s) : self(s) {}
        ~FinallyUnbusy() { self->unbusy(); }
    } finallyUnbusy(this);

    const bool hasSideList = (sidePane_ && !sideItems_.isEmpty());

    // remember current page or side item to restore if user cancels
    TabPage* curPage = nullptr;
    QListWidgetItem* curItem = nullptr;
    if (hasSideList) {
        const int cur = sidePane_->listWidget()->currentRow();
        if (!(first < cur && (cur < last || last < 0)))
            curItem = sidePane_->listWidget()->currentItem();
    }
    else {
        const int cur = ui->tabWidget->currentIndex();
        if (!(first < cur && (cur < last || last < 0)))
            curPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    }

    // helpers
    auto mapIndexToTabIndex = [&](int i) -> int {
        return hasSideList ? ui->tabWidget->indexOf(sideItems_.value(sidePane_->listWidget()->item(i))) : i;
    };

    auto updateGuiAfterClose = [&]() {
        const int count = ui->tabWidget->count();
        if (count == 0) {
            ui->actionReload->setDisabled(true);
            ui->actionSave->setDisabled(true);
            enableWidgets(false);
        }
        else if (count == 1) {
            updateGUIForSingleTab(true);
        }
    };

    auto capSaveFilesList = [&]() {
        if (lastWinFilesCur_.size() >= MAX_LAST_WIN_FILES)  // never remember more than MAX_LAST_WIN_FILES
            saveFilesList = false;
    };

    auto nextRightmostIndex = [&](int currentLast, int currentFirst) -> int {
        if (currentLast == 0)  // nothing on the left/top to close
            return -1;         // signal to stop
        if (currentLast < 0)   // close from the end
            return ui->tabWidget->count() - 1;
        if (currentFirst >= currentLast - 1)  // nothing remains in range
            return -1;
        return currentLast - 1;  // last is 1-based boundary, so pick the item before it
    };

    bool keep = false;
    bool closing = saveFilesList;  // saveFilesList is true only with closing
    DOCSTATE state = SAVED;

    while (state == SAVED && ui->tabWidget->count() > 0) {
        makeBusy();

        int index = nextRightmostIndex(last, first);
        if (index < 0)
            break;

        int tabIndex = mapIndexToTabIndex(index);

        // show No-to-all only if more than one is expected and we are not already in no-prompt continuation
        const bool multiple = !(first == index - 1 && !closePreviousPages_);
        state = savePrompt(tabIndex, multiple, first, last, closing);

        switch (state) {
            case SAVED: {
                keep = false;
                capSaveFilesList();
                deleteTabPage(tabIndex, saveFilesList, !closing);

                if (last > -1)  // last is a left/top boundary that shifts as we delete
                    --last;

                updateGuiAfterClose();
                break;
            }
            case UNDECIDED: {
                keep = true;
                if (!locked_)
                    lastWinFilesCur_.clear();
                break;
            }
            case DISCARDED: {
                keep = false;

                // close everything within (first, last) without further prompts
                while (true) {
                    int idx = nextRightmostIndex(last, first);
                    if (idx < 0)
                        break;

                    int t = mapIndexToTabIndex(idx);
                    capSaveFilesList();
                    deleteTabPage(t, saveFilesList, !closing);

                    if (last > -1)
                        --last;
                }

                updateGuiAfterClose();

                if (closePreviousPages_) {  // continue closing previous pages without prompt
                    closePreviousPages_ = false;
                    if (first > 0) {
                        int idx = first - 1;
                        while (idx > -1) {
                            int t = mapIndexToTabIndex(idx);
                            capSaveFilesList();
                            deleteTabPage(t, saveFilesList, !closing);
                            --idx;
                        }
                        updateGuiAfterClose();
                    }
                    return false;
                }
                break;
            }
            default:
                break;
        }
    }

    // restore the current page/item if nothing special to keep
    if (!keep) {
        if (curPage)
            ui->tabWidget->setCurrentWidget(curPage);
        else if (curItem)
            sidePane_->listWidget()->setCurrentItem(curItem);

        if (closePreviousPages_) {  // continue closing previous pages after restoring selection
            closePreviousPages_ = false;
            return closePages(-1, first);
        }
    }

    return keep;
}
/*************************/
void FPwin::copyTabFileName() {
    if (rightClicked_ < 0)
        return;
    TabPage* tabPage = nullptr;
    if (sidePane_)
        tabPage = sideItems_.value(sidePane_->listWidget()->item(rightClicked_));
    else
        tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(rightClicked_));
    if (tabPage) {
        QString fname = tabPage->textEdit()->getFileName();
        QApplication::clipboard()->setText(fname.section('/', -1));
    }
}
/*************************/
void FPwin::copyTabFilePath() {
    if (rightClicked_ < 0)
        return;
    TabPage* tabPage = nullptr;
    if (sidePane_)
        tabPage = sideItems_.value(sidePane_->listWidget()->item(rightClicked_));
    else
        tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(rightClicked_));
    if (tabPage) {
        QString str = tabPage->textEdit()->getFileName();
        if (!str.isEmpty())
            QApplication::clipboard()->setText(str);
    }
}
/*************************/
void FPwin::closeAllPages() {
    closePages(-1, -1);
}
/*************************/
void FPwin::closeNextPages() {
    closePages(rightClicked_, -1);
}
/*************************/
void FPwin::closePreviousPages() {
    closePages(-1, rightClicked_);
}
/*************************/
void FPwin::closeOtherPages() {
    /* NOTE: Because saving as root is possible, we can't close the previous pages
             here. They will be closed by closePages() if needed. */
    closePreviousPages_ = true;
    closePages(rightClicked_, -1);
}
/*************************/
void FPwin::dragEnterEvent(QDragEnterEvent* event) {
    if (locked_ || findChildren<QDialog*>().count() > 0)
        return;
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    /* check if this comes from one of our windows (and not from a root instance, for example) */
    else if (event->mimeData()->hasFormat("application/featherpad-tab") && event->source() != nullptr) {
        event->acceptProposedAction();
    }
}
/*************************/
void FPwin::dropEvent(QDropEvent* event) {
    if (locked_)
        return;
    if (event->mimeData()->hasFormat("application/featherpad-tab")) {
        if (QObject* sourseObject = event->source()) {
            /* announce that the drop is accepted by us (see "TabBar::mouseMoveEvent") */
            sourseObject->setProperty(TabBar::tabDropped, true);
            /* the tab will be dropped after the DND is finished */
            auto data = event->mimeData()->data("application/featherpad-tab");
            QTimer::singleShot(0, sourseObject, [this, sourseObject, data]() {
                dropTab(QString::fromUtf8(data.constData()), sourseObject);
            });
        }
    }
    else {
        const QList<QUrl> urlList = event->mimeData()->urls();
        bool multiple(urlList.count() > 1 || isLoading());
        for (const QUrl& url : urlList) {
            QString file;
            QString scheme = url.scheme();
            if (scheme == "admin")  // gvfs' "admin:///"
                file = url.adjusted(QUrl::NormalizePathSegments).path();
            else if (scheme == "file" || scheme.isEmpty())
                file = url.adjusted(QUrl::NormalizePathSegments)  // KDE may give a double slash
                           .toLocalFile();
            else
                continue;
            newTabFromName(file, 0, 0, multiple);
        }
    }

    event->acceptProposedAction();
}
/*************************/
// This method checks if there's any text that isn't saved under a tab and,
// if there is, it activates the tab and shows an appropriate prompt dialog.
// "tabIndex" is always the tab index and not the item row (in the side-pane).

// The other variables are only for saveAsRoot(): "first and "last" determine
// the range of indexes/rows that should be closed and "curItem"/"curPage" is
// the item/tab that should be made current in side-pane/tab-bar again.
FPwin::DOCSTATE FPwin::savePrompt(int tabIndex,
                                  bool noToAll,
                                  int first,
                                  int last,
                                  bool closingWindow,
                                  QListWidgetItem* curItem,
                                  TabPage* curPage) {
    DOCSTATE state = SAVED;
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(tabIndex));
    if (tabPage == nullptr)
        return state;
    TextEdit* textEdit = tabPage->textEdit();
    QString fname = textEdit->getFileName();
    bool isRemoved(!fname.isEmpty() && !QFile::exists(fname));  // don't check QFileInfo (fname).isFile()
    if (textEdit->document()->isModified() || isRemoved) {
        unbusy();                  // made busy at closePages()
        if (hasAnotherDialog()) {  // cancel
            closePreviousPages_ = false;
            return UNDECIDED;
        }

        if (tabIndex != ui->tabWidget->currentIndex()) {  // switch to the page that needs attention
            if (sidePane_ && !sideItems_.isEmpty())
                sidePane_->listWidget()->setCurrentItem(
                    sideItems_.key(tabPage));  // sets the current widget at changeTab()
            else
                ui->tabWidget->setCurrentIndex(tabIndex);
        }

        updateShortcuts(true);

        MessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setText("<center><b><big>" + tr("Save changes?") + "</big></b></center>");
        if (isRemoved)
            msgBox.setInformativeText("<center><i>" + tr("The file does not exist.") + "</i></center>");
        else
            msgBox.setInformativeText("<center><i>" + tr("The document has been modified.") + "</i></center>");
        if (noToAll && ui->tabWidget->count() > 1)
            msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel |
                                      QMessageBox::NoToAll);
        else
            msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.changeButtonText(QMessageBox::Save, tr("&Save"));
        msgBox.changeButtonText(QMessageBox::Discard, tr("&Discard changes"));
        msgBox.changeButtonText(QMessageBox::Cancel, tr("&Cancel"));
        if (noToAll)
            msgBox.changeButtonText(QMessageBox::NoToAll, tr("&No to all"));
        msgBox.setDefaultButton(QMessageBox::Save);
        msgBox.setWindowModality(Qt::WindowModal);
        /* enforce a central position */
        /*msgBox.show();
        msgBox.move (x() + width()/2 - msgBox.width()/2,
                     y() + height()/2 - msgBox.height()/ 2);*/
        switch (msgBox.exec()) {
            case QMessageBox::Save:
                if (!saveFile(true, first, last, closingWindow, curItem, curPage)) {
                    state = UNDECIDED;
                    /* NOTE: closePreviousPages_ is set to false by saveFile() if there is no
                             root saving; otherwise, it's set by saveAsRoot() appropriately. */
                }
                break;
            case QMessageBox::Discard:
                break;
            case QMessageBox::Cancel:
                state = UNDECIDED;
                closePreviousPages_ = false;
                break;
            case QMessageBox::NoToAll:
                state = DISCARDED;
                break;
            default:
                state = UNDECIDED;
                break;
        }

        updateShortcuts(false);
    }
    return state;
}
/*************************/
// Enable or disable some widgets.
void FPwin::enableWidgets(bool enable) const {
    if (!enable && ui->dockReplace->isVisible())
        ui->dockReplace->setVisible(false);
    if (!enable && ui->spinBox->isVisible()) {
        ui->spinBox->setVisible(false);
        ui->label->setVisible(false);
        ui->checkBox->setVisible(false);
    }
    if ((!enable && ui->statusBar->isVisible()) ||
        (enable && static_cast<FPsingleton*>(qApp)->getConfig().getShowStatusbar()))  // starting from no tab
    {
        ui->statusBar->setVisible(enable);
    }

    ui->actionSelectAll->setEnabled(enable);
    ui->actionFind->setEnabled(enable);
    ui->actionJump->setEnabled(enable);
    ui->actionReplace->setEnabled(enable);
    ui->actionClose->setEnabled(enable);
    ui->actionSaveAs->setEnabled(enable);
    ui->actionSaveAllFiles->setEnabled(enable);
    ui->actionSaveCodec->setEnabled(enable);
    ui->menuEncoding->setEnabled(enable);
    ui->actionFont->setEnabled(enable);
    ui->actionDoc->setEnabled(enable);
    ui->actionPrint->setEnabled(enable);

    if (!enable) {
        ui->actionUndo->setEnabled(false);
        ui->actionRedo->setEnabled(false);

        ui->actionEdit->setVisible(false);
        ui->actionRun->setVisible(false);

        ui->actionCut->setEnabled(false);
        ui->actionCopy->setEnabled(false);
        ui->actionPaste->setEnabled(false);
        ui->actionSoftTab->setEnabled(false);
        ui->actionDate->setEnabled(false);
        ui->actionDelete->setEnabled(false);

        ui->actionUpperCase->setEnabled(false);
        ui->actionLowerCase->setEnabled(false);
        ui->actionStartCase->setEnabled(false);
    }
}
/*************************/
void FPwin::updateCustomizableShortcuts(bool disable) {
    QHash<QAction*, QKeySequence>::const_iterator iter = defaultShortcuts_.constBegin();
    if (disable) {  // remove shortcuts
        while (iter != defaultShortcuts_.constEnd()) {
            iter.key()->setShortcut(QKeySequence());
            ++iter;
        }
    }
    else {  // restore shortcuts
        QHash<QString, QString> ca = static_cast<FPsingleton*>(qApp)->getConfig().customShortcutActions();
        QList<QString> cn = ca.keys();

        while (iter != defaultShortcuts_.constEnd()) {
            const QString name = iter.key()->objectName();
            iter.key()->setShortcut(cn.contains(name) ? QKeySequence(ca.value(name), QKeySequence::PortableText)
                                                      : iter.value());
            ++iter;
        }
    }
}
/*************************/
// When a window-modal dialog is shown, Qt doesn't disable the main window shortcuts.
// This is definitely a bug in Qt. As a workaround, we use this function to disable
// all shortcuts on showing a dialog and to enable them again on hiding it.
// The searchbar shortcuts of the current tab page are handled separately.
//
// This function also updates shortcuts after they're customized in the Preferences dialog.
void FPwin::updateShortcuts(bool disable, bool page) {
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

    if (page)  // disable/enable searchbar shortcuts of the current page too
    {
        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
            tabPage->updateShortcuts(disable);
    }
}
/*************************/
void FPwin::newTab() {
    createEmptyTab(!isLoading());
}
/*************************/
TabPage* FPwin::createEmptyTab(bool setCurrent, bool allowNormalHighlighter) {
    FPsingleton* singleton = static_cast<FPsingleton*>(qApp);
    Config config = singleton->getConfig();

    static const QList<QKeySequence> searchShortcuts = {QKeySequence(Qt::Key_F3), QKeySequence(Qt::Key_F4),
                                                        QKeySequence(Qt::Key_F5), QKeySequence(Qt::Key_F6),
                                                        QKeySequence(Qt::Key_F7)};
    TabPage* tabPage =
        new TabPage(config.getDarkColScheme() ? config.getDarkBgColorValue() : config.getLightBgColorValue(),
                    searchShortcuts, nullptr);
    tabPage->setSearchModel(singleton->searchModel());
    TextEdit* textEdit = tabPage->textEdit();
    connect(textEdit, &QWidget::customContextMenuRequested, this, &FPwin::editorContextMenu);
    textEdit->setSelectionHighlighting(config.getSelectionHighlighting());
    textEdit->setPastePaths(config.getPastePaths());
    textEdit->setAutoReplace(config.getAutoReplace());
    textEdit->setAutoBracket(config.getAutoBracket());
    textEdit->setTtextTab(config.getTextTabSize());
    textEdit->setCurLineHighlight(config.getCurLineHighlight());
    textEdit->setEditorFont(config.getFont());
    textEdit->setInertialScrolling(config.getInertialScrolling());
    textEdit->setDateFormat(config.getDateFormat());
    if (config.getThickCursor())
        textEdit->setThickCursor(true);
    if (config.getTextMargin()) {
        textEdit->document()->setDocumentMargin(12);
        textEdit->document()->setModified(false);
    }

    if (allowNormalHighlighter && ui->actionSyntax->isChecked())
        syntaxHighlighting(textEdit);  // the default (url) syntax highlighter

    int index = ui->tabWidget->currentIndex();
    if (index == -1)
        enableWidgets(true);

    /* hide the searchbar consistently */
    if ((index == -1 && config.getHideSearchbar()) ||
        (index > -1 && !qobject_cast<TabPage*>(ui->tabWidget->widget(index))->isSearchBarVisible())) {
        tabPage->setSearchBarVisible(false);
    }

    ui->tabWidget->insertTab(index + 1, tabPage, tr("Untitled"));

    /* set all preliminary properties */
    if (index >= 0)
        updateGUIForSingleTab(false);
    ui->tabWidget->setTabToolTip(index + 1, tr("Unsaved"));
    if (!ui->actionWrap->isChecked())
        textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    if (!ui->actionIndent->isChecked())
        textEdit->setAutoIndentation(false);
    if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
        textEdit->showLineNumbers(true);
    if (ui->spinBox->isVisible())
        connect(textEdit->document(), &QTextDocument::blockCountChanged, this, &FPwin::setMax);
    if (ui->statusBar->isVisible() ||
        config.getShowStatusbar())  // when the main window is being created, isVisible() isn't set yet
    {
        /* If this becomes the current tab, "tabSwitch()" will take care of the status label,
           the word button and the cursor position label. */

        connect(textEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
        connect(textEdit, &TextEdit::selChanged, this, &FPwin::statusMsg);
        if (config.getShowCursorPos())
            connect(textEdit, &QPlainTextEdit::cursorPositionChanged, this, &FPwin::showCursorPos);
    }
    connect(textEdit->document(), &QTextDocument::undoAvailable, ui->actionUndo, &QAction::setEnabled);
    connect(textEdit->document(), &QTextDocument::redoAvailable, ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        connect(textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::enableSaving);
    connect(textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::asterisk);
    connect(textEdit, &TextEdit::canCopy, ui->actionCut, &QAction::setEnabled);
    connect(textEdit, &TextEdit::canCopy, ui->actionDelete, &QAction::setEnabled);
    connect(textEdit, &TextEdit::canCopy, ui->actionCopy, &QAction::setEnabled);
    connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionUpperCase, &QAction::setEnabled);
    connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionLowerCase, &QAction::setEnabled);
    connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionStartCase, &QAction::setEnabled);

    connect(textEdit, &TextEdit::filePasted, this, &FPwin::newTabFromName);
    connect(textEdit, &TextEdit::zoomedOut, this, &FPwin::reformat);
    connect(textEdit, &TextEdit::hugeColumn, this, &FPwin::columnWarning);

    connect(tabPage, &TabPage::find, this, &FPwin::find);
    connect(tabPage, &TabPage::searchFlagChanged, this, &FPwin::searchFlagChanged);

    /* I don't know why, under KDE, when a text is selected for the first time,
       it may not be copied to the selection clipboard. Perhaps it has something
       to do with Klipper. I neither know why the following line is a workaround
       but it can cause a long delay when FeatherPad is started. */
    // QApplication::clipboard()->text (QClipboard::Selection);

    if (sidePane_) {
        ListWidget* lw = sidePane_->listWidget();
        ListWidgetItem* lwi = new ListWidgetItem(tr("Untitled"), lw);
        lwi->setToolTip(tr("Unsaved"));
        sideItems_.insert(lwi, tabPage);
        lw->addItem(lwi);
        if (setCurrent || index == -1)  // for tabs, it's done automatically
        {
            lw->setCurrentItem(lwi);
        }
    }

    if (setCurrent) {
        ui->tabWidget->setCurrentWidget(tabPage);
        textEdit->setFocus();
    }

    if (setCurrent)
        stealFocus();
    else if (isMinimized())
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
#ifdef HAS_X11
    else if (static_cast<FPsingleton*>(qApp)->isX11()) {
        if (isWindowShaded(winId()))
            unshadeWindow(winId());
    }
#endif

    return tabPage;
}
/*************************/
void FPwin::editorContextMenu(const QPoint& p) {
    /* NOTE: The editor's customized context menu comes here (and not in
             the TextEdit class) for not duplicating actions, although that
             requires extra signal connections and disconnections on tab DND. */

    TextEdit* textEdit = qobject_cast<TextEdit*>(QObject::sender());
    if (textEdit == nullptr)
        return;

    /* Announce that the mouse button is released, because "TextEdit::mouseReleaseEvent"
       is not called when the context menu is shown. This is only needed for removing the
       column highlight on changing the cursor position after opening the context menu. */
    QTimer::singleShot(0, textEdit, [textEdit]() { textEdit->releaseMouse(); });

    /* put the cursor at the right-click position if it has no selection */
    if (!textEdit->textCursor().hasSelection())
        textEdit->setTextCursor(textEdit->cursorForPosition(p));

    QMenu* menu = textEdit->createStandardContextMenu(p);
    const QList<QAction*> actions = menu->actions();
    if (!actions.isEmpty()) {
        bool hasColumn = !textEdit->getColSel().isEmpty();
        for (QAction* const thisAction : actions) {
            /* remove the shortcut strings because shortcuts may change */
            QString txt = thisAction->text();
            if (!txt.isEmpty())
                txt = txt.split('\t').first();
            if (!txt.isEmpty())
                thisAction->setText(txt);
            /* correct the slots of some actions */
            if (thisAction->objectName() == "edit-copy") {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::copy);
                if (hasColumn && !thisAction->isEnabled())
                    thisAction->setEnabled(true);
            }
            else if (thisAction->objectName() == "edit-cut") {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::cut);
                if (hasColumn && !thisAction->isEnabled())
                    thisAction->setEnabled(true);
            }
            else if (thisAction->objectName() == "edit-paste") {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::paste);
                /* also, correct the enabled state of the paste action by consulting our
                   "TextEdit::pastingIsPossible()" instead of "QPlainTextEdit::canPaste()" */
                thisAction->setEnabled(textEdit->pastingIsPossible());
            }
            else if (thisAction->objectName() == "edit-delete") {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::deleteText);
                if (hasColumn && !thisAction->isEnabled())
                    thisAction->setEnabled(true);
            }
            else if (thisAction->objectName() == "edit-undo") {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::undo);
            }
            else if (thisAction->objectName() == "edit-redo") {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::redo);
            }
            else if (thisAction->objectName() == "select-all") {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::selectAll);
            }
        }
        QString str = textEdit->getUrl(textEdit->textCursor().position());
        if (!str.isEmpty()) {
            QAction* sep = menu->insertSeparator(actions.first());
            QAction* openLink = new QAction(tr("Open Link"), menu);
            menu->insertAction(sep, openLink);
            connect(openLink, &QAction::triggered, [str] {
                QUrl url(str);
                if (url.isRelative())
                    url = QUrl::fromUserInput(str, "/");
                /* QDesktopServices::openUrl() may resort to "xdg-open", which isn't
                   the best choice. "gio" is always reliable, so we check it first. */
                if (QStandardPaths::findExecutable("gio").isEmpty() ||
                    !QProcess::startDetached("gio", QStringList() << "open" << url.toString())) {
                    QDesktopServices::openUrl(url);
                }
            });
            if (str.startsWith("mailto:"))  // see getUrl()
                str.remove(0, 7);
            QAction* copyLink = new QAction(tr("Copy Link"), menu);
            menu->insertAction(sep, copyLink);
            connect(copyLink, &QAction::triggered, [str] { QApplication::clipboard()->setText(str); });
        }
        menu->addSeparator();
    }
    if (!textEdit->isReadOnly()) {
        menu->addAction(ui->actionSoftTab);
        menu->addSeparator();
        if (textEdit->textCursor().hasSelection()) {
            menu->addAction(ui->actionUpperCase);
            menu->addAction(ui->actionLowerCase);
            menu->addAction(ui->actionStartCase);
            if (textEdit->textCursor().selectedText().contains(QChar(QChar::ParagraphSeparator))) {
                menu->addSeparator();
                ui->actionSortLines->setEnabled(true);
                ui->actionRSortLines->setEnabled(true);
                ui->ActionRmDupeSort->setEnabled(true);
                ui->ActionRmDupeRSort->setEnabled(true);
                ui->ActionSpaceDupeSort->setEnabled(true);
                ui->ActionSpaceDupeRSort->setEnabled(true);
                menu->addAction(ui->actionSortLines);
                menu->addAction(ui->actionRSortLines);
                menu->addAction(ui->ActionRmDupeSort);
                menu->addAction(ui->ActionRmDupeRSort);
                menu->addAction(ui->ActionSpaceDupeSort);
                menu->addAction(ui->ActionSpaceDupeRSort);
            }
            menu->addSeparator();
        }
        menu->addAction(ui->actionCheckSpelling);
        menu->addSeparator();
        menu->addAction(ui->actionDate);
    }
    else
        menu->addAction(ui->actionCheckSpelling);

    menu->exec(textEdit->viewport()->mapToGlobal(p));
    delete menu;
}
/*************************/
void FPwin::updateRecenMenu() {
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    QStringList recentFiles = config.getRecentFiles();
    int recentSize = recentFiles.count();
    int recentNumber = config.getCurRecentFilesNumber();
    QList<QAction*> actions = ui->menuOpenRecently->actions();
    QFontMetrics metrics(ui->menuOpenRecently->font());
    int w = 150 * metrics.horizontalAdvance(' ');
    QMimeDatabase mimeDatabase;
    for (int i = 0; i < recentNumber; ++i) {
        if (i < recentSize) {
            actions.at(i)->setText(metrics.elidedText(recentFiles.at(i), Qt::ElideMiddle, w));
            QIcon icon;
            auto mimes = mimeDatabase.mimeTypesForFileName(recentFiles.at(i).section("/", -1));
            if (!mimes.isEmpty())
                icon = QIcon::fromTheme(mimes.at(0).iconName());
            actions.at(i)->setIcon(icon);
            actions.at(i)->setData(recentFiles.at(i));
            actions.at(i)->setVisible(true);
        }
        else {
            actions.at(i)->setText(QString());
            actions.at(i)->setIcon(QIcon());
            actions.at(i)->setData(QVariant());
            actions.at(i)->setVisible(false);
        }
    }
    ui->actionClearRecent->setEnabled(recentSize != 0);
}
/*************************/
void FPwin::clearRecentMenu() {
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.clearRecentFiles();
    updateRecenMenu();
}
/*************************/
void FPwin::addRecentFile(const QString& file) {
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.addRecentFile(file);

    /* also, try to make other windows know about this file */
    FPsingleton* singleton = static_cast<FPsingleton*>(qApp);
    if (singleton->isStandAlone())
        singleton->sendRecentFile(file, config.getRecentOpened());
}
/*************************/
void FPwin::reformat(TextEdit* textEdit) {
    formatTextRect();  // in "syntax.cpp"
    if (!textEdit->getSearchedText().isEmpty())
        hlight();  // in "find.cpp"
    textEdit->selectionHlight();
}
/*************************/
void FPwin::zoomIn() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->zooming(1.f);
}
/*************************/
void FPwin::zoomOut() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        textEdit->zooming(-1.f);
    }
}
/*************************/
void FPwin::zoomZero() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        textEdit->zooming(0.f);
    }
}
/*************************/
void FPwin::defaultSize() {
    QSize s = static_cast<FPsingleton*>(qApp)->getConfig().getStartSize();
    if (size() == s)
        return;
    if (isMaximized() || isFullScreen())
        showNormal();
    /*if (isMaximized() && isFullScreen())
        showMaximized();
    if (isMaximized())
        showNormal();*/
    /* instead of hiding, reparent with the dummy
       widget to guarantee resizing under all DEs */
    /*Qt::WindowFlags flags = windowFlags();
    setParent (dummyWidget, Qt::SubWindow);*/
    // hide();
    resize(s);
    /*if (parent() != nullptr)
        setParent (nullptr, flags);*/
    // QTimer::singleShot (0, this, &FPwin::show);
}
/*************************/
/*void FPwin::align()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    TextEdit *textEdit = qobject_cast< TabPage *>(ui->tabWidget->widget (index))->textEdit();
    QTextOption opt = textEdit->document()->defaultTextOption();
    if (opt.alignment() == (Qt::AlignLeft))
    {
        opt = QTextOption (Qt::AlignRight);
        opt.setTextDirection (Qt::LayoutDirectionAuto);
        textEdit->document()->setDefaultTextOption (opt);
    }
    else if (opt.alignment() == (Qt::AlignRight))
    {
        opt = QTextOption (Qt::AlignLeft);
        opt.setTextDirection (Qt::LayoutDirectionAuto);
        textEdit->document()->setDefaultTextOption (opt);
    }
}*/
/*************************/
void FPwin::focusView() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->setFocus();
}
/*************************/
void FPwin::focusSidePane() {
    if (sidePane_) {
        QList<int> sizes = ui->splitter->sizes();
        if (sizes.size() == 2 && sizes.at(0) == 0)  // with RTL too
        {                                           // first, ensure its visibility (see toggleSidePane())
            sizes.clear();
            Config config = static_cast<FPsingleton*>(qApp)->getConfig();
            if (config.getRemSplitterPos()) {
                sizes.append(std::min(std::max(16, config.getSplitterPos()), size().width() / 2));
                sizes.append(100);
            }
            else {
                int s = std::min(size().width() / 5, 40 * sidePane_->fontMetrics().horizontalAdvance(' '));
                sizes << s << size().width() - s;
            }
            ui->splitter->setSizes(sizes);
        }
        sidePane_->listWidget()->setFocus();
    }
}
/*************************/
void FPwin::executeProcess() {
    QList<QDialog*> dialogs = findChildren<QDialog*>();
    for (int i = 0; i < dialogs.count(); ++i) {
        if (dialogs.at(i)->isModal())
            return;  // shortcut may work when there's a modal dialog
    }
    closeWarningBar();

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (!config.getExecuteScripts())
        return;

    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        if (tabPage->findChild<QProcess*>(QString(), Qt::FindDirectChildrenOnly)) {
            showWarningBar("<center><b><big>" + tr("Another process is running in this tab!") + "</big></b></center>" +
                               "<center><i>" + tr("Only one process is allowed per tab.") + "</i></center>",
                           15);
            return;
        }

        QString fName = tabPage->textEdit()->getFileName();
        if (!isScriptLang(tabPage->textEdit()->getProg()) || !QFileInfo(fName).isExecutable()) {
            ui->actionRun->setVisible(false);
            return;
        }

        QProcess* process = new QProcess(tabPage);
        process->setObjectName(fName);  // to put it into the message dialog
        connect(process, &QProcess::readyReadStandardOutput, this, &FPwin::displayOutput);
        connect(process, &QProcess::readyReadStandardError, this, &FPwin::displayError);
        QString command = config.getExecuteCommand();
        if (!command.isEmpty()) {
            QStringList commandParts = QProcess::splitCommand(command);
            if (!commandParts.isEmpty()) {
                command = commandParts.takeAt(0);  // there may be arguments
                process->start(command, QStringList() << commandParts << fName);
            }
            else
                process->start(fName, QStringList());
        }
        else
            process->start(fName, QStringList());
        /* old-fashioned: connect(process, static_cast<void(QProcess::*)(int,
         * QProcess::ExitStatus)>(&QProcess::finished),... */
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                [=](int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/) { process->deleteLater(); });
    }
}
/*************************/
bool FPwin::isScriptLang(const QString& lang) const {
    return (lang == "sh" || lang == "python" || lang == "ruby" || lang == "lua" || lang == "perl");
}
/*************************/
void FPwin::exitProcess() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        if (QProcess* process = tabPage->findChild<QProcess*>(QString(), Qt::FindDirectChildrenOnly))
            process->kill();
    }
}
/*************************/
void FPwin::displayMessage(bool error) {
    QProcess* process = static_cast<QProcess*>(QObject::sender());
    if (!process)
        return;  // impossible
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
    QList<QDialog*> dialogs = findChildren<QDialog*>();
    for (int i = 0; i < dialogs.count(); ++i) {
        if (dialogs.at(i)->parent() == process->parent()) {
            msgDlg = dialogs.at(i);
            break;
        }
    }
    if (msgDlg) {  // append to the existing message
        if (QPlainTextEdit* tEdit = msgDlg->findChild<QPlainTextEdit*>()) {
            tEdit->setPlainText(tEdit->toPlainText() + "\n" + msg.constData());
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
        QGridLayout* grid = new QGridLayout;
        QLabel* label = new QLabel(msgDlg);
        label->setText("<center><b>" + tr("Script File") + ": </b></center><i>" + process->objectName() + "</i>");
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setWordWrap(true);
        label->setMargin(5);
        grid->addWidget(label, 0, 0, 1, 2);
        QPlainTextEdit* tEdit = new QPlainTextEdit(msgDlg);
        tEdit->setTextInteractionFlags(Qt::TextSelectableByMouse);
        tEdit->ensureCursorVisible();
        grid->addWidget(tEdit, 1, 0, 1, 2);
        QPushButton* closeButton = new QPushButton(QIcon::fromTheme("edit-delete"), tr("Close"));
        connect(closeButton, &QAbstractButton::clicked, msgDlg, &QDialog::reject);
        grid->addWidget(closeButton, 2, 1, Qt::AlignRight);
        QPushButton* clearButton = new QPushButton(QIcon::fromTheme("edit-clear"), tr("Clear"));
        connect(clearButton, &QAbstractButton::clicked, tEdit, &QPlainTextEdit::clear);
        grid->addWidget(clearButton, 2, 0, Qt::AlignLeft);
        msgDlg->setLayout(grid);
        tEdit->setPlainText(msg.constData());
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
void FPwin::displayOutput() {
    displayMessage(false);
}
/*************************/
void FPwin::displayError() {
    displayMessage(true);
}
/*************************/
// This closes either the current page or the right-clicked side-pane item but
// never the right-clicked tab because the tab context menu has no closing item.
void FPwin::closePage() {
    if (!isReady())
        return;

    pauseAutoSaving(true);

    QListWidgetItem* curItem = nullptr;
    int tabIndex = -1;
    int index = -1;                       // tab index or side-pane row
    if (sidePane_ && rightClicked_ >= 0)  // close the right-clicked item
    {
        index = rightClicked_;
        tabIndex = ui->tabWidget->indexOf(sideItems_.value(sidePane_->listWidget()->item(rightClicked_)));
        if (tabIndex != ui->tabWidget->currentIndex())
            curItem = sidePane_->listWidget()->currentItem();
    }
    else  // close the current page
    {
        tabIndex = ui->tabWidget->currentIndex();
        if (tabIndex == -1)  // not needed
        {
            pauseAutoSaving(false);
            return;
        }
        index = tabIndex;  // may need to be converted to the side-pane row
        if (sidePane_ && !sideItems_.isEmpty()) {
            if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(tabIndex))) {
                if (QListWidgetItem* wi = sideItems_.key(tabPage))
                    index = sidePane_->listWidget()->row(wi);
            }
        }
    }

    if (savePrompt(tabIndex, false, index - 1, index + 1, false, curItem) != SAVED) {
        pauseAutoSaving(false);
        return;
    }

    deleteTabPage(tabIndex);
    int count = ui->tabWidget->count();
    if (count == 0) {
        ui->actionReload->setDisabled(true);
        ui->actionSave->setDisabled(true);
        enableWidgets(false);
    }
    else  // set focus to text-edit
    {
        if (count == 1)
            updateGUIForSingleTab(true);

        if (curItem)  // restore the current item
            sidePane_->listWidget()->setCurrentItem(curItem);

        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
            tabPage->textEdit()->setFocus();
    }

    pauseAutoSaving(false);
}
/*************************/
void FPwin::closeTabAtIndex(int tabIndex) {
    pauseAutoSaving(true);

    TabPage* curPage = nullptr;
    QListWidgetItem* curItem = nullptr;
    if (tabIndex != ui->tabWidget->currentIndex()) {
        if (sidePane_)
            curItem = sidePane_->listWidget()->currentItem();
        else
            curPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    }
    int index = tabIndex;  // may need to be converted to the side-pane row
    if (sidePane_ && !sideItems_.isEmpty()) {
        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(tabIndex))) {
            if (QListWidgetItem* wi = sideItems_.key(tabPage))
                index = sidePane_->listWidget()->row(wi);
        }
    }
    if (savePrompt(tabIndex, false, index - 1, index + 1, false, curItem, curPage) != SAVED) {
        pauseAutoSaving(false);
        return;
    }
    closeWarningBar();

    deleteTabPage(tabIndex);
    int count = ui->tabWidget->count();
    if (count == 0) {
        ui->actionReload->setDisabled(true);
        ui->actionSave->setDisabled(true);
        enableWidgets(false);
    }
    else {
        if (count == 1)
            updateGUIForSingleTab(true);

        /* restore the current page/item */
        if (curPage)
            ui->tabWidget->setCurrentWidget(curPage);
        else if (curItem)
            sidePane_->listWidget()->setCurrentItem(curItem);

        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
            tabPage->textEdit()->setFocus();
    }

    pauseAutoSaving(false);
}
/*************************/
void FPwin::setWinTitle(const QString& title) {
    setWindowTitle(title);
    if (!ui->menuBar->isHidden()) {
        if (auto mbt = qobject_cast<MenuBarTitle*>(ui->menuBar->cornerWidget()))
            mbt->setTitle(title);
    }
}
/*************************/
void FPwin::setTitle(const QString& fileName, int tabIndex) {
    int index = tabIndex;
    if (index < 0)
        index = ui->tabWidget->currentIndex();  // is never -1

    bool isLink(false);
    bool hasFinalTarget(false);
    QString shownName;
    if (fileName.isEmpty()) {
        shownName = tr("Untitled");
        if (tabIndex < 0)
            setWinTitle(shownName);
    }
    else {
        QFileInfo fInfo(fileName);
        if (tabIndex < 0)
            setWinTitle(fileName.contains("/") ? fileName : fInfo.absolutePath() + "/" + fileName);
        isLink = fInfo.isSymLink();
        if (!isLink) {
            const QString finalTarget = fInfo.canonicalFilePath();
            hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != fileName);
        }
        shownName = fileName.section('/', -1);
        shownName.replace("\n", " ");  // no multi-line tab text
    }

    if (sidePane_ && !sideItems_.isEmpty()) {
        if (QListWidgetItem* wi = sideItems_.key(qobject_cast<TabPage*>(ui->tabWidget->widget(index)))) {
            wi->setText(shownName);
            if (isLink)
                wi->setIcon(QIcon(":icons/link.svg"));
            else if (hasFinalTarget)
                wi->setIcon(QIcon(":icons/hasTarget.svg"));
            else
                wi->setIcon(QIcon());
        }
    }

    shownName.replace("&", "&&");  // single ampersand is for tab mnemonic
    shownName.replace('\t', ' ');
    ui->tabWidget->setTabText(index, shownName);
    if (isLink)
        ui->tabWidget->setTabIcon(index, QIcon(":icons/link.svg"));
    else if (hasFinalTarget)
        ui->tabWidget->setTabIcon(index, QIcon(":icons/hasTarget.svg"));
    else
        ui->tabWidget->setTabIcon(index, QIcon());
}
/*************************/
void FPwin::enableSaving(bool modified) {
    if (!inactiveTabModified_)
        ui->actionSave->setEnabled(modified);
}
/*************************/
void FPwin::asterisk(bool modified) {
    if (inactiveTabModified_)
        return;

    int index = ui->tabWidget->currentIndex();
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index));
    if (tabPage == nullptr)
        return;
    QString fname = tabPage->textEdit()->getFileName();
    QString shownName;
    if (fname.isEmpty()) {
        shownName = tr("Untitled");
        setWinTitle((modified ? "*" : QString()) + shownName);
    }
    else {
        shownName = fname.section('/', -1);
        setWinTitle((modified ? "*" : QString()) +
                    (fname.contains("/") ? fname : QFileInfo(fname).absolutePath() + "/" + fname));
    }
    shownName.replace("\n", " ");

    if (sidePane_ && !sideItems_.isEmpty()) {
        if (QListWidgetItem* wi = sideItems_.key(tabPage))
            wi->setText(modified ? shownName + "*" : shownName);
    }

    if (modified)
        shownName.prepend("*");
    shownName.replace("&", "&&");
    shownName.replace('\t', ' ');
    ui->tabWidget->setTabText(index, shownName);
}
/*************************/
void FPwin::makeBusy() {
    if (QGuiApplication::overrideCursor() == nullptr)
        QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
}
/*************************/
void FPwin::unbusy() {
    if (QGuiApplication::overrideCursor() != nullptr)
        QGuiApplication::restoreOverrideCursor();
}
/*************************/
void FPwin::loadText(const QString& fileName,
                     bool enforceEncod,
                     bool reload,
                     int restoreCursor,
                     int posInLine,
                     bool enforceUneditable,
                     bool multiple) {
    ++loadingProcesses_;
    QString charset;
    if (enforceEncod)
        charset = checkToEncoding();
    Loading* thread = new Loading(fileName, charset, reload, restoreCursor, posInLine, enforceUneditable, multiple);
    thread->setSkipNonText(static_cast<FPsingleton*>(qApp)->getConfig().getSkipNonText());
    connect(thread, &Loading::completed, this, &FPwin::addText);
    connect(thread, &Loading::finished, thread, &QObject::deleteLater);
    thread->start();

    makeBusy();
    ui->tabWidget->tabBar()->lockTabs(true);
    updateShortcuts(true, false);
}
/*************************/
// When multiple files are being loaded, we don't change the current tab.
void FPwin::addText(const QString& text,
                    const QString& fileName,
                    const QString& charset,
                    bool enforceEncod,
                    bool reload,
                    int restoreCursor,
                    int posInLine,
                    bool uneditable,
                    bool multiple) {
    // early error and special-case routing
    if (fileName.isEmpty() || charset.isEmpty()) {
        if (!fileName.isEmpty() && charset.isEmpty())  // very large file
            connect(this, &FPwin::finishedLoading, this, &FPwin::onOpeningHugeFiles, Qt::UniqueConnection);
        else if (fileName.isEmpty() && !charset.isEmpty())  // non-text file that shouldn't be opened
            connect(this, &FPwin::finishedLoading, this, &FPwin::onOpeninNonTextFiles, Qt::UniqueConnection);
        else
            connect(this, &FPwin::finishedLoading, this, &FPwin::onPermissionDenied, Qt::UniqueConnection);

        --loadingProcesses_;  // can never become negative
        if (!isLoading()) {
            ui->tabWidget->tabBar()->lockTabs(false);
            updateShortcuts(false, false);
            closeWarningBar();
            emit finishedLoading();
            QTimer::singleShot(0, this, &FPwin::unbusy);
            stealFocus();
        }
        return;
    }

    if (enforceEncod || reload)
        multiple = false;  // respect the logic

    // only for the side-pane mode
    static bool scrollToFirstItem = false;
    static QListWidgetItem* firstItem = nullptr;

    TabPage* tabPage = nullptr;
    if (ui->tabWidget->currentIndex() == -1)
        tabPage = createEmptyTab(!multiple, false);
    else
        tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();

    bool openInCurrentTab = true;
    if (!reload && !enforceEncod &&
        (!textEdit->document()->isEmpty() || textEdit->document()->isModified() ||
         !textEdit->getFileName().isEmpty())) {
        tabPage = createEmptyTab(!multiple, false);
        textEdit = tabPage->textEdit();
        openInCurrentTab = false;
    }
    else {
        if (sidePane_ && !reload && !enforceEncod)  // an unused empty tab
            scrollToFirstItem = true;
    }

    textEdit->setSaveCursor(restoreCursor == 1);
    textEdit->setLang(QString());  // remove the enforced syntax

    // capture view position before changing highlighter on reload
    TextEdit::viewPosition vPos;
    if (reload) {
        textEdit->forgetTxtCurHPos();
        vPos = textEdit->getViewPosition();
    }

    // temporarily remove highlighter to avoid redundant work during setPlainText
    if (textEdit->getHighlighter()) {
        textEdit->setGreenSel(QList<QTextEdit::ExtraSelection>());  // previous finds will be meaningless after load
        syntaxHighlighting(textEdit, false);
    }

    const QFileInfo fInfo(fileName);
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();

    // set the text
    inactiveTabModified_ = true;   // ignore modificationChanged during initial set
    textEdit->setPlainText(text);  // resets undo/redo
    inactiveTabModified_ = false;

    // restore cursor position if requested
    if (!reload && restoreCursor != 0) {
        if (restoreCursor == 1 || restoreCursor == -1) {  // restore cursor from settings
            const QHash<QString, QVariant> cursorPos =
                (restoreCursor == 1) ? config.savedCursorPos() : config.getLastFilesCursorPos();

            auto it = cursorPos.constFind(fileName);
            if (it != cursorPos.constEnd()) {
                QTextCursor cur = textEdit->textCursor();
                cur.movePosition(QTextCursor::End);
                const int pos = std::clamp(it.value().toInt(), 0, cur.position());
                cur.setPosition(pos);
                QTimer::singleShot(0, textEdit, [textEdit, cur] { textEdit->setTextCursor(cur); });
            }
        }
        else if (restoreCursor < -1) {  // doc end in commandline
            QTextCursor cur = textEdit->textCursor();
            cur.movePosition(QTextCursor::End);
            QTimer::singleShot(0, textEdit, [textEdit, cur] { textEdit->setTextCursor(cur); });
        }
        else {                              // restoreCursor >= 2 means 1-based line number
            int line0 = restoreCursor - 2;  // Qt blocks start at 0
            if (line0 < textEdit->document()->blockCount()) {
                const QTextBlock block = textEdit->document()->findBlockByNumber(line0);
                QTextCursor cur(block);
                QTextCursor tmp = cur;
                tmp.movePosition(QTextCursor::EndOfBlock);
                if (posInLine < 0 || posInLine >= tmp.positionInBlock())
                    cur = tmp;
                else
                    cur.setPosition(block.position() + posInLine);
                QTimer::singleShot(0, textEdit, [textEdit, cur] { textEdit->setTextCursor(cur); });
            }
            else {
                QTextCursor cur = textEdit->textCursor();
                cur.movePosition(QTextCursor::End);
                QTimer::singleShot(0, textEdit, [textEdit, cur] { textEdit->setTextCursor(cur); });
            }
        }
    }

    // file metadata and bookkeeping
    textEdit->setFileName(fileName);
    textEdit->setSize(fInfo.size());
    textEdit->setLastModified(fInfo.lastModified());
    lastFile_ = fileName;
    if (config.getRecentOpened())
        addRecentFile(lastFile_);
    textEdit->setEncoding(charset);
    textEdit->setWordNumber(-1);

    if (uneditable) {
        textEdit->makeUneditable(true);
        if (!reload)  // on reload this will be connected later
            connect(this, &FPwin::finishedLoading, this, &FPwin::onOpeningUneditable, Qt::UniqueConnection);
    }

    setProgLang(textEdit);
    if (ui->actionSyntax->isChecked())
        syntaxHighlighting(textEdit);

    setTitle(fileName, (multiple && !openInCurrentTab) ? ui->tabWidget->indexOf(tabPage) : -1);

    // build tooltip for tab and side pane
    QString tip =
        fileName.contains(QLatin1Char('/')) ? fileName.section(QLatin1Char('/'), 0, -2) : fInfo.absolutePath();
    if (!tip.endsWith(QLatin1Char('/')))
        tip += QLatin1Char('/');

    const QFontMetrics metrics(QToolTip::font());
    const QString elidedTip =
        QStringLiteral("<p style='white-space:pre'>%1</p>")
            .arg(metrics.elidedText(tip, Qt::ElideMiddle, 200 * metrics.horizontalAdvance(QLatin1Char(' '))));
    const int tabIndex = ui->tabWidget->indexOf(tabPage);
    ui->tabWidget->setTabToolTip(tabIndex, elidedTip);

    if (!sideItems_.isEmpty()) {
        if (QListWidgetItem* wi = sideItems_.key(tabPage)) {
            wi->setToolTip(elidedTip);
            if (scrollToFirstItem &&
                (!firstItem || *(static_cast<ListWidgetItem*>(wi)) < *(static_cast<ListWidgetItem*>(firstItem)))) {
                firstItem = wi;
            }
        }
    }

    // adjust readonly and UI state if uneditable or opened in another tab
    if (uneditable || alreadyOpen(tabPage)) {
        textEdit->setReadOnly(true);

        // lightweight palette tweak via stylesheet for readonly view
        if (!textEdit->hasDarkScheme()) {
            if (uneditable)
                textEdit->viewport()->setStyleSheet(
                    ".QWidget {"
                    "color: black;"
                    "background-color: rgb(225, 238, 255);}");
            else
                textEdit->viewport()->setStyleSheet(
                    ".QWidget {"
                    "color: black;"
                    "background-color: rgb(236, 236, 208);}");
        }
        else {
            if (uneditable)
                textEdit->viewport()->setStyleSheet(
                    ".QWidget {"
                    "color: white;"
                    "background-color: rgb(0, 60, 110);}");
            else
                textEdit->viewport()->setStyleSheet(
                    ".QWidget {"
                    "color: white;"
                    "background-color: rgb(60, 0, 0);}");
        }

        if (!multiple || openInCurrentTab) {
            if (!uneditable)
                ui->actionEdit->setVisible(true);
            else {
                ui->actionSaveAs->setDisabled(true);
                ui->actionSaveCodec->setDisabled(true);
            }
            ui->actionCut->setDisabled(true);
            ui->actionPaste->setDisabled(true);
            ui->actionSoftTab->setDisabled(true);
            ui->actionDate->setDisabled(true);
            ui->actionDelete->setDisabled(true);
            ui->actionUpperCase->setDisabled(true);
            ui->actionLowerCase->setDisabled(true);
            ui->actionStartCase->setDisabled(true);
            if (config.getSaveUnmodified())
                ui->actionSave->setDisabled(true);
        }

        // disconnect actions that depend on copy availability
        disconnect(textEdit, &TextEdit::canCopy, ui->actionCut, &QAction::setEnabled);
        disconnect(textEdit, &TextEdit::canCopy, ui->actionDelete, &QAction::setEnabled);
        disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionUpperCase, &QAction::setEnabled);
        disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionLowerCase, &QAction::setEnabled);
        disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionStartCase, &QAction::setEnabled);
    }
    else if (textEdit->isReadOnly()) {
        QTimer::singleShot(0, this, &FPwin::makeEditable);
    }

    // UI updates for the active tab when not opening multiple files at once
    if (!multiple || openInCurrentTab) {
        if (!fInfo.exists())
            connect(this, &FPwin::finishedLoading, this, &FPwin::onOpeningNonexistent, Qt::UniqueConnection);

        if (ui->statusBar->isVisible()) {
            statusMsgWithLineCount(textEdit->document()->blockCount());
            if (QToolButton* wordButton = ui->statusBar->findChild<QToolButton*>(QStringLiteral("wordButton")))
                wordButton->setVisible(true);
            if (text.isEmpty())
                updateWordInfo();
        }

        if (config.getShowLangSelector() && config.getSyntaxByDefault())
            updateLangBtn(textEdit);

        encodingToCheck(charset);
        ui->actionReload->setEnabled(true);
        textEdit->setFocus();

        if (openInCurrentTab) {
            if (isScriptLang(textEdit->getProg()) && fInfo.isExecutable())
                ui->actionRun->setVisible(config.getExecuteScripts());
            else
                ui->actionRun->setVisible(false);
        }
    }

    // a file is completely loaded
    --loadingProcesses_;
    if (!isLoading()) {
        ui->tabWidget->tabBar()->lockTabs(false);
        updateShortcuts(false, false);

        if (reload) {
            // restore cursor and scroll position after reload
            lambdaConnection_ = QObject::connect(this, &FPwin::finishedLoading, textEdit, [this, textEdit, vPos] {
                QTimer::singleShot(0, textEdit, [textEdit, vPos] { textEdit->setViewPostion(vPos); });
                disconnectLambda();
            });
            if (uneditable)
                connect(this, &FPwin::finishedLoading, this, &FPwin::onOpeningUneditable, Qt::UniqueConnection);
        }
        else if (firstItem) {
            // select the first item when sidePane exists
            sidePane_->listWidget()->setCurrentItem(firstItem);
        }

        // reset static side-pane helpers
        scrollToFirstItem = false;
        firstItem = nullptr;

        closeWarningBar(true);  // allow closing animation to finish
        emit finishedLoading();
        QTimer::singleShot(0, this, &FPwin::unbusy);  // remove busy cursor after pending events like highlighting
        stealFocus();
    }
}
/*************************/
void FPwin::disconnectLambda() {
    QObject::disconnect(lambdaConnection_);
}
/*************************/
void FPwin::onOpeningHugeFiles() {
    disconnect(this, &FPwin::finishedLoading, this, &FPwin::onOpeningHugeFiles);
    QTimer::singleShot(0, this, [=]() {
        showWarningBar("<center><b><big>" + tr("Huge file(s) not opened!") + "</big></b></center>\n" + "<center>" +
                       tr("FeatherPad does not open files larger than 100 MiB.") + "</center>");
    });
}
/*************************/
void FPwin::onOpeninNonTextFiles() {
    disconnect(this, &FPwin::finishedLoading, this, &FPwin::onOpeninNonTextFiles);
    QTimer::singleShot(0, this, [=]() {
        showWarningBar("<center><b><big>" + tr("Non-text file(s) not opened!") + "</big></b></center>\n" +
                           "<center><i>" + tr("See Preferences → Files → Do not permit opening of non-text files") +
                           "</i></center>",
                       20);
    });
}
/*************************/
void FPwin::onPermissionDenied() {
    disconnect(this, &FPwin::finishedLoading, this, &FPwin::onPermissionDenied);
    QTimer::singleShot(0, this, [=]() {
        showWarningBar("<center><b><big>" + tr("Some file(s) could not be opened!") + "</big></b></center>\n" +
                       "<center>" + tr("You may not have the permission to read.") + "</center>");
    });
}
/*************************/
void FPwin::onOpeningUneditable() {
    disconnect(this, &FPwin::finishedLoading, this, &FPwin::onOpeningUneditable);
    /* A timer is needed here because the scrollbar position is restored on reloading by a
       lambda connection. Timers are also used in similar places for the sake of certainty. */
    QTimer::singleShot(0, this, [=]() {
        showWarningBar("<center><b><big>" + tr("Uneditable file(s)!") + "</big></b></center>\n" + "<center>" +
                       tr("Non-text files or files with huge lines cannot be edited.") + "</center>");
    });
}
/*************************/
void FPwin::onOpeningNonexistent() {
    disconnect(this, &FPwin::finishedLoading, this, &FPwin::onOpeningNonexistent);
    QTimer::singleShot(0, this, [=]() {
        /* show the bar only if the current file doesn't exist at this very moment */
        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
            QString fname = tabPage->textEdit()->getFileName();
            if (!fname.isEmpty() && !QFile::exists(fname))
                showWarningBar("<center><b><big>" + tr("The file does not exist.") + "</big></b></center>");
        }
    });
}
/*************************/
void FPwin::columnWarning() {
    showWarningBar("<center><b><big>" + tr("Huge column!") + "</big></b></center>\n" + "<center>" +
                   tr("Columns with more than 1000 rows are not supported.") + "</center>");
}
/*************************/
void FPwin::showWarningBar(const QString& message, int timeout, bool startupBar) {
    /* don't show this warning bar if the window is locked at this moment */
    if (locked_)
        return;
    if (timeout > 0) {
        /* don't show the temporary warning bar when there's a modal dialog */
        QList<QDialog*> dialogs = findChildren<QDialog*>();
        for (int i = 0; i < dialogs.count(); ++i) {
            if (dialogs.at(i)->isModal())
                return;
        }
    }

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());

    /* don't close and show the same warning bar */
    if (WarningBar* prevBar = ui->tabWidget->findChild<WarningBar*>()) {
        if (!prevBar->isClosing() && prevBar->getMessage() == message) {
            prevBar->setTimeout(timeout);
            if (tabPage && timeout > 0) {  // close the bar when the text is scrolled
                disconnect(tabPage->textEdit(), &QPlainTextEdit::updateRequest, prevBar,
                           &WarningBar::closeBarOnScrolling);
                connect(tabPage->textEdit(), &QPlainTextEdit::updateRequest, prevBar, &WarningBar::closeBarOnScrolling);
            }
            return;
        }
    }

    int vOffset = 0;
    if (tabPage)
        vOffset = tabPage->height() - tabPage->textEdit()->height();
    WarningBar* bar = new WarningBar(message, vOffset, timeout, ui->tabWidget);
    if (startupBar)
        bar->setObjectName("startupBar");
    /* close the bar when the text is scrolled */
    if (tabPage && timeout > 0)
        connect(tabPage->textEdit(), &QPlainTextEdit::updateRequest, bar, &WarningBar::closeBarOnScrolling);
}
/*************************/
void FPwin::showRootWarning() {
    QTimer::singleShot(
        0, this, [=]() { showWarningBar("<center><b><big>" + tr("Root Instance") + "</big></b></center>", 10, true); });
}
/*************************/
void FPwin::closeWarningBar(bool keepOnStartup) {
    const QList<WarningBar*> warningBars = ui->tabWidget->findChildren<WarningBar*>();
    for (WarningBar* wb : warningBars) {
        if (!keepOnStartup || wb->objectName() != "startupBar")
            wb->closeBar();
    }
}
/*************************/
void FPwin::newTabFromName(const QString& fileName, int restoreCursor, int posInLine, bool multiple) {
    if (!fileName.isEmpty())
        loadText(fileName, false, false, restoreCursor, posInLine, false, multiple);
}
/*************************/
void FPwin::newTabFromRecent() {
    QAction* action = qobject_cast<QAction*>(QObject::sender());
    if (!action)
        return;
    loadText(action->data().toString(), false, false);
}
/*************************/
void FPwin::fileOpen() {
    if (isLoading())
        return;

    /* find a suitable directory */
    QString fname;
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        fname = tabPage->textEdit()->getFileName();

    QString path;
    if (!fname.isEmpty()) {
        if (QFile::exists(fname))
            path = fname;
        else {
            QDir dir = QFileInfo(fname).absoluteDir();
            if (!dir.exists())
                dir = QDir::home();
            path = dir.path();
        }
    }
    else {
        /* I like the last opened file to be remembered */
        fname = lastFile_;
        if (!fname.isEmpty()) {
            QDir dir = QFileInfo(fname).absoluteDir();
            if (!dir.exists())
                dir = QDir::home();
            path = dir.path();
        }
        else {
            QDir dir = QDir::home();
            path = dir.path();
        }
    }

    if (hasAnotherDialog())
        return;
    updateShortcuts(true);
    QString filter = tr("All Files") + " (*)";
    if (!fname.isEmpty() && QFileInfo(fname).fileName().contains('.')) {
        /* if relevant, do filtering to make opening of similar files easier */
        filter = tr("All Files") + QString(" (*);;*.%1").arg(fname.section('.', -1, -1));
    }
    FileDialog dialog(this, static_cast<FPsingleton*>(qApp)->getConfig().getNativeDialog());
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setWindowTitle(tr("Open file..."));
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter(filter);
    /*dialog.setLabelText (QFileDialog::Accept, tr ("Open"));
    dialog.setLabelText (QFileDialog::Reject, tr ("Cancel"));*/
    if (QFileInfo(path).isDir())
        dialog.setDirectory(path);
    else {
        dialog.setDirectory(path.section("/", 0, -2));  // it's a shame the KDE's file dialog is buggy and needs this
        dialog.selectFile(path);
        dialog.autoScroll();
    }
    if (dialog.exec()) {
        const QStringList files = dialog.selectedFiles();
        bool multiple(files.count() > 1 || isLoading());
        for (const QString& file : files)
            newTabFromName(file, 0, 0, multiple);
    }
    updateShortcuts(false);
}
/*************************/
// Check if the file is already opened for editing somewhere else.
bool FPwin::alreadyOpen(TabPage* tabPage) const {
    bool res = false;

    QString fileName = tabPage->textEdit()->getFileName();
    QFileInfo info(fileName);
    bool exists = info.exists();
    QString target = info.isSymLink() ? info.symLinkTarget()  // consider symlinks too
                                      : fileName;
    FPsingleton* singleton = static_cast<FPsingleton*>(qApp);
    for (int i = 0; i < singleton->Wins.count(); ++i) {
        FPwin* thisOne = singleton->Wins.at(i);
        for (int j = 0; j < thisOne->ui->tabWidget->count(); ++j) {
            TabPage* thisTabPage = qobject_cast<TabPage*>(thisOne->ui->tabWidget->widget(j));
            if (thisOne == this && thisTabPage == tabPage)
                continue;
            TextEdit* thisTextEdit = thisTabPage->textEdit();
            if (thisTextEdit->isReadOnly())
                continue;
            QFileInfo thisInfo(thisTextEdit->getFileName());
            QString thisTarget = thisInfo.isSymLink() ? thisInfo.symLinkTarget() : thisTextEdit->getFileName();
            if (thisTarget == target || (exists && thisInfo.exists() && info == thisInfo)) {
                res = true;
                break;
            }
        }
        if (res)
            break;
    }
    return res;
}
/*************************/
void FPwin::enforceEncoding(QAction* a) {
    /* not needed because encoding has no keyboard shortcut or tool button */
    if (isLoading())
        return;

    int index = ui->tabWidget->currentIndex();
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index));
    if (tabPage == nullptr)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    QString fname = textEdit->getFileName();
    if (!fname.isEmpty()) {
        if (savePrompt(index, false) != SAVED) {  // back to the previous encoding
            if (!locked_)
                encodingToCheck(textEdit->getEncoding());
            return;
        }
        /* if the file is removed, close its tab to open a new one */
        if (!QFile::exists(fname))
            deleteTabPage(index, false, false);

        a->setChecked(true);  // the checked action might have been changed (to UTF-8) with saving
        loadText(fname, true, true, 0, 0, textEdit->isUneditable(), false);
    }
    else {
        /* just change the statusbar text; the doc
           might be saved later with the new encoding */
        textEdit->setEncoding(checkToEncoding());
        if (ui->statusBar->isVisible()) {
            QLabel* statusLabel = ui->statusBar->findChild<QLabel*>("statusLabel");
            QString str = statusLabel->text();
            QString encodStr = tr("Encoding");
            // the next info is about lines; there's no syntax info
            QString lineStr = "</i>&nbsp;&nbsp;&nbsp;<b>" + tr("Lines");
            int i = str.indexOf(encodStr);
            int j = str.indexOf(lineStr);
            int offset = encodStr.size() + 9;  // size of ":</b> <i>"
            str.replace(i + offset, j - i - offset, checkToEncoding());
            statusLabel->setText(str);
        }
    }
}
/*************************/
void FPwin::reload() {
    if (isLoading())
        return;

    int index = ui->tabWidget->currentIndex();
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index));
    if (tabPage == nullptr)
        return;

    if (savePrompt(index, false) != SAVED)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    QString fname = textEdit->getFileName();
    /* if the file is removed, close its tab to open a new one */
    if (!QFile::exists(fname))
        deleteTabPage(index, false, false);
    if (!fname.isEmpty()) {
        loadText(fname, false, true, textEdit->getSaveCursor() ? 1 : 0);
    }
}
/*************************/
static inline int trailingSpaces(const QString& str) {
    int i = 0;
    while (i < str.length()) {
        if (!str.at(str.length() - 1 - i).isSpace())
            return i;
        ++i;
    }
    return i;
}

void FPwin::removeTrailingSpacesIfNeeded(TextEdit* textEdit) {
    QString lang = textEdit->getFileName().isEmpty() ? textEdit->getLang() : textEdit->getProg();

    // Skip "diff" or "locale.gen" per original logic
    if (lang == "diff" || textEdit->getFileName().endsWith("/locale.gen")) {
        return;
    }

    makeBusy();
    // same block iteration as before
    QTextBlock block = textEdit->document()->firstBlock();
    QTextCursor tmpCur = textEdit->textCursor();
    tmpCur.beginEditBlock();

    // Check 'doubleSpace' for markdown, 'singleSpace' for LaTeX, etc.
    bool doubleSpace = (lang == "markdown" || lang == "fountain");
    bool singleSpace = (lang == "LaTeX");

    while (block.isValid()) {
        int num = trailingSpaces(block.text());
        if (num > 0) {
            tmpCur.setPosition(block.position() + block.text().length());
            if (doubleSpace) {
                if (num != 2)
                    tmpCur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, std::max(1, num - 2));
            }
            else if (singleSpace) {
                if (num > 1)
                    tmpCur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, num - 1);
            }
            else {
                tmpCur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, num);
            }
            tmpCur.removeSelectedText();
        }
        block = block.next();
    }
    tmpCur.endEditBlock();
    unbusy();
}

static inline QStringEncoder getEncoder(const QString& encoding) {
    if (encoding.compare("UTF-16", Qt::CaseInsensitive) == 0) {
        return QStringEncoder(QStringConverter::Utf16,
                              QStringConverter::Flag::WriteBom);  // needed with fwrite()
    }
    return QStringEncoder(encoding.compare("UTF-8", Qt::CaseInsensitive) == 0    ? QStringConverter::Utf8
                          : encoding.compare("UTF-32", Qt::CaseInsensitive) == 0 ?  // not needed
                              QStringConverter::Utf32
                                                                                 : QStringConverter::Latin1);
}
/*************************/
bool FPwin::showSaveDialogAndSetFileName(QString& fname, const QString& filter, const QString& title) {
    if (hasAnotherDialog())
        return false;
    updateShortcuts(true);

    FileDialog dialog(this, static_cast<FPsingleton*>(qApp)->getConfig().getNativeDialog());
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setWindowTitle(title);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilter(filter);

    // workaround for some KDE file dialogs
    const QString dirPath = fname.section(QLatin1Char('/'), 0, -2);
    if (!dirPath.isEmpty())
        dialog.setDirectory(dirPath);
    dialog.selectFile(fname);
    dialog.autoScroll();

    const bool ok = dialog.exec();
    updateShortcuts(false);
    if (!ok)
        return false;

    const QString chosen = dialog.selectedFiles().value(0);
    if (chosen.isEmpty() || QFileInfo(chosen).isDir())
        return false;

    fname = chosen;
    return true;
}

/*************************/
bool FPwin::writeFileWithEncoding(const QString& fname, TextEdit* textEdit, bool& MSWinLineEnd) {
    const QString encoding = checkToEncoding();  // e.g. UTF-16 or ISO-8859-1
    if (encoding == QLatin1String("UTF-16")) {
        MSWinLineEnd = true;
        return writeUtf16File(fname, textEdit);
    }
    // ask if user wants \r\n
    return promptAndWriteWithChosenEOL(fname, textEdit, encoding, MSWinLineEnd);
}

/*************************/
bool FPwin::writeUtf16File(const QString& fname, TextEdit* textEdit) {
    // write UTF-16 with CRLF
    QStringEncoder encoder = getEncoder(QStringLiteral("UTF-16"));
    QString contents = textEdit->document()->toPlainText();
    contents.replace(QLatin1Char('\n'), QLatin1String("\r\n"));

    const QByteArray bytes = encoder.encode(contents);

    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const qint64 written = file.write(bytes);
    file.flush();
    file.close();
    return written == bytes.size();
}

/*************************/
bool FPwin::promptAndWriteWithChosenEOL(const QString& fname,
                                        TextEdit* textEdit,
                                        const QString& encoding,
                                        bool& MSWinLineEnd) {
    QStringEncoder encoder = getEncoder(encoding);

    // temporarily disable shortcuts around the message box
    updateShortcuts(true);

    MessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.addButton(QMessageBox::Yes);
    msgBox.addButton(QMessageBox::No);
    msgBox.addButton(QMessageBox::Cancel);
    msgBox.changeButtonText(QMessageBox::Yes, tr("Yes"));
    msgBox.changeButtonText(QMessageBox::No, tr("No"));
    msgBox.changeButtonText(QMessageBox::Cancel, tr("Cancel"));
    msgBox.setText(QStringLiteral("<center>%1</center>").arg(tr("Do you want to use <b>MS Windows</b> end-of-lines?")));
    msgBox.setInformativeText(
        QStringLiteral("<center><i>%1</i></center>").arg(tr("This may be good for readability under MS Windows")));
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setWindowModality(Qt::WindowModal);

    const int result = msgBox.exec();
    updateShortcuts(false);
    if (result == QMessageBox::Cancel)
        return false;

    QString contents = textEdit->document()->toPlainText();
    if (result == QMessageBox::Yes) {
        MSWinLineEnd = true;
        contents.replace(QLatin1Char('\n'), QLatin1String("\r\n"));
    }
    else {
        MSWinLineEnd = false;
    }

    const QByteArray bytes = encoder.encode(contents);

    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const qint64 written = file.write(bytes);
    file.flush();
    file.close();
    return written == bytes.size();
}

/*************************/
void FPwin::handleSaveFailure(const QString& fname) {
    closePreviousPages_ = false;

    const QString errorTitle = tr("Cannot be saved!");
    const QString errorInfo = tr("Could not save the file: %1").arg(fname);

    QTimer::singleShot(0, this, [this, errorTitle, errorInfo] {
        showWarningBar(QStringLiteral("<center><b><big>%1</big></b></center>\n<center><i>%2</i></center>")
                           .arg(errorTitle, errorInfo),
                       15);
    });
}

/*************************/
// This is for both Save and Save As
// See savePrompt() for the meanings of first and its following variables
bool FPwin::saveFile(bool keepSyntax,
                     int first,
                     int last,
                     bool closingWindow,
                     QListWidgetItem* curItem,
                     TabPage* curPage) {
    if (!isReady()) {
        closePreviousPages_ = false;
        return false;
    }

    if (!curPage) {
        const int currentIndex = ui->tabWidget->currentIndex();
        curPage = qobject_cast<TabPage*>(ui->tabWidget->widget(currentIndex));
    }
    if (!curPage) {
        closePreviousPages_ = false;
        return false;
    }

    TextEdit* textEdit = curPage->textEdit();
    QString fname = textEdit->getFileName();

    // build filter
    QString filter = tr("All Files") + QStringLiteral(" (*)");
    if (!fname.isEmpty()) {
        const QString ext = QFileInfo(fname).suffix();
        if (!ext.isEmpty())
            filter = QStringLiteral("*.%1;;").arg(ext) + tr("All Files") + QStringLiteral(" (*)");
    }
    else {
        if (fname.isEmpty())
            fname = lastFile_;
    }

    // decide whether to show Save As dialog
    bool restorable = false;
    const QObject* snd = QObject::sender();
    const bool explicitSaveAs = (snd == ui->actionSaveAs);
    const bool explicitSaveCodec = (snd == ui->actionSaveCodec);

    if (fname.isEmpty() || !QFile::exists(fname) || textEdit->getFileName().isEmpty()) {
        if (fname.isEmpty()) {
            fname = QDir::home().filePath(tr("Untitled"));
        }
        else if (!QFile::exists(fname)) {
            QFileInfo fi(fname);
            QDir dir = fi.absoluteDir();
            if (!dir.exists()) {
                dir = QDir::home();
                if (textEdit->getFileName().isEmpty())
                    filter = tr("All Files") + QStringLiteral(" (*)");
            }
            else if (!textEdit->getFileName().isEmpty()) {
                restorable = true;
            }
            fname = dir.filePath(textEdit->getFileName().isEmpty() ? tr("Untitled") : fi.fileName());
        }
        else {
            QFileInfo fi(fname);
            fname = fi.absoluteDir().filePath(tr("Untitled"));
        }

        if (!restorable && !explicitSaveAs && !explicitSaveCodec) {
            if (!showSaveDialogAndSetFileName(fname, filter, tr("Save as..."))) {
                closePreviousPages_ = false;
                return false;
            }
        }
    }

    if (explicitSaveAs) {
        if (!showSaveDialogAndSetFileName(fname, filter, tr("Save as..."))) {
            closePreviousPages_ = false;
            return false;
        }
    }
    else if (explicitSaveCodec) {
        if (!showSaveDialogAndSetFileName(fname, filter, tr("Keep encoding and save as..."))) {
            closePreviousPages_ = false;
            return false;
        }
    }

    // whitespace cleanup
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (config.getRemoveTrailingSpaces())
        removeTrailingSpacesIfNeeded(textEdit);

    // append newline if requested
    if (config.getAppendEmptyLine() && !textEdit->document()->lastBlock().text().isEmpty()) {
        QTextCursor c = textEdit->textCursor();
        c.beginEditBlock();
        c.movePosition(QTextCursor::End);
        c.insertBlock();
        c.endEditBlock();
    }

    // write to disk
    bool success = false;
    bool MSWinLineEnd = false;

    if (explicitSaveCodec) {
        success = writeFileWithEncoding(fname, textEdit, MSWinLineEnd);
    }
    else {
        encodingToCheck(QStringLiteral("UTF-8"));
        QFile file(fname);
        if (file.open(QIODevice::WriteOnly)) {
            QTextDocumentWriter writer(&file, "plaintext");
            success = writer.write(textEdit->document());
            file.flush();
            file.close();
        }
        else {
            success = false;
        }
    }

    if (success) {
        const QFileInfo fi(fname);

        textEdit->document()->setModified(false);
        textEdit->setFileName(fname);
        textEdit->setSize(fi.size());
        textEdit->setLastModified(fi.lastModified());
        ui->actionReload->setDisabled(false);
        setTitle(fname);

        // update tab tooltip
        QString tipDir = fname.contains(QLatin1Char('/')) ? fname.section(QLatin1Char('/'), 0, -2) : fi.absolutePath();
        if (!tipDir.endsWith(QLatin1Char('/')))
            tipDir += QLatin1Char('/');
        const QFontMetrics fm(QToolTip::font());
        const QString elided =
            QStringLiteral("<p style='white-space:pre'>%1</p>")
                .arg(fm.elidedText(tipDir, Qt::ElideMiddle, 200 * fm.horizontalAdvance(QLatin1Char(' '))));
        const int pageIndex = ui->tabWidget->indexOf(curPage);
        ui->tabWidget->setTabToolTip(pageIndex, elided);
        if (!sideItems_.isEmpty()) {
            if (QListWidgetItem* wi = sideItems_.key(curPage))
                wi->setToolTip(elided);
        }

        lastFile_ = fname;
        addRecentFile(lastFile_);

        // reload text if needed or refresh syntax
        const bool outOfRange = (pageIndex <= first || (pageIndex >= last && last >= 0));
        if (outOfRange && textEdit->getEncoding() != checkToEncoding()) {
            loadText(fname, true, true, 0, 0, textEdit->isUneditable(), false);
        }
        else if (!keepSyntax) {
            reloadSyntaxHighlighter(textEdit);
        }

        // make editable if it was read-only and not opened elsewhere
        if (textEdit->isReadOnly() && !alreadyOpen(curPage))
            QTimer::singleShot(0, this, &FPwin::makeEditable);
    }
    else {
        handleSaveFailure(fname);
    }

    return success;
}
/*************************/
void FPwin::reloadSyntaxHighlighter(
    TextEdit* textEdit) {  // uninstall and reinstall the syntax highlighter if the programming language is changed
    QString prevLan = textEdit->getProg();
    setProgLang(textEdit);
    if (prevLan == textEdit->getProg())
        return;

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (config.getShowLangSelector() && config.getSyntaxByDefault()) {
        if (textEdit->getLang() == textEdit->getProg())
            textEdit->setLang(QString());  // not enforced because it's the real syntax
        updateLangBtn(textEdit);
    }

    if (ui->statusBar->isVisible() && textEdit->getWordNumber() != -1) {  // we want to change the statusbar text below
        disconnect(textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
    }

    if (textEdit->getLang().isEmpty()) {  // restart the syntax highlighting only when the language isn't forced
        syntaxHighlighting(textEdit, false);
        if (ui->actionSyntax->isChecked())
            syntaxHighlighting(textEdit);
    }

    if (ui->statusBar->isVisible()) {  // correct the statusbar text just by replacing the old syntax info
        QLabel* statusLabel = ui->statusBar->findChild<QLabel*>("statusLabel");
        QString str = statusLabel->text();
        QString syntaxStr = tr("Syntax");
        int i = str.indexOf(syntaxStr);
        if (i == -1)  // there was no real language before saving (prevLan was "url")
        {
            QString lineStr = "&nbsp;&nbsp;&nbsp;<b>" + tr("Lines");
            int j = str.indexOf(lineStr);
            syntaxStr = "&nbsp;&nbsp;&nbsp;<b>" + tr("Syntax") + QString(":</b> <i>%1</i>").arg(textEdit->getProg());
            str.insert(j, syntaxStr);
        }
        else {
            if (textEdit->getProg() == "url")  // there's no real language after saving
            {
                syntaxStr = "&nbsp;&nbsp;&nbsp;<b>" + tr("Syntax");
                QString lineStr = "&nbsp;&nbsp;&nbsp;<b>" + tr("Lines");
                int j = str.indexOf(syntaxStr);
                int k = str.indexOf(lineStr);
                str.remove(j, k - j);
            }
            else  // the language is changed by saving
            {
                QString lineStr = "</i>&nbsp;&nbsp;&nbsp;<b>" + tr("Lines");
                int j = str.indexOf(lineStr);
                int offset = syntaxStr.size() + 9;  // size of ":</b> <i>"
                str.replace(i + offset, j - i - offset, textEdit->getProg());
            }
        }
        statusLabel->setText(str);
        if (textEdit->getWordNumber() != -1)
            connect(textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
    }
}
/*************************/
void FPwin::lockWindow(TabPage* tabPage, bool lock) {
    locked_ = lock;
    if (lock) {
        pauseAutoSaving(true);
        /* close Session Manager */
        QList<QDialog*> dialogs = findChildren<QDialog*>();
        for (int i = 0; i < dialogs.count(); ++i) {
            if (dialogs.at(i)->objectName() == "sessionDialog") {
                dialogs.at(i)->close();
                break;
            }
        }
    }
    ui->menuBar->setEnabled(!lock);
    const auto allMenus = ui->menuBar->findChildren<QMenu*>();
    for (const auto& thisMenu : allMenus) {
        const auto menuActions = thisMenu->actions();
        for (const auto& menuAction : menuActions)
            menuAction->blockSignals(lock);
    }
    ui->tabWidget->tabBar()->blockSignals(lock);
    ui->tabWidget->tabBar()->lockTabs(lock);
    tabPage->lockPage(lock);
    ui->dockReplace->setEnabled(!lock);
    ui->statusBar->setEnabled(!lock);
    ui->spinBox->setEnabled(!lock);
    ui->checkBox->setEnabled(!lock);
    if (sidePane_)
        sidePane_->lockPane(lock);
    if (!lock) {
        tabPage->textEdit()->setFocus();
        pauseAutoSaving(false);
    }
}
/*************************/
void FPwin::cutText() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->cut();
}
/*************************/
void FPwin::copyText() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->copy();
}
/*************************/
void FPwin::pasteText() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->paste();
}
/*************************/
void FPwin::toSoftTabs() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        makeBusy();
        bool res = tabPage->textEdit()->toSoftTabs();
        unbusy();
        if (res) {
            removeGreenSel();
            showWarningBar("<center><b><big>" + tr("Text tabs are converted to spaces.") + "</big></b></center>");
        }
    }
}
/*************************/
void FPwin::insertDate() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        Config config = static_cast<FPsingleton*>(qApp)->getConfig();
        QString format = config.getDateFormat();
        tabPage->textEdit()->insertPlainText(format.isEmpty()
                                                 ? locale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat)
                                                 : locale().toString(QDateTime::currentDateTime(), format));
    }
}
/*************************/
void FPwin::deleteText() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        if (!textEdit->isReadOnly())
            textEdit->deleteText();
    }
}
/*************************/
void FPwin::selectAllText() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->selectAll();
}
/*************************/
void FPwin::upperCase() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        if (!textEdit->isReadOnly())
            textEdit->insertPlainText(locale().toUpper(textEdit->textCursor().selectedText()));
    }
}
/*************************/
void FPwin::lowerCase() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        if (!textEdit->isReadOnly())
            textEdit->insertPlainText(locale().toLower(textEdit->textCursor().selectedText()));
    }
}
/*************************/
void FPwin::startCase() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        if (!textEdit->isReadOnly()) {
            bool showWarning = false;
            QTextCursor cur = textEdit->textCursor();
            int start = std::min(cur.anchor(), cur.position());
            int end = std::max(cur.anchor(), cur.position());
            if (end > start + 100000) {
                showWarning = true;
                end = start + 100000;
            }

            cur.setPosition(start);
            QString blockText = cur.block().text();
            int blockPos = cur.block().position();
            while (start > blockPos && !blockText.at(start - blockPos - 1).isSpace())
                --start;

            cur.setPosition(end);
            blockText = cur.block().text();
            blockPos = cur.block().position();
            while (end < blockPos + blockText.size() && !blockText.at(end - blockPos).isSpace())
                ++end;

            cur.setPosition(start);
            cur.setPosition(end, QTextCursor::KeepAnchor);
            QString str = locale().toLower(cur.selectedText());

            start = 0;
            QRegularExpressionMatch match;
            /* WARNING: "QTextCursor::selectedText()" uses "U+2029" instead of "\n". */
            while ((start = str.indexOf(QRegularExpression("[^\\s\\-\\.\\n\\x{2029}]+"), start, &match)) > -1) {
                QChar c = str.at(start);
                /* find the first letter from the start of the word */
                int i = 0;
                while (!c.isLetter() && i + 1 < match.capturedLength()) {
                    ++i;
                    c = str.at(start + i);
                }
                str.replace(start + i, 1, c.toUpper());
                start += match.capturedLength();
            }

            cur.beginEditBlock();
            textEdit->setTextCursor(cur);
            textEdit->insertPlainText(str);
            textEdit->ensureCursorVisible();
            cur.endEditBlock();

            if (showWarning)
                showWarningBar("<center><b><big>" + tr("The selected text was too long.") + "</big></b></center>\n" +
                               "<center>" + tr("It is not fully processed.") + "</center>");
        }
    }
}
/*************************/
// Because sort line actions don't have shortcuts, their state can be set when
// their menu is going to be shown. Also, the state of the paste action is set.
void FPwin::showingEditMenu() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        if (!textEdit->isReadOnly()) {
            ui->actionPaste->setEnabled(textEdit->pastingIsPossible());
            if (textEdit->textCursor().selectedText().contains(QChar(QChar::ParagraphSeparator))) {
                ui->actionSortLines->setEnabled(true);
                ui->actionRSortLines->setEnabled(true);
                ui->ActionRmDupeSort->setEnabled(true);
                ui->ActionRmDupeRSort->setEnabled(true);
                ui->ActionSpaceDupeSort->setEnabled(true);
                ui->ActionSpaceDupeRSort->setEnabled(true);
                return;
            }
        }
        else
            ui->actionPaste->setEnabled(false);
    }
    else
        ui->actionPaste->setEnabled(false);
    ui->actionSortLines->setEnabled(false);
    ui->actionRSortLines->setEnabled(false);
    ui->ActionRmDupeSort->setEnabled(false);
    ui->ActionRmDupeRSort->setEnabled(false);
    ui->ActionSpaceDupeSort->setEnabled(false);
    ui->ActionSpaceDupeRSort->setEnabled(false);
}
/*************************/
void FPwin::hidngEditMenu() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        /* QPlainTextEdit::canPaste() isn't consulted because it might change later */
        ui->actionPaste->setEnabled(!tabPage->textEdit()->isReadOnly());
    }
    else
        ui->actionPaste->setEnabled(false);
}
/*************************/
void FPwin::sortLines() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->sortLines(qobject_cast<QAction*>(QObject::sender()) == ui->actionRSortLines);
}

void FPwin::rmDupeSort() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->rmDupeSort(qobject_cast<QAction*>(QObject::sender()) == ui->ActionRmDupeRSort);
}

void FPwin::spaceDupeSort() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->spaceDupeSort(qobject_cast<QAction*>(QObject::sender()) == ui->ActionSpaceDupeRSort);
}
/*************************/
void FPwin::makeEditable() {
    if (!isReady())
        return;

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    bool textIsSelected = textEdit->textCursor().hasSelection();
    bool hasColumn = !textEdit->getColSel().isEmpty();

    textEdit->setReadOnly(false);
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (!textEdit->hasDarkScheme()) {
        textEdit->viewport()->setStyleSheet(QString(".QWidget {"
                                                    "color: black;"
                                                    "background-color: rgb(%1, %1, %1);}")
                                                .arg(config.getLightBgColorValue()));
    }
    else {
        textEdit->viewport()->setStyleSheet(QString(".QWidget {"
                                                    "color: white;"
                                                    "background-color: rgb(%1, %1, %1);}")
                                                .arg(config.getDarkBgColorValue()));
    }
    ui->actionEdit->setVisible(false);

    ui->actionPaste->setEnabled(true);  // it might change temporarily in showingEditMenu()
    ui->actionSoftTab->setEnabled(true);
    ui->actionDate->setEnabled(true);
    ui->actionCopy->setEnabled(textIsSelected || hasColumn);
    ui->actionCut->setEnabled(textIsSelected || hasColumn);
    ui->actionDelete->setEnabled(textIsSelected || hasColumn);
    ui->actionUpperCase->setEnabled(textIsSelected);
    ui->actionLowerCase->setEnabled(textIsSelected);
    ui->actionStartCase->setEnabled(textIsSelected);
    connect(textEdit, &TextEdit::canCopy, ui->actionCut, &QAction::setEnabled);
    connect(textEdit, &TextEdit::canCopy, ui->actionDelete, &QAction::setEnabled);
    connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionUpperCase, &QAction::setEnabled);
    connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionLowerCase, &QAction::setEnabled);
    connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionStartCase, &QAction::setEnabled);
    if (config.getSaveUnmodified())
        ui->actionSave->setEnabled(true);
}
/*************************/
void FPwin::undoing() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->undo();
}
/*************************/
void FPwin::redoing() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->redo();
}
/*************************/
void FPwin::changeTab(QListWidgetItem* current) {
    if (!sidePane_ || sideItems_.isEmpty())
        return;
    /* "current" is never null; see the c-tor of ListWidget in "sidepane.cpp" */
    ui->tabWidget->setCurrentWidget(sideItems_.value(current));
}
/*************************/
// Called immediately after changing tab (closes the warningbar if it isn't needed)
void FPwin::onTabChanged(int index) {
    if (index > -1) {
        QString fname = qobject_cast<TabPage*>(ui->tabWidget->widget(index))->textEdit()->getFileName();
        if (fname.isEmpty() || QFile::exists(fname))
            closeWarningBar();
    }
    else
        closeWarningBar();
}
/*************************/
// Called with a timeout after tab switching (changes the window title, sets action states, etc.)
void FPwin::tabSwitch(int index) {
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index));
    if (tabPage == nullptr) {
        setWindowTitle("FeatherPad[*]");
        if (auto label = qobject_cast<QLabel*>(ui->menuBar->cornerWidget()))
            label->clear();
        setWindowModified(false);
        return;
    }

    TextEdit* textEdit = tabPage->textEdit();
    if (!tabPage->isSearchBarVisible() && !sidePane_)
        textEdit->setFocus();
    QString fname = textEdit->getFileName();
    bool modified(textEdit->document()->isModified());

    QFileInfo info;
    QString shownName;
    if (fname.isEmpty()) {
        if (textEdit->getProg() == "help")
            shownName = "** " + tr("Help") + " **";
        else
            shownName = tr("Untitled");
    }
    else {
        info.setFile(fname);
        shownName = (fname.contains("/") ? fname : info.absolutePath() + "/" + fname);
        if (!QFile::exists(fname))
            onOpeningNonexistent();
        else if (textEdit->getLastModified() != info.lastModified())
            showWarningBar("<center><b><big>" + tr("This file has been modified elsewhere or in another way!") +
                               "</big></b></center>\n" + "<center>" +
                               tr("Please be careful about reloading or saving this document!") + "</center>",
                           15);
    }
    if (modified)
        shownName.prepend("*");
    setWinTitle(shownName);

    /* although the window size, wrapping state or replacing text may have changed or
       the replace dock may have been closed, hlight() will be called automatically */
    // if (!textEdit->getSearchedText().isEmpty()) hlight();

    /* correct the encoding menu */
    encodingToCheck(textEdit->getEncoding());

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();

    /* correct the states of some buttons */
    ui->actionUndo->setEnabled(textEdit->document()->isUndoAvailable());
    ui->actionRedo->setEnabled(textEdit->document()->isRedoAvailable());
    bool readOnly = textEdit->isReadOnly();
    if (!config.getSaveUnmodified())
        ui->actionSave->setEnabled(modified);
    else
        ui->actionSave->setDisabled(readOnly || textEdit->isUneditable());
    ui->actionReload->setEnabled(!fname.isEmpty());
    if (fname.isEmpty() && !modified && !textEdit->document()->isEmpty())  // 'Help' is an exception
    {
        ui->actionEdit->setVisible(false);
        ui->actionSaveAs->setEnabled(true);
        ui->actionSaveCodec->setEnabled(true);
    }
    else {
        ui->actionEdit->setVisible(readOnly && !textEdit->isUneditable());
        ui->actionSaveAs->setEnabled(!textEdit->isUneditable());
        ui->actionSaveCodec->setEnabled(!textEdit->isUneditable());
    }
    ui->actionPaste->setEnabled(!readOnly);  // it might change temporarily in showingEditMenu()
    ui->actionSoftTab->setEnabled(!readOnly);
    ui->actionDate->setEnabled(!readOnly);
    bool textIsSelected = textEdit->textCursor().hasSelection();
    bool hasColumn = !textEdit->getColSel().isEmpty();
    ui->actionCopy->setEnabled(textIsSelected || hasColumn);
    ui->actionCut->setEnabled(!readOnly && (textIsSelected || hasColumn));
    ui->actionDelete->setEnabled(!readOnly && (textIsSelected || hasColumn));
    ui->actionUpperCase->setEnabled(!readOnly && textIsSelected);
    ui->actionLowerCase->setEnabled(!readOnly && textIsSelected);
    ui->actionStartCase->setEnabled(!readOnly && textIsSelected);

    if (isScriptLang(textEdit->getProg()) && info.isExecutable())
        ui->actionRun->setVisible(config.getExecuteScripts());
    else
        ui->actionRun->setVisible(false);

    /* handle the spinbox */
    if (ui->spinBox->isVisible())
        ui->spinBox->setMaximum(textEdit->document()->blockCount());

    /* handle the statusbar */
    if (ui->statusBar->isVisible()) {
        statusMsgWithLineCount(textEdit->document()->blockCount());
        QToolButton* wordButton = ui->statusBar->findChild<QToolButton*>("wordButton");
        if (textEdit->getWordNumber() == -1) {
            if (wordButton)
                wordButton->setVisible(true);
            if (textEdit->document()->isEmpty())  // make an exception
                updateWordInfo();
        }
        else {
            if (wordButton)
                wordButton->setVisible(false);
            QLabel* statusLabel = ui->statusBar->findChild<QLabel*>("statusLabel");
            statusLabel->setText(
                QString("%1 <i>%2</i>").arg(statusLabel->text(), locale().toString(textEdit->getWordNumber())));
        }
        showCursorPos();
    }
    if (config.getShowLangSelector() && config.getSyntaxByDefault())
        updateLangBtn(textEdit);

    /* al last, set the title of Replacment dock */
    if (ui->dockReplace->isVisible()) {
        QString title = textEdit->getReplaceTitle();
        if (!title.isEmpty())
            ui->dockReplace->setWindowTitle(title);
        else
            ui->dockReplace->setWindowTitle(tr("Replacement"));
    }
    else
        textEdit->setReplaceTitle(QString());
}
/*************************/
void FPwin::fontDialog() {
    if (isLoading())
        return;

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr)
        return;

    if (hasAnotherDialog())
        return;
    updateShortcuts(true);

    TextEdit* textEdit = tabPage->textEdit();

    QFont currentFont = textEdit->getDefaultFont();
    FontDialog fd(currentFont, this);
    fd.setWindowModality(Qt::WindowModal);
    /*fd.move (x() + width()/2 - fd.width()/2,
             y() + height()/2 - fd.height()/ 2);*/
    if (fd.exec()) {
        QFont newFont = fd.selectedFont();
        Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
        if (config.getRemFont()) {
            config.setFont(newFont);
            config.writeConfig();

            FPsingleton* singleton = static_cast<FPsingleton*>(qApp);
            for (int i = 0; i < singleton->Wins.count(); ++i) {
                FPwin* thisWin = singleton->Wins.at(i);
                for (int j = 0; j < thisWin->ui->tabWidget->count(); ++j) {
                    TextEdit* thisTextEdit = qobject_cast<TabPage*>(thisWin->ui->tabWidget->widget(j))->textEdit();
                    thisTextEdit->setEditorFont(newFont);
                }
            }
        }
        else
            textEdit->setEditorFont(newFont);

        /* the font can become larger... */
        textEdit->adjustScrollbars();
        /* ... or smaller */
        reformat(textEdit);
    }
    updateShortcuts(false);
}
/*************************/
void FPwin::changeEvent(QEvent* event) {
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
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
        /* if the window gets maximized/fullscreen, remember its position and size */
        if ((windowState() & Qt::WindowMaximized) || (windowState() & Qt::WindowFullScreen)) {
            if (auto stateEvent = static_cast<QWindowStateChangeEvent*>(event)) {
                if (!(stateEvent->oldState() & Qt::WindowMaximized) &&
                    !(stateEvent->oldState() & Qt::WindowFullScreen)) {
                    if (config.getRemPos() && !static_cast<FPsingleton*>(qApp)->isWayland())
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
void FPwin::showEvent(QShowEvent* event) {
    /* To position the main window correctly when it's shown for
       the first time, we call setGeometry() inside showEvent(). */
    if (!shownBefore_ && !event->spontaneous()) {
        shownBefore_ = true;
        Config config = static_cast<FPsingleton*>(qApp)->getConfig();
        if (config.getRemPos() && !static_cast<FPsingleton*>(qApp)->isWayland()) {
            QSize theSize = (config.getRemSize() ? config.getWinSize() : config.getStartSize());
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
bool FPwin::event(QEvent* event) {
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
            TextEdit* textEdit = tabPage->textEdit();
            QString fname = textEdit->getFileName();
            if (!fname.isEmpty()) {
                if (!QFile::exists(fname)) {
                    if (isLoading())
                        connect(this, &FPwin::finishedLoading, this, &FPwin::onOpeningNonexistent,
                                Qt::UniqueConnection);
                    else
                        onOpeningNonexistent();
                }
                else if (textEdit->getLastModified() != QFileInfo(fname).lastModified())
                    showWarningBar("<center><b><big>" + tr("This file has been modified elsewhere or in another way!") +
                                       "</big></b></center>\n" + "<center>" +
                                       tr("Please be careful about reloading or saving this document!") + "</center>",
                                   15);
            }
        }
    }
    return QMainWindow::event(event);
}
/*************************/
void FPwin::showHideSearch() {
    if (!isReady())
        return;

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr)
        return;

    bool isFocused = tabPage->isSearchBarVisible() && tabPage->searchBarHasFocus();

    if (!isFocused)
        tabPage->focusSearchBar();
    else {
        ui->dockReplace->setVisible(false);  // searchbar is needed by replace dock
        /* return focus to the document,... */
        tabPage->textEdit()->setFocus();
    }

    int count = ui->tabWidget->count();
    for (int indx = 0; indx < count; ++indx) {
        TabPage* page = qobject_cast<TabPage*>(ui->tabWidget->widget(indx));
        if (isFocused) {
            /* ... remove all yellow and green highlights... */
            TextEdit* textEdit = page->textEdit();
            textEdit->setSearchedText(QString());
            QList<QTextEdit::ExtraSelection> es;
            textEdit->setGreenSel(es);  // not needed
            if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
                es.prepend(textEdit->currentLineSelection());
            es.append(textEdit->getBlueSel());
            es.append(textEdit->getColSel());
            es.append(textEdit->getRedSel());
            textEdit->setExtraSelections(es);
            /* ... and empty all search entries */
            page->clearSearchEntry();
        }
        page->setSearchBarVisible(!isFocused);
    }
}
/*************************/
void FPwin::jumpTo() {
    if (!isReady())
        return;

    bool visibility = ui->spinBox->isVisible();

    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        TextEdit* thisTextEdit = qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit();
        if (!ui->actionLineNumbers->isChecked())
            thisTextEdit->showLineNumbers(!visibility);

        if (!visibility) {
            /* setMaximum() isn't a slot */
            connect(thisTextEdit->document(), &QTextDocument::blockCountChanged, this, &FPwin::setMax);
        }
        else
            disconnect(thisTextEdit->document(), &QTextDocument::blockCountChanged, this, &FPwin::setMax);
    }

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage) {
        if (!visibility && ui->tabWidget->count() > 0)
            ui->spinBox->setMaximum(tabPage->textEdit()->document()->blockCount());
    }
    ui->spinBox->setVisible(!visibility);
    ui->label->setVisible(!visibility);
    ui->checkBox->setVisible(!visibility);
    if (!visibility) {
        ui->spinBox->setFocus();
        ui->spinBox->selectAll();
    }
    else if (tabPage) /* return focus to doc */
        tabPage->textEdit()->setFocus();
}
/*************************/
void FPwin::setMax(const int max) {
    ui->spinBox->setMaximum(max);
}
/*************************/
void FPwin::goTo() {
    /* workaround for not being able to use returnPressed()
       because of protectedness of spinbox's QLineEdit */
    if (!ui->spinBox->hasFocus())
        return;

    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        QTextBlock block = textEdit->document()->findBlockByNumber(ui->spinBox->value() - 1);
        int pos = block.position();
        QTextCursor start = textEdit->textCursor();
        if (ui->checkBox->isChecked())
            start.setPosition(pos, QTextCursor::KeepAnchor);
        else
            start.setPosition(pos);
        textEdit->setTextCursor(start);
    }
}
/*************************/
void FPwin::showLN(bool checked) {
    int count = ui->tabWidget->count();
    if (count == 0)
        return;

    if (checked) {
        for (int i = 0; i < count; ++i)
            qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit()->showLineNumbers(true);
    }
    else if (!ui->spinBox->isVisible())  // also the spinBox affects line numbers visibility
    {
        for (int i = 0; i < count; ++i)
            qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit()->showLineNumbers(false);
    }
}
/*************************/
void FPwin::toggleWrapping() {
    int count = ui->tabWidget->count();
    if (count == 0)
        return;

    bool wrapLines = ui->actionWrap->isChecked();
    for (int i = 0; i < count; ++i) {
        auto textEdit = qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit();
        textEdit->setLineWrapMode(wrapLines ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
        textEdit->removeColumnHighlight();
    }
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        reformat(tabPage->textEdit());
}
/*************************/
void FPwin::toggleIndent() {
    int count = ui->tabWidget->count();
    if (count == 0)
        return;

    if (ui->actionIndent->isChecked()) {
        for (int i = 0; i < count; ++i)
            qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit()->setAutoIndentation(true);
    }
    else {
        for (int i = 0; i < count; ++i)
            qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit()->setAutoIndentation(false);
    }
}
/*************************/
void FPwin::encodingToCheck(const QString& encoding) {
    ui->actionOther->setDisabled(true);

    if (encoding == "UTF-8")
        ui->actionUTF_8->setChecked(true);
    else if (encoding == "UTF-16")
        ui->actionUTF_16->setChecked(true);
    else if (encoding == "ISO-8859-1")
        ui->actionISO_8859_1->setChecked(true);
    else {
        ui->actionOther->setDisabled(false);
        ui->actionOther->setChecked(true);
    }
}
/*************************/
const QString FPwin::checkToEncoding() const {
    QString encoding;

    if (ui->actionUTF_8->isChecked())
        encoding = "UTF-8";
    else if (ui->actionUTF_16->isChecked())
        encoding = "UTF-16";
    else if (ui->actionISO_8859_1->isChecked())
        encoding = "ISO-8859-1";
    else
        encoding = "UTF-8";

    return encoding;
}
/*************************/
void FPwin::docProp() {
    bool showCurPos = static_cast<FPsingleton*>(qApp)->getConfig().getShowCursorPos();
    if (ui->statusBar->isVisible()) {
        for (int i = 0; i < ui->tabWidget->count(); ++i) {
            TextEdit* thisTextEdit = qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit();
            disconnect(thisTextEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
            disconnect(thisTextEdit, &TextEdit::selChanged, this, &FPwin::statusMsg);
            if (showCurPos)
                disconnect(thisTextEdit, &QPlainTextEdit::cursorPositionChanged, this, &FPwin::showCursorPos);
            /* don't delete the cursor position label because the statusbar might be shown later */
        }
        ui->statusBar->setVisible(false);
        return;
    }

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr)
        return;

    statusMsgWithLineCount(tabPage->textEdit()->document()->blockCount());
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        TextEdit* thisTextEdit = qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit();
        connect(thisTextEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
        connect(thisTextEdit, &TextEdit::selChanged, this, &FPwin::statusMsg);
        if (showCurPos)
            connect(thisTextEdit, &QPlainTextEdit::cursorPositionChanged, this, &FPwin::showCursorPos);
    }

    ui->statusBar->setVisible(true);
    if (showCurPos) {
        addCursorPosLabel();
        showCursorPos();
    }
    if (QToolButton* wordButton = ui->statusBar->findChild<QToolButton*>("wordButton"))
        wordButton->setVisible(true);
    updateWordInfo();
}
/*************************/
// Set the status bar text according to the block count.
void FPwin::statusMsgWithLineCount(const int lines) {
    TextEdit* textEdit = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())->textEdit();
    /* ensure that the signal comes from the active tab if this is about a connection */
    if (qobject_cast<TextEdit*>(QObject::sender()) && QObject::sender() != textEdit)
        return;

    QLabel* statusLabel = ui->statusBar->findChild<QLabel*>("statusLabel");

    /* the order: Encoding -> Syntax -> Lines -> Sel. Chars -> Words */
    QString encodStr = "<b>" + tr("Encoding") + QString(":</b> <i>%1</i>").arg(textEdit->getEncoding());
    QString syntaxStr;
    if (textEdit->getProg() != "help" && textEdit->getProg() != "url")
        syntaxStr = "&nbsp;&nbsp;&nbsp;<b>" + tr("Syntax") + QString(":</b> <i>%1</i>").arg(textEdit->getProg());
    QLocale l = locale();
    QString lineStr = "&nbsp;&nbsp;&nbsp;<b>" + tr("Lines") + QString(":</b> <i>%1</i>").arg(l.toString(lines));
    QString selStr = "&nbsp;&nbsp;&nbsp;<b>" + tr("Sel. Chars") +
                     QString(":</b> <i>%1</i>").arg(l.toString(textEdit->selectionSize()));
    QString wordStr = "&nbsp;&nbsp;&nbsp;<b>" + tr("Words") + ":</b>";

    statusLabel->setText(encodStr + syntaxStr + lineStr + selStr + wordStr);
}
/*************************/
// Change the status bar text when the selection changes.
void FPwin::statusMsg() {
    QLocale l = locale();
    QLabel* statusLabel = ui->statusBar->findChild<QLabel*>("statusLabel");
    int sel = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())->textEdit()->selectionSize();
    QString str = statusLabel->text();
    QString selStr = tr("Sel. Chars");
    QString wordStr = "&nbsp;&nbsp;&nbsp;<b>" + tr("Words");
    int i = str.indexOf(selStr) + selStr.size();
    int j = str.indexOf(wordStr);
    if (sel == 0) {
        QString prevSel = str.mid(i + 9, j - i - 13);  // j - i - 13 --> j - (i + 9[":</b> <i>]") - 4["</i>"]
        if (l.toInt(prevSel) == 0)
            return;
    }
    QString charN = l.toString(sel);
    str.replace(i + 9, j - i - 13, charN);
    statusLabel->setText(str);
}
/*************************/
void FPwin::showCursorPos() {
    QLabel* posLabel = ui->statusBar->findChild<QLabel*>("posLabel");
    if (!posLabel)
        return;

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr)
        return;

    int pos = tabPage->textEdit()->textCursor().positionInBlock();
    QString charN = "<i> " + locale().toString(pos) + "</i>";
    QString str = posLabel->text();
    QString scursorStr = "<b>" + tr("Position:") + "</b>";
    int i = scursorStr.size();
    str.replace(i, str.size() - i, charN);
    posLabel->setText(str);
}
/*************************/
void FPwin::updateLangBtn(TextEdit* textEdit) {
    QToolButton* langButton = ui->statusBar->findChild<QToolButton*>("langButton");
    if (!langButton)
        return;

    langButton->setEnabled(!textEdit->isUneditable() && textEdit->getHighlighter());

    QString lang = textEdit->getLang().isEmpty() ? textEdit->getProg() : textEdit->getLang();
    QAction* action = langs_.value(lang);
    if (!action)  // it's "help", "url" or a bug (some language isn't included)
    {
        lang = tr("Normal");
        action = langs_.value(lang);  // "Normal" is the last action
    }
    langButton->setText(lang);
    if (action)  // always the case
        action->setChecked(true);
}
/*************************/
void FPwin::enforceLang(QAction* action) {
    QToolButton* langButton = ui->statusBar->findChild<QToolButton*>("langButton");
    if (!langButton)
        return;

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    QString lang = action->text();
    lang.remove('&');  // because of KAcceleratorManager
    langButton->setText(lang);
    if (lang == tr("Normal")) {
        if (textEdit->getProg() == "desktop" || textEdit->getProg() == "theme" || textEdit->getProg() == "openbox" ||
            textEdit->getProg() == "changelog" || textEdit->getProg() == "srt" ||
            textEdit->getProg() == "gtkrc") {  // not listed by the language button
            lang = textEdit->getProg();
        }
        else
            lang = "url";  // the default highlighter
    }
    if (textEdit->getProg() == lang || textEdit->getProg() == "help")
        textEdit->setLang(QString());  // not enforced
    else
        textEdit->setLang(lang);
    if (ui->actionSyntax->isChecked()) {
        syntaxHighlighting(textEdit, false);
        makeBusy();  // it may take a while with huge texts
        syntaxHighlighting(textEdit, true, lang);
        QTimer::singleShot(0, this, &FPwin::unbusy);
    }
}
/*************************/
void FPwin::updateWordInfo(int /*position*/, int charsRemoved, int charsAdded) {
    QToolButton* wordButton = ui->statusBar->findChild<QToolButton*>("wordButton");
    if (!wordButton)
        return;
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr)
        return;
    TextEdit* textEdit = tabPage->textEdit();
    /* ensure that the signal comes from the active tab (when the info is going to be removed) */
    if (qobject_cast<QTextDocument*>(QObject::sender()) && QObject::sender() != textEdit->document())
        return;

    if (wordButton->isVisible()) {
        QLabel* statusLabel = ui->statusBar->findChild<QLabel*>("statusLabel");
        int words = textEdit->getWordNumber();
        if (words == -1) {
            words = textEdit->toPlainText().split(QRegularExpression("(\\s|\\n|\\r)+"), Qt::SkipEmptyParts).count();
            textEdit->setWordNumber(words);
        }

        wordButton->setVisible(false);
        statusLabel->setText(QString("%1 <i>%2</i>").arg(statusLabel->text(), locale().toString(words)));
        connect(textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
    }
    else if (charsRemoved > 0 || charsAdded > 0)  // not if only the format is changed
    {
        disconnect(textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
        textEdit->setWordNumber(-1);
        wordButton->setVisible(true);
        statusMsgWithLineCount(textEdit->document()->blockCount());
    }
}
/*************************/
void FPwin::filePrint() {
    if (isLoading() || hasAnotherDialog())
        return;

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr)
        return;

    showWarningBar("<center><b><big>" + tr("Printing in progress...") + "</big></b></center>", 0);
    lockWindow(tabPage, true);

    TextEdit* textEdit = tabPage->textEdit();

    /* complete the syntax highlighting when printing
       because the whole document may not be highlighted */
    makeBusy();
    if (Highlighter* highlighter = qobject_cast<Highlighter*>(textEdit->getHighlighter())) {
        QTextCursor start = textEdit->textCursor();
        start.movePosition(QTextCursor::Start);
        QTextCursor end = textEdit->textCursor();
        end.movePosition(QTextCursor::End);
        highlighter->setLimit(start, end);
        QTextBlock block = start.block();
        while (block.isValid() && block.blockNumber() <= end.blockNumber()) {
            if (TextBlockData* data = static_cast<TextBlockData*>(block.userData())) {
                if (!data->isHighlighted())
                    highlighter->rehighlightBlock(block);
            }
            block = block.next();
        }
    }
    QTimer::singleShot(0, this, &FPwin::unbusy);  // wait for the dialog too

    /* choose an appropriate name and directory */
    QString fileName = textEdit->getFileName();
    if (fileName.isEmpty()) {
        QDir dir = QDir::home();
        fileName = dir.filePath(tr("Untitled"));
    }
    fileName.append(".pdf");

    bool Use96Dpi = QCoreApplication::instance()->testAttribute(Qt::AA_Use96Dpi);
    QScreen* screen = QGuiApplication::primaryScreen();
    double sourceDpiX = Use96Dpi ? 96 : screen ? screen->logicalDotsPerInchX() : 100;
    double sourceDpiY = Use96Dpi ? 96 : screen ? screen->logicalDotsPerInchY() : 100;
    Printing* thread = new Printing(textEdit->document(), fileName, textEdit->getTextPrintColor(),
                                    textEdit->getDarkValue(), sourceDpiX, sourceDpiY);

    QPrintDialog dlg(thread->printer(), this);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowTitle(tr("Print Document"));
    if (dlg.exec() == QDialog::Accepted) {
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        connect(thread, &QThread::finished, tabPage, [this, tabPage] {
            lockWindow(tabPage, false);
            showWarningBar("<center><b><big>" + tr("Printing completed.") + "</big></b></center>");
        });
        thread->start();
    }
    else {
        delete thread;
        lockWindow(tabPage, false);
        closeWarningBar();
    }
}
/*************************/
void FPwin::nextTab() {
    if (isLoading())
        return;

    int index = ui->tabWidget->currentIndex();
    if (index == -1)
        return;

    if (sidePane_) {
        int curRow = sidePane_->listWidget()->currentRow();
        if (curRow < 0 && !sideItems_.isEmpty()) {
            if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index))) {
                if (QListWidgetItem* wi = sideItems_.key(tabPage))
                    curRow = sidePane_->listWidget()->row(wi);
            }
        }
        if (curRow == sidePane_->listWidget()->count() - 1) {
            if (static_cast<FPsingleton*>(qApp)->getConfig().getTabWrapAround())
                sidePane_->listWidget()->setCurrentRow(0);
        }
        else
            sidePane_->listWidget()->setCurrentRow(curRow + 1);
    }
    else {
        if (QWidget* widget = ui->tabWidget->widget(index + 1))
            ui->tabWidget->setCurrentWidget(widget);
        else if (static_cast<FPsingleton*>(qApp)->getConfig().getTabWrapAround())
            ui->tabWidget->setCurrentIndex(0);
    }
}
/*************************/
void FPwin::previousTab() {
    if (isLoading())
        return;

    int index = ui->tabWidget->currentIndex();
    if (index == -1)
        return;

    if (sidePane_) {
        int curRow = sidePane_->listWidget()->currentRow();
        if (curRow < 0 && !sideItems_.isEmpty()) {
            if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index))) {
                if (QListWidgetItem* wi = sideItems_.key(tabPage))
                    curRow = sidePane_->listWidget()->row(wi);
            }
        }
        if (curRow == 0) {
            if (static_cast<FPsingleton*>(qApp)->getConfig().getTabWrapAround())
                sidePane_->listWidget()->setCurrentRow(sidePane_->listWidget()->count() - 1);
        }
        else
            sidePane_->listWidget()->setCurrentRow(curRow - 1);
    }
    else {
        if (QWidget* widget = ui->tabWidget->widget(index - 1))
            ui->tabWidget->setCurrentWidget(widget);
        else if (static_cast<FPsingleton*>(qApp)->getConfig().getTabWrapAround()) {
            int count = ui->tabWidget->count();
            if (count > 0)
                ui->tabWidget->setCurrentIndex(count - 1);
        }
    }
}
/*************************/
void FPwin::lastTab() {
    if (isLoading())
        return;

    if (sidePane_) {
        int count = sidePane_->listWidget()->count();
        if (count > 0)
            sidePane_->listWidget()->setCurrentRow(count - 1);
    }
    else {
        int count = ui->tabWidget->count();
        if (count > 0)
            ui->tabWidget->setCurrentIndex(count - 1);
    }
}
/*************************/
void FPwin::firstTab() {
    if (isLoading())
        return;

    if (sidePane_) {
        if (sidePane_->listWidget()->count() > 0)
            sidePane_->listWidget()->setCurrentRow(0);
    }
    else if (ui->tabWidget->count() > 0)
        ui->tabWidget->setCurrentIndex(0);
}
/*************************/
void FPwin::lastActiveTab() {
    if (sidePane_) {
        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->getLastActiveTab())) {
            if (QListWidgetItem* wi = sideItems_.key(tabPage))
                sidePane_->listWidget()->setCurrentItem(wi);
        }
    }
    else
        ui->tabWidget->selectLastActiveTab();
}
/*************************/
void FPwin::detachTab() {
    if (!isReady())
        return;

    int index = -1;
    if (sidePane_ && rightClicked_ >= 0)
        index = ui->tabWidget->indexOf(sideItems_.value(sidePane_->listWidget()->item(rightClicked_)));
    else
        index = ui->tabWidget->currentIndex();
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index));
    if (tabPage == nullptr || ui->tabWidget->count() == 1) {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();

    /*****************************************************
     *****          Get all necessary info.          *****
     ***** Then, remove the tab but keep its widget. *****
     *****************************************************/

    QString tooltip = ui->tabWidget->tabToolTip(index);
    QString tabText = ui->tabWidget->tabText(index);
    QString title = windowTitle();
    bool hl = true;
    bool spin = false;
    bool ln = false;
    bool status = false;
    bool statusCurPos = false;
    if (!ui->actionSyntax->isChecked())
        hl = false;
    if (ui->spinBox->isVisible())
        spin = true;
    if (ui->actionLineNumbers->isChecked())
        ln = true;
    if (ui->statusBar->isVisible()) {
        status = true;
        if (ui->statusBar->findChild<QLabel*>("posLabel"))
            statusCurPos = true;
    }

    TextEdit* textEdit = tabPage->textEdit();

    disconnect(textEdit, &TextEdit::resized, this, &FPwin::hlight);
    disconnect(textEdit, &TextEdit::updateRect, this, &FPwin::hlight);
    disconnect(textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
    if (status) {
        disconnect(textEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
        disconnect(textEdit, &TextEdit::selChanged, this, &FPwin::statusMsg);
        if (statusCurPos)
            disconnect(textEdit, &QPlainTextEdit::cursorPositionChanged, this, &FPwin::showCursorPos);
    }
    disconnect(textEdit, &TextEdit::canCopy, ui->actionCut, &QAction::setEnabled);
    disconnect(textEdit, &TextEdit::canCopy, ui->actionDelete, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionUpperCase, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionLowerCase, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionStartCase, &QAction::setEnabled);
    disconnect(textEdit, &TextEdit::canCopy, ui->actionCopy, &QAction::setEnabled);
    disconnect(textEdit, &QWidget::customContextMenuRequested, this, &FPwin::editorContextMenu);
    disconnect(textEdit, &TextEdit::zoomedOut, this, &FPwin::reformat);
    disconnect(textEdit, &TextEdit::hugeColumn, this, &FPwin::columnWarning);
    disconnect(textEdit, &TextEdit::filePasted, this, &FPwin::newTabFromName);
    disconnect(textEdit, &TextEdit::updateBracketMatching, this, &FPwin::matchBrackets);
    disconnect(textEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::formatOnBlockChange);
    disconnect(textEdit, &TextEdit::updateRect, this, &FPwin::formatTextRect);
    disconnect(textEdit, &TextEdit::resized, this, &FPwin::formatTextRect);

    disconnect(textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
    disconnect(textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::formatOnTextChange);
    disconnect(textEdit->document(), &QTextDocument::blockCountChanged, this, &FPwin::setMax);
    disconnect(textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::asterisk);
    disconnect(textEdit->document(), &QTextDocument::undoAvailable, ui->actionUndo, &QAction::setEnabled);
    disconnect(textEdit->document(), &QTextDocument::redoAvailable, ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        disconnect(textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::enableSaving);

    disconnect(tabPage, &TabPage::find, this, &FPwin::find);
    disconnect(tabPage, &TabPage::searchFlagChanged, this, &FPwin::searchFlagChanged);

    /* for tabbar to be updated peoperly with tab reordering during a
       fast drag-and-drop, mouse should be released before tab removal */
    ui->tabWidget->tabBar()->releaseMouse();

    ui->tabWidget->removeTab(index);
    if (ui->tabWidget->count() == 1)
        updateGUIForSingleTab(true);
    if (sidePane_ && !sideItems_.isEmpty()) {
        if (QListWidgetItem* wi = sideItems_.key(tabPage)) {
            sideItems_.remove(wi);
            delete sidePane_->listWidget()->takeItem(sidePane_->listWidget()->row(wi));
        }
    }

    /*******************************************************************
     ***** create a new window and replace its tab by this widget. *****
     *******************************************************************/

    FPsingleton* singleton = static_cast<FPsingleton*>(qApp);
    FPwin* dropTarget = singleton->newWin();

    /* remove the single empty tab, as in closeTabAtIndex() */
    dropTarget->deleteTabPage(0, false, false);
    dropTarget->ui->actionReload->setDisabled(true);
    dropTarget->ui->actionSave->setDisabled(true);
    dropTarget->enableWidgets(false);

    /* first, set the new info... */
    dropTarget->lastFile_ = textEdit->getFileName();
    textEdit->setGreenSel(QList<QTextEdit::ExtraSelection>());
    textEdit->setRedSel(QList<QTextEdit::ExtraSelection>());
    /* ... then insert the detached widget... */
    dropTarget->enableWidgets(true);  // the tab will be inserted and switched to below
    QFileInfo lastFileInfo(dropTarget->lastFile_);
    bool isLink = dropTarget->lastFile_.isEmpty() ? false : lastFileInfo.isSymLink();
    bool hasFinalTarget(false);
    if (!isLink) {
        const QString finalTarget = lastFileInfo.canonicalFilePath();
        hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != dropTarget->lastFile_);
    }
    dropTarget->ui->tabWidget->insertTab(0, tabPage,
                                         isLink           ? QIcon(":icons/link.svg")
                                         : hasFinalTarget ? QIcon(":icons/hasTarget.svg")
                                                          : QIcon(),
                                         tabText);
    if (dropTarget->sidePane_) {
        ListWidget* lw = dropTarget->sidePane_->listWidget();
        QString fname = textEdit->getFileName();
        if (fname.isEmpty()) {
            if (textEdit->getProg() == "help")
                fname = "** " + tr("Help") + " **";
            else
                fname = tr("Untitled");
        }
        else
            fname = fname.section('/', -1);
        if (textEdit->document()->isModified())
            fname.append("*");
        fname.replace("\n", " ");
        ListWidgetItem* lwi = new ListWidgetItem(isLink           ? QIcon(":icons/link.svg")
                                                 : hasFinalTarget ? QIcon(":icons/hasTarget.svg")
                                                                  : QIcon(),
                                                 fname, lw);
        lw->setToolTip(tooltip);
        dropTarget->sideItems_.insert(lwi, tabPage);
        lw->addItem(lwi);
        lw->setCurrentItem(lwi);
    }
    /* ... and remove all yellow and green highlights
       (the yellow ones will be recreated later if needed) */
    QList<QTextEdit::ExtraSelection> es;
    if (ln || spin)
        es.prepend(textEdit->currentLineSelection());
    es.append(textEdit->getBlueSel());
    textEdit->setExtraSelections(es);

    /* at last, set all properties correctly */
    dropTarget->setWinTitle(title);
    dropTarget->ui->tabWidget->setTabToolTip(0, tooltip);
    /* reload buttons, syntax highlighting, jump bar, line numbers */
    dropTarget->encodingToCheck(textEdit->getEncoding());
    if (!textEdit->getFileName().isEmpty())
        dropTarget->ui->actionReload->setEnabled(true);
    if (!hl)
        dropTarget->ui->actionSyntax->setChecked(false);
    else
        dropTarget->syntaxHighlighting(textEdit, true, textEdit->getLang());
    if (spin) {
        dropTarget->ui->spinBox->setVisible(true);
        dropTarget->ui->label->setVisible(true);
        dropTarget->ui->spinBox->setMaximum(textEdit->document()->blockCount());
        connect(textEdit->document(), &QTextDocument::blockCountChanged, dropTarget, &FPwin::setMax);
    }
    if (ln)
        dropTarget->ui->actionLineNumbers->setChecked(true);
    /* searching */
    if (!textEdit->getSearchedText().isEmpty()) {
        connect(textEdit, &QPlainTextEdit::textChanged, dropTarget, &FPwin::hlight);
        connect(textEdit, &TextEdit::updateRect, dropTarget, &FPwin::hlight);
        connect(textEdit, &TextEdit::resized, dropTarget, &FPwin::hlight);
        /* restore yellow highlights, which will automatically
           set the current line highlight if needed because the
           spin button and line number menuitem are set above */
        dropTarget->hlight();
    }
    /* status bar */
    if (status) {
        dropTarget->ui->statusBar->setVisible(true);
        dropTarget->statusMsgWithLineCount(textEdit->document()->blockCount());
        if (textEdit->getWordNumber() == -1) {
            if (QToolButton* wordButton = dropTarget->ui->statusBar->findChild<QToolButton*>("wordButton"))
                wordButton->setVisible(true);
        }
        else {
            if (QToolButton* wordButton = dropTarget->ui->statusBar->findChild<QToolButton*>("wordButton"))
                wordButton->setVisible(false);
            QLabel* statusLabel = dropTarget->ui->statusBar->findChild<QLabel*>("statusLabel");
            statusLabel->setText(
                QString("%1 <i>%2</i>").arg(statusLabel->text(), locale().toString(textEdit->getWordNumber())));
            connect(textEdit->document(), &QTextDocument::contentsChange, dropTarget, &FPwin::updateWordInfo);
        }
        connect(textEdit, &QPlainTextEdit::blockCountChanged, dropTarget, &FPwin::statusMsgWithLineCount);
        connect(textEdit, &TextEdit::selChanged, dropTarget, &FPwin::statusMsg);
        if (statusCurPos) {
            dropTarget->addCursorPosLabel();
            dropTarget->showCursorPos();
            connect(textEdit, &QPlainTextEdit::cursorPositionChanged, dropTarget, &FPwin::showCursorPos);
        }
    }
    if (textEdit->lineWrapMode() == QPlainTextEdit::NoWrap)
        dropTarget->ui->actionWrap->setChecked(false);
    /* auto indentation */
    if (textEdit->getAutoIndentation() == false)
        dropTarget->ui->actionIndent->setChecked(false);
    /* the remaining signals */
    connect(textEdit->document(), &QTextDocument::undoAvailable, dropTarget->ui->actionUndo, &QAction::setEnabled);
    connect(textEdit->document(), &QTextDocument::redoAvailable, dropTarget->ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        connect(textEdit->document(), &QTextDocument::modificationChanged, dropTarget, &FPwin::enableSaving);
    connect(textEdit->document(), &QTextDocument::modificationChanged, dropTarget, &FPwin::asterisk);
    connect(textEdit, &TextEdit::canCopy, dropTarget->ui->actionCopy, &QAction::setEnabled);

    connect(tabPage, &TabPage::find, dropTarget, &FPwin::find);
    connect(tabPage, &TabPage::searchFlagChanged, dropTarget, &FPwin::searchFlagChanged);

    if (!textEdit->isReadOnly()) {
        connect(textEdit, &TextEdit::canCopy, dropTarget->ui->actionCut, &QAction::setEnabled);
        connect(textEdit, &TextEdit::canCopy, dropTarget->ui->actionDelete, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, dropTarget->ui->actionUpperCase, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, dropTarget->ui->actionLowerCase, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, dropTarget->ui->actionStartCase, &QAction::setEnabled);
    }
    connect(textEdit, &TextEdit::filePasted, dropTarget, &FPwin::newTabFromName);
    connect(textEdit, &TextEdit::zoomedOut, dropTarget, &FPwin::reformat);
    connect(textEdit, &TextEdit::hugeColumn, dropTarget, &FPwin::columnWarning);
    connect(textEdit, &QWidget::customContextMenuRequested, dropTarget, &FPwin::editorContextMenu);

    textEdit->setFocus();

    dropTarget->stealFocus();
}
/*************************/
void FPwin::dropTab(const QString& str, QObject* source) {
    QWidget* w = qobject_cast<QWidget*>(source);
    if (w == nullptr || str.isEmpty())  // impossible
    {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }
    int index = str.toInt();
    if (index <= -1)  // impossible
    {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }

    FPwin* dragSource = qobject_cast<FPwin*>(w->window());
    if (dragSource == this || dragSource == nullptr)  // impossible
    {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }

    closeWarningBar();
    dragSource->closeWarningBar();

    TabPage* tabPage = qobject_cast<TabPage*>(dragSource->ui->tabWidget->widget(index));
    if (tabPage == nullptr) {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }
    TextEdit* textEdit = tabPage->textEdit();

    QString tooltip = dragSource->ui->tabWidget->tabToolTip(index);
    QString tabText = dragSource->ui->tabWidget->tabText(index);
    bool spin = false;
    bool ln = false;
    if (dragSource->ui->spinBox->isVisible())
        spin = true;
    if (dragSource->ui->actionLineNumbers->isChecked())
        ln = true;

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();

    disconnect(textEdit, &TextEdit::resized, dragSource, &FPwin::hlight);
    disconnect(textEdit, &TextEdit::updateRect, dragSource, &FPwin::hlight);
    disconnect(textEdit, &QPlainTextEdit::textChanged, dragSource, &FPwin::hlight);
    if (dragSource->ui->statusBar->isVisible()) {
        disconnect(textEdit, &QPlainTextEdit::blockCountChanged, dragSource, &FPwin::statusMsgWithLineCount);
        disconnect(textEdit, &TextEdit::selChanged, dragSource, &FPwin::statusMsg);
        if (dragSource->ui->statusBar->findChild<QLabel*>("posLabel"))
            disconnect(textEdit, &QPlainTextEdit::cursorPositionChanged, dragSource, &FPwin::showCursorPos);
    }
    disconnect(textEdit, &TextEdit::canCopy, dragSource->ui->actionCut, &QAction::setEnabled);
    disconnect(textEdit, &TextEdit::canCopy, dragSource->ui->actionDelete, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, dragSource->ui->actionUpperCase, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, dragSource->ui->actionLowerCase, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, dragSource->ui->actionStartCase, &QAction::setEnabled);
    disconnect(textEdit, &TextEdit::canCopy, dragSource->ui->actionCopy, &QAction::setEnabled);
    disconnect(textEdit, &QWidget::customContextMenuRequested, dragSource, &FPwin::editorContextMenu);
    disconnect(textEdit, &TextEdit::zoomedOut, dragSource, &FPwin::reformat);
    disconnect(textEdit, &TextEdit::hugeColumn, dragSource, &FPwin::columnWarning);
    disconnect(textEdit, &TextEdit::filePasted, dragSource, &FPwin::newTabFromName);
    disconnect(textEdit, &TextEdit::updateBracketMatching, dragSource, &FPwin::matchBrackets);
    disconnect(textEdit, &QPlainTextEdit::blockCountChanged, dragSource, &FPwin::formatOnBlockChange);
    disconnect(textEdit, &TextEdit::updateRect, dragSource, &FPwin::formatTextRect);
    disconnect(textEdit, &TextEdit::resized, dragSource, &FPwin::formatTextRect);

    disconnect(textEdit->document(), &QTextDocument::contentsChange, dragSource, &FPwin::updateWordInfo);
    disconnect(textEdit->document(), &QTextDocument::contentsChange, dragSource, &FPwin::formatOnTextChange);
    disconnect(textEdit->document(), &QTextDocument::blockCountChanged, dragSource, &FPwin::setMax);
    disconnect(textEdit->document(), &QTextDocument::modificationChanged, dragSource, &FPwin::asterisk);
    disconnect(textEdit->document(), &QTextDocument::undoAvailable, dragSource->ui->actionUndo, &QAction::setEnabled);
    disconnect(textEdit->document(), &QTextDocument::redoAvailable, dragSource->ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        disconnect(textEdit->document(), &QTextDocument::modificationChanged, dragSource, &FPwin::enableSaving);

    disconnect(tabPage, &TabPage::find, dragSource, &FPwin::find);
    disconnect(tabPage, &TabPage::searchFlagChanged, dragSource, &FPwin::searchFlagChanged);

    /* it's important to release mouse before tab removal because otherwise, the source
       tabbar might not be updated properly with tab reordering during a fast drag-and-drop */
    dragSource->ui->tabWidget->tabBar()->releaseMouse();

    dragSource->ui->tabWidget->removeTab(index);  // there can't be a side-pane here
    int count = dragSource->ui->tabWidget->count();
    if (count == 1)
        dragSource->updateGUIForSingleTab(true);

    /***************************************************************************
     ***** The tab is dropped into this window; so insert it as a new tab. *****
     ***************************************************************************/

    int insertIndex = ui->tabWidget->currentIndex() + 1;

    /* first, set the new info... */
    lastFile_ = textEdit->getFileName();
    textEdit->setGreenSel(QList<QTextEdit::ExtraSelection>());
    textEdit->setRedSel(QList<QTextEdit::ExtraSelection>());
    /* ... then insert the detached widget,
       considering whether the searchbar should be shown... */
    if (!textEdit->getSearchedText().isEmpty()) {
        if (insertIndex == 0  // the window has no tab yet
            || !qobject_cast<TabPage*>(ui->tabWidget->widget(insertIndex - 1))->isSearchBarVisible()) {
            for (int i = 0; i < ui->tabWidget->count(); ++i)
                qobject_cast<TabPage*>(ui->tabWidget->widget(i))->setSearchBarVisible(true);
        }
    }
    else if (insertIndex > 0) {
        tabPage->setSearchBarVisible(
            qobject_cast<TabPage*>(ui->tabWidget->widget(insertIndex - 1))->isSearchBarVisible());
    }
    if (ui->tabWidget->count() == 0)  // the tab will be inserted and switched to below
        enableWidgets(true);
    else if (ui->tabWidget->count() == 1)
        updateGUIForSingleTab(false);  // tab detach and switch actions
    QFileInfo lastFileInfo(lastFile_);
    bool isLink = lastFile_.isEmpty() ? false : lastFileInfo.isSymLink();
    bool hasFinalTarget(false);
    if (!isLink) {
        const QString finalTarget = lastFileInfo.canonicalFilePath();
        hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != lastFile_);
    }
    ui->tabWidget->insertTab(insertIndex, tabPage,
                             isLink           ? QIcon(":icons/link.svg")
                             : hasFinalTarget ? QIcon(":icons/hasTarget.svg")
                                              : QIcon(),
                             tabText);
    if (sidePane_) {
        ListWidget* lw = sidePane_->listWidget();
        QString fname = textEdit->getFileName();
        if (fname.isEmpty()) {
            if (textEdit->getProg() == "help")
                fname = "** " + tr("Help") + " **";
            else
                fname = tr("Untitled");
        }
        else
            fname = fname.section('/', -1);
        if (textEdit->document()->isModified())
            fname.append("*");
        fname.replace("\n", " ");
        ListWidgetItem* lwi = new ListWidgetItem(isLink           ? QIcon(":icons/link.svg")
                                                 : hasFinalTarget ? QIcon(":icons/hasTarget.svg")
                                                                  : QIcon(),
                                                 fname, lw);
        lw->setToolTip(tooltip);
        sideItems_.insert(lwi, tabPage);
        lw->addItem(lwi);
        lw->setCurrentItem(lwi);
    }
    ui->tabWidget->setCurrentIndex(insertIndex);
    /* ... and remove all yellow and green highlights
       (the yellow ones will be recreated later if needed) */
    QList<QTextEdit::ExtraSelection> es;
    if ((ln || spin) && (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())) {
        es.prepend(textEdit->currentLineSelection());
    }
    es.append(textEdit->getBlueSel());
    textEdit->setExtraSelections(es);

    /* at last, set all properties correctly */
    ui->tabWidget->setTabToolTip(insertIndex, tooltip);
    /* reload buttons, syntax highlighting, jump bar, line numbers */
    if (ui->actionSyntax->isChecked()) {
        makeBusy();  // it may take a while with huge texts
        syntaxHighlighting(textEdit, true, textEdit->getLang());
        QTimer::singleShot(0, this, &FPwin::unbusy);
    }
    else if (!ui->actionSyntax->isChecked() &&
             textEdit->getHighlighter()) {  // there's no connction to the drag target yet
        textEdit->setDrawIndetLines(false);
        Highlighter* highlighter = qobject_cast<Highlighter*>(textEdit->getHighlighter());
        delete highlighter;
        highlighter = nullptr;
    }
    if (ui->spinBox->isVisible())
        connect(textEdit->document(), &QTextDocument::blockCountChanged, this, &FPwin::setMax);
    if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
        textEdit->showLineNumbers(true);
    else
        textEdit->showLineNumbers(false);
    /* searching */
    if (!textEdit->getSearchedText().isEmpty()) {
        connect(textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
        connect(textEdit, &TextEdit::updateRect, this, &FPwin::hlight);
        connect(textEdit, &TextEdit::resized, this, &FPwin::hlight);
        /* restore yellow highlights, which will automatically
           set the current line highlight if needed because the
           spin button and line number menuitem are set above */
        hlight();
    }
    /* status bar */
    if (ui->statusBar->isVisible()) {
        connect(textEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
        connect(textEdit, &TextEdit::selChanged, this, &FPwin::statusMsg);
        if (ui->statusBar->findChild<QLabel*>("posLabel")) {
            showCursorPos();
            connect(textEdit, &QPlainTextEdit::cursorPositionChanged, this, &FPwin::showCursorPos);
        }
        if (textEdit->getWordNumber() != -1)
            connect(textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
    }
    if (ui->actionWrap->isChecked() && textEdit->lineWrapMode() == QPlainTextEdit::NoWrap)
        textEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    else if (!ui->actionWrap->isChecked() && textEdit->lineWrapMode() == QPlainTextEdit::WidgetWidth)
        textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    /* auto indentation */
    if (ui->actionIndent->isChecked() && textEdit->getAutoIndentation() == false)
        textEdit->setAutoIndentation(true);
    else if (!ui->actionIndent->isChecked() && textEdit->getAutoIndentation() == true)
        textEdit->setAutoIndentation(false);
    /* the remaining signals */
    connect(textEdit->document(), &QTextDocument::undoAvailable, ui->actionUndo, &QAction::setEnabled);
    connect(textEdit->document(), &QTextDocument::redoAvailable, ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        connect(textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::enableSaving);
    connect(textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::asterisk);
    connect(textEdit, &TextEdit::canCopy, ui->actionCopy, &QAction::setEnabled);

    connect(tabPage, &TabPage::find, this, &FPwin::find);
    connect(tabPage, &TabPage::searchFlagChanged, this, &FPwin::searchFlagChanged);

    if (!textEdit->isReadOnly()) {
        connect(textEdit, &TextEdit::canCopy, ui->actionCut, &QAction::setEnabled);
        connect(textEdit, &TextEdit::canCopy, ui->actionDelete, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionUpperCase, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionLowerCase, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionStartCase, &QAction::setEnabled);
    }
    connect(textEdit, &TextEdit::filePasted, this, &FPwin::newTabFromName);
    connect(textEdit, &TextEdit::zoomedOut, this, &FPwin::reformat);
    connect(textEdit, &TextEdit::hugeColumn, this, &FPwin::columnWarning);
    connect(textEdit, &QWidget::customContextMenuRequested, this, &FPwin::editorContextMenu);

    textEdit->setFocus();

    stealFocus();

    if (count == 0)
        QTimer::singleShot(0, dragSource, &QWidget::close);
}
/*************************/
void FPwin::tabContextMenu(const QPoint& p) {
    auto mbt = qobject_cast<MenuBarTitle*>(QObject::sender());
    rightClicked_ = mbt == nullptr ? ui->tabWidget->tabBar()->tabAt(p) : ui->tabWidget->currentIndex();
    if (rightClicked_ < 0)
        return;

    QString fname = qobject_cast<TabPage*>(ui->tabWidget->widget(rightClicked_))->textEdit()->getFileName();
    QMenu menu(this);  // "this" is for Wayland, when the window isn't active
    bool showMenu = false;
    if (mbt == nullptr) {
        int tabNum = ui->tabWidget->count();
        if (tabNum > 1) {
            QWidgetAction* labelAction = new QWidgetAction(&menu);
            QLabel* label = new QLabel("<center><b>" + tr("%1 Pages").arg(tabNum) + "</b></center>");
            label->setMargin(4);
            labelAction->setDefaultWidget(label);
            menu.addAction(labelAction);
            menu.addSeparator();

            showMenu = true;
            if (rightClicked_ < tabNum - 1)
                menu.addAction(ui->actionCloseRight);
            if (rightClicked_ > 0)
                menu.addAction(ui->actionCloseLeft);
            menu.addSeparator();
            if (rightClicked_ < tabNum - 1 && rightClicked_ > 0)
                menu.addAction(ui->actionCloseOther);
            menu.addAction(ui->actionCloseAll);
            if (!fname.isEmpty())
                menu.addSeparator();
        }
    }
    if (!fname.isEmpty()) {
        showMenu = true;
        menu.addAction(ui->actionCopyName);
        menu.addAction(ui->actionCopyPath);
        QFileInfo info(fname);
        const QString finalTarget = info.canonicalFilePath();
        bool hasFinalTarget = false;
        if (info.isSymLink()) {
            menu.addSeparator();
            const QString symTarget = info.symLinkTarget();
            hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != symTarget);
            QAction* action = menu.addAction(QIcon(":icons/link.svg"), tr("Copy Target Path"));
            connect(action, &QAction::triggered, [symTarget] { QApplication::clipboard()->setText(symTarget); });
            action = menu.addAction(QIcon(":icons/link.svg"), tr("Open Target Here"));
            connect(action, &QAction::triggered, this, [this, symTarget] {
                for (int i = 0; i < ui->tabWidget->count(); ++i) {
                    TabPage* thisTabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
                    if (symTarget == thisTabPage->textEdit()->getFileName()) {
                        ui->tabWidget->setCurrentWidget(thisTabPage);
                        return;
                    }
                }
                newTabFromName(symTarget, 0, 0);
            });
        }
        else
            hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != fname);
        if (hasFinalTarget) {
            menu.addSeparator();
            QAction* action = menu.addAction(QIcon(":icons/hasTarget.svg"), tr("Copy Final Target Path"));
            connect(action, &QAction::triggered, [finalTarget] { QApplication::clipboard()->setText(finalTarget); });
            action = menu.addAction(QIcon(":icons/hasTarget.svg"), tr("Open Final Target Here"));
            connect(action, &QAction::triggered, this, [this, finalTarget] {
                for (int i = 0; i < ui->tabWidget->count(); ++i) {
                    TabPage* thisTabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
                    if (finalTarget == thisTabPage->textEdit()->getFileName()) {
                        ui->tabWidget->setCurrentWidget(thisTabPage);
                        return;
                    }
                }
                newTabFromName(finalTarget, 0, 0);
            });
        }
        if (!static_cast<FPsingleton*>(qApp)->isRoot() && QFile::exists(fname)) {
            menu.addSeparator();
            QAction* action = menu.addAction(static_cast<FPsingleton*>(qApp)->getConfig().getSysIcons()
                                                 ? QIcon::fromTheme("folder")
                                                 : symbolicIcon::icon(":icons/document-open.svg"),
                                             tr("Open Containing Folder"));
            connect(action, &QAction::triggered, this, [fname] {
                QDBusMessage methodCall = QDBusMessage::createMethodCall(
                    QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
                    QString(), QStringLiteral("ShowItems"));
                /* NOTE: The removal of the auto-start flag is needed for switching to
                         URL opening if "org.freedesktop.FileManager1" doesn't exist. */
                methodCall.setAutoStartService(false);
                QList<QVariant> args;
                args.append(QStringList() << fname);
                args.append("0");
                methodCall.setArguments(args);
                QDBusMessage response = QDBusConnection::sessionBus().call(methodCall, QDBus::Block, 1000);
                if (response.type() == QDBusMessage::ErrorMessage) {
                    QString folder = fname.section("/", 0, -2);
                    if (QStandardPaths::findExecutable("gio").isEmpty() ||
                        !QProcess::startDetached("gio", QStringList() << "open" << folder)) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
                    }
                }
            });
        }
    }
    if (showMenu)  // we don't want an empty menu
    {
        if (mbt == nullptr)
            menu.exec(ui->tabWidget->tabBar()->mapToGlobal(p));
        else
            menu.exec(mbt->mapToGlobal(p));
    }
    rightClicked_ = -1;  // reset
}
/*************************/
void FPwin::listContextMenu(const QPoint& p) {
    if (!sidePane_ || sideItems_.isEmpty() || locked_)
        return;

    ListWidget* lw = sidePane_->listWidget();
    QModelIndex index = lw->indexAt(p);
    if (!index.isValid())
        return;
    QListWidgetItem* item = lw->getItemFromIndex(index);
    rightClicked_ = lw->row(item);
    QString fname = sideItems_.value(item)->textEdit()->getFileName();

    QMenu menu(this);  // "this" is for Wayland, when the window isn't active
    menu.addAction(ui->actionClose);
    if (lw->count() > 1) {
        QWidgetAction* labelAction = new QWidgetAction(&menu);
        QLabel* label = new QLabel("<center><b>" + tr("%1 Pages").arg(lw->count()) + "</b></center>");
        label->setMargin(4);
        labelAction->setDefaultWidget(label);
        menu.insertAction(ui->actionClose, labelAction);
        menu.insertSeparator(ui->actionClose);

        menu.addSeparator();
        if (rightClicked_ < lw->count() - 1)
            menu.addAction(ui->actionCloseRight);
        if (rightClicked_ > 0)
            menu.addAction(ui->actionCloseLeft);
        if (rightClicked_ < lw->count() - 1 && rightClicked_ > 0) {
            menu.addSeparator();
            menu.addAction(ui->actionCloseOther);
        }
        menu.addAction(ui->actionCloseAll);
        if (!static_cast<FPsingleton*>(qApp)->isStandAlone()) {
            menu.addSeparator();
            menu.addAction(ui->actionDetachTab);
        }
    }
    if (!fname.isEmpty()) {
        menu.addSeparator();
        menu.addAction(ui->actionCopyName);
        menu.addAction(ui->actionCopyPath);
        QFileInfo info(fname);
        const QString finalTarget = info.canonicalFilePath();
        bool hasFinalTarget = false;
        if (info.isSymLink()) {
            menu.addSeparator();
            const QString symTarget = info.symLinkTarget();
            hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != symTarget);
            QAction* action = menu.addAction(QIcon(":icons/link.svg"), tr("Copy Target Path"));
            connect(action, &QAction::triggered, [symTarget] { QApplication::clipboard()->setText(symTarget); });
            action = menu.addAction(QIcon(":icons/link.svg"), tr("Open Target Here"));
            connect(action, &QAction::triggered, this, [this, symTarget] {
                for (int i = 0; i < ui->tabWidget->count(); ++i) {
                    TabPage* thisTabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
                    if (symTarget == thisTabPage->textEdit()->getFileName()) {
                        if (QListWidgetItem* wi = sideItems_.key(thisTabPage))
                            sidePane_->listWidget()->setCurrentItem(wi);  // sets the current widget at changeTab()
                        return;
                    }
                }
                newTabFromName(symTarget, 0, 0);
            });
        }
        else
            hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != fname);
        if (hasFinalTarget) {
            menu.addSeparator();
            QAction* action = menu.addAction(QIcon(":icons/hasTarget.svg"), tr("Copy Final Target Path"));
            connect(action, &QAction::triggered, [finalTarget] { QApplication::clipboard()->setText(finalTarget); });
            action = menu.addAction(QIcon(":icons/hasTarget.svg"), tr("Open Final Target Here"));
            connect(action, &QAction::triggered, this, [this, finalTarget] {
                for (int i = 0; i < ui->tabWidget->count(); ++i) {
                    TabPage* thisTabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
                    if (finalTarget == thisTabPage->textEdit()->getFileName()) {
                        if (QListWidgetItem* wi = sideItems_.key(thisTabPage))
                            sidePane_->listWidget()->setCurrentItem(wi);  // sets the current widget at changeTab()
                        return;
                    }
                }
                newTabFromName(finalTarget, 0, 0);
            });
        }
        if (!static_cast<FPsingleton*>(qApp)->isRoot() && QFile::exists(fname)) {
            menu.addSeparator();
            QAction* action = menu.addAction(static_cast<FPsingleton*>(qApp)->getConfig().getSysIcons()
                                                 ? QIcon::fromTheme("folder")
                                                 : symbolicIcon::icon(":icons/document-open.svg"),
                                             tr("Open Containing Folder"));
            connect(action, &QAction::triggered, this, [fname] {
                QDBusMessage methodCall = QDBusMessage::createMethodCall(
                    QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
                    QString(), QStringLiteral("ShowItems"));
                methodCall.setAutoStartService(false);
                QList<QVariant> args;
                args.append(QStringList() << fname);
                args.append("0");
                methodCall.setArguments(args);
                QDBusMessage response = QDBusConnection::sessionBus().call(methodCall, QDBus::Block, 1000);
                if (response.type() == QDBusMessage::ErrorMessage) {
                    QString folder = fname.section("/", 0, -2);
                    if (QStandardPaths::findExecutable("gio").isEmpty() ||
                        !QProcess::startDetached("gio", QStringList() << "open" << folder)) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
                    }
                }
            });
        }
    }
    menu.exec(lw->viewport()->mapToGlobal(p));
    rightClicked_ = -1;  // reset
}
/*************************/
void FPwin::prefDialog() {
    if (isLoading())
        return;
    if (hasAnotherDialog())
        return;

    updateShortcuts(true);
    PrefDialog dlg(this);
    /*dlg.show();
    move (x() + width()/2 - dlg.width()/2,
          y() + height()/2 - dlg.height()/ 2);*/
    dlg.exec();
    updateShortcuts(false);
}
/*************************/
static inline void moveToWordStart(QTextCursor& cur, bool forward) {
    const QString blockText = cur.block().text();
    const int l = blockText.length();
    int indx = cur.positionInBlock();
    if (indx < l) {
        QChar ch = blockText.at(indx);
        while (!ch.isLetterOrNumber() && ch != '\'' && ch != '-' && ch != QChar(QChar::Nbsp) && ch != QChar(0x200C)) {
            cur.movePosition(QTextCursor::NextCharacter);
            ++indx;
            if (indx == l) {
                if (cur.movePosition(QTextCursor::NextBlock))
                    moveToWordStart(cur, forward);
                return;
            }
            ch = blockText.at(indx);
        }
    }
    if (!forward && indx > 0) {
        QChar ch = blockText.at(indx - 1);
        while (ch.isLetterOrNumber() || ch == '\'' || ch == '-' || ch == QChar(QChar::Nbsp) || ch == QChar(0x200C)) {
            cur.movePosition(QTextCursor::PreviousCharacter);
            --indx;
            ch = blockText.at(indx);
            if (indx == 0)
                break;
        }
    }
}

static inline void selectWord(QTextCursor& cur) {
    moveToWordStart(cur, true);
    const QString blockText = cur.block().text();
    const int l = blockText.length();
    int indx = cur.positionInBlock();
    if (indx < l) {
        QChar ch = blockText.at(indx);
        while (ch.isLetterOrNumber() || ch == '\'' || ch == '-' || ch == QChar(QChar::Nbsp) || ch == QChar(0x200C)) {
            cur.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            ++indx;
            if (indx == l)
                break;
            ch = blockText.at(indx);
        }
    }

    /* no dash, single quote mark or number at the start */
    while (!cur.selectedText().isEmpty() && (cur.selectedText().at(0) == '-' || cur.selectedText().at(0) == '\'' ||
                                             cur.selectedText().at(0).isNumber())) {
        int p = cur.position();
        cur.setPosition(cur.anchor() + 1);
        cur.setPosition(p, QTextCursor::KeepAnchor);
    }
    /* no dash or single quote mark at the end */
    while (!cur.selectedText().isEmpty() && (cur.selectedText().endsWith("-") || cur.selectedText().endsWith("\'"))) {
        cur.setPosition(cur.position() - 1, QTextCursor::KeepAnchor);
    }
}

void FPwin::checkSpelling() {
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr)
        return;
    if (isLoading())
        return;
    if (hasAnotherDialog())
        return;

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    auto dictPath = config.getDictPath();
    if (dictPath.isEmpty()) {
        showWarningBar("<center><b><big>" + tr("You need to add a Hunspell dictionary.") + "</big></b></center>" +
                           "<center><i>" + tr("See Preferences → Text → Spell Checking!") + "</i></center>",
                       20);
        return;
    }
    if (!QFile::exists(dictPath)) {
        showWarningBar("<center><b><big>" + tr("The Hunspell dictionary does not exist.") + "</big></b></center>" +
                           "<center><i>" + tr("See Preferences → Text → Spell Checking!") + "</i></center>",
                       20);
        return;
    }
    if (dictPath.endsWith(".dic"))
        dictPath = dictPath.left(dictPath.size() - 4);
    const QString affixFile = dictPath + ".aff";
    if (!QFile::exists(affixFile)) {
        showWarningBar("<center><b><big>" + tr("The Hunspell dictionary is not accompanied by an affix file.") +
                           "</big></b></center>" + "<center><i>" + tr("See Preferences → Text → Spell Checking!") +
                           "</i></center>",
                       20);
        return;
    }
    QString confPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (!QFile(confPath + "/featherpad").exists())  // create config dir if needed
        QDir(confPath).mkpath(confPath + "/featherpad");
    QString userDict = confPath + "/featherpad/userDict-" + dictPath.section("/", -1);

    TextEdit* textEdit = tabPage->textEdit();
    QTextCursor cur = textEdit->textCursor();
    if (config.getSpellCheckFromStart())
        cur.movePosition(QTextCursor::Start);
    cur.setPosition(cur.anchor());
    moveToWordStart(cur, false);
    selectWord(cur);
    QString word = cur.selectedText();
    while (word.isEmpty()) {
        if (!cur.movePosition(QTextCursor::NextCharacter)) {
            if (config.getSpellCheckFromStart())
                showWarningBar("<center><b><big>" + tr("No misspelling in document.") + "</big></b></center>");
            else
                showWarningBar("<center><b><big>" + tr("No misspelling from text cursor.") + "</big></b></center>");
            return;
        }
        selectWord(cur);
        word = cur.selectedText();
    }

    SpellChecker* spellChecker = new SpellChecker(dictPath, userDict);

    while (spellChecker->spell(word)) {
        cur.setPosition(cur.position());
        if (cur.atEnd()) {
            delete spellChecker;
            if (config.getSpellCheckFromStart())
                showWarningBar("<center><b><big>" + tr("No misspelling in document.") + "</big></b></center>");
            else
                showWarningBar("<center><b><big>" + tr("No misspelling from text cursor.") + "</big></b></center>");
            return;
        }
        if (cur.movePosition(QTextCursor::NextCharacter))
            selectWord(cur);
        word = cur.selectedText();
        while (word.isEmpty()) {
            cur.setPosition(cur.anchor());
            if (!cur.movePosition(QTextCursor::NextCharacter)) {
                delete spellChecker;
                if (config.getSpellCheckFromStart())
                    showWarningBar("<center><b><big>" + tr("No misspelling in document.") + "</big></b></center>");
                else
                    showWarningBar("<center><b><big>" + tr("No misspelling from text cursor.") + "</big></b></center>");
                return;
            }
            selectWord(cur);
            word = cur.selectedText();
        }
    }
    textEdit->skipSelectionHighlighting();
    textEdit->setTextCursor(cur);
    textEdit->ensureCursorVisible();

    updateShortcuts(true);
    SpellDialog dlg(spellChecker, word,
                    /* disable the correcting buttons if the text isn't editable */
                    !textEdit->isReadOnly() && !textEdit->isUneditable(), this);
    dlg.setWindowTitle(tr("Spell Checking"));

    connect(&dlg, &SpellDialog::spellChecked, [&dlg, textEdit](int res) {
        bool uneditable = textEdit->isReadOnly() || textEdit->isUneditable();
        QTextCursor cur = textEdit->textCursor();
        if (!cur.hasSelection())
            return;  // impossible
        QString word = cur.selectedText();
        QString corrected;
        switch (res) {
            case SpellDialog::CorrectOnce:
                if (!uneditable)
                    cur.insertText(dlg.replacement());
                break;
            case SpellDialog::IgnoreOnce:
                break;
            case SpellDialog::CorrectAll:
                /* remember this corretion */
                dlg.spellChecker()->addToCorrections(word, dlg.replacement());
                if (!uneditable)
                    cur.insertText(dlg.replacement());
                break;
            case SpellDialog::IgnoreAll:
                /* always ignore the selected word */
                dlg.spellChecker()->ignoreWord(word);
                break;
            case SpellDialog::AddToDict:
                /* not only ignore it but also add it to user dictionary */
                dlg.spellChecker()->addToUserWordlist(word);
                break;
        }

        /* check the next word */
        cur.setPosition(cur.position());
        if (cur.atEnd()) {
            textEdit->skipSelectionHighlighting();
            textEdit->setTextCursor(cur);
            textEdit->ensureCursorVisible();
            dlg.close();
            return;
        }
        if (cur.movePosition(QTextCursor::NextCharacter))
            selectWord(cur);
        word = cur.selectedText();

        while (word.isEmpty()) {
            cur.setPosition(cur.anchor());
            if (!cur.movePosition(QTextCursor::NextCharacter)) {
                textEdit->skipSelectionHighlighting();
                textEdit->setTextCursor(cur);
                textEdit->ensureCursorVisible();
                dlg.close();
                return;
            }
            selectWord(cur);
            word = cur.selectedText();
        }
        while (dlg.spellChecker()->spell(word) || !(corrected = dlg.spellChecker()->correct(word)).isEmpty()) {
            if (!corrected.isEmpty()) {
                if (!uneditable)
                    cur.insertText(corrected);
                corrected = QString();
            }
            else
                cur.setPosition(cur.position());
            if (cur.atEnd()) {
                textEdit->skipSelectionHighlighting();
                textEdit->setTextCursor(cur);
                textEdit->ensureCursorVisible();
                dlg.close();
                return;
            }
            if (cur.movePosition(QTextCursor::NextCharacter))
                selectWord(cur);
            word = cur.selectedText();
            while (word.isEmpty()) {
                cur.setPosition(cur.anchor());
                if (!cur.movePosition(QTextCursor::NextCharacter)) {
                    textEdit->skipSelectionHighlighting();
                    textEdit->setTextCursor(cur);
                    textEdit->ensureCursorVisible();
                    dlg.close();
                    return;
                }
                selectWord(cur);
                word = cur.selectedText();
            }
        }
        textEdit->skipSelectionHighlighting();
        textEdit->setTextCursor(cur);
        textEdit->ensureCursorVisible();
        dlg.checkWord(word);
    });

    dlg.exec();
    updateShortcuts(false);

    delete spellChecker;
}
/*************************/
void FPwin::userDict() {
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    QString dictPath = config.getDictPath();
    if (dictPath.isEmpty())
        showWarningBar("<center><b><big>" + tr("The file does not exist.") + "</big></b></center>");
    else {
        if (dictPath.endsWith(".dic"))
            dictPath = dictPath.left(dictPath.size() - 4);
        QString confPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        QString userDict = confPath + "/featherpad/userDict-" + dictPath.section("/", -1);
        newTabFromName(userDict, 0, 0);
    }
}
/*************************/
void FPwin::manageSessions() {
    if (!isReady())
        return;

    /* first see whether the Sessions dialog is already open... */
    FPsingleton* singleton = static_cast<FPsingleton*>(qApp);
    for (int i = 0; i < singleton->Wins.count(); ++i) {
        const auto dialogs = singleton->Wins.at(i)->findChildren<QDialog*>();
        for (const auto& dialog : dialogs) {
            if (dialog->objectName() == "sessionDialog") {
                stealFocus(dialog);
                return;
            }
        }
    }
    /* ... and if not, create a non-modal Sessions dialog */
    SessionDialog* dlg = new SessionDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    /*move (x() + width()/2 - dlg.width()/2,
          y() + height()/2 - dlg.height()/ 2);*/
    dlg->raise();
    dlg->activateWindow();
}
/*************************/
// Pauses or resumes auto-saving.
void FPwin::pauseAutoSaving(bool pause) {
    if (!autoSaver_)
        return;
    if (pause) {
        if (!autoSaverPause_.isValid())  // don't start it again
        {
            autoSaverPause_.start();
            autoSaverRemainingTime_ = autoSaver_->remainingTime();
        }
    }
    else if (!locked_ && autoSaverPause_.isValid()) {
        if (autoSaverPause_.hasExpired(autoSaverRemainingTime_)) {
            autoSaverPause_.invalidate();
            autoSave();
        }
        else
            autoSaverPause_.invalidate();
    }
}
/*************************/
void FPwin::startAutoSaving(bool start, int interval) {
    if (start) {
        if (!autoSaver_) {
            autoSaver_ = new QTimer(this);
            connect(autoSaver_, &QTimer::timeout, this, &FPwin::autoSave);
        }
        autoSaver_->setInterval(interval * 1000 * 60);
        autoSaver_->start();
    }
    else if (autoSaver_) {
        if (autoSaver_->isActive())
            autoSaver_->stop();
        delete autoSaver_;
        autoSaver_ = nullptr;
    }
}
/*************************/
void FPwin::autoSave() {
    /* since there are important differences between this
       and saveFile(), we can't use the latter here.
       We especially don't show any prompt or warning here. */
    if (autoSaverPause_.isValid())
        return;
    QTimer::singleShot(0, this, [=]() {
        if (!autoSaver_ || !autoSaver_->isActive())
            return;
        saveAllFiles(false);  // without warning
    });
}
/*************************/
void FPwin::saveAllFiles(bool showWarning) {
    const int currentIndex = ui->tabWidget->currentIndex();
    if (currentIndex == -1)
        return;

    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    const bool removeTrailing = config.getRemoveTrailingSpaces();
    const bool appendEmpty = config.getAppendEmptyLine();

    bool error = false;
    const int n = ui->tabWidget->count();

    for (int i = 0; i < n; ++i) {
        auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
        TextEdit* te = tabPage->textEdit();
        QTextDocument* doc = te->document();

        if (te->isUneditable() || !doc->isModified())
            continue;

        const QString fname = te->getFileName();
        if (fname.isEmpty() || !QFile::exists(fname))
            continue;

        // optional whitespace cleanup
        if (removeTrailing && te->getProg() != QLatin1String("diff") &&
            QFileInfo(fname).fileName() != QLatin1String("locale.gen")) {
            makeBusy();
            const QString prog = te->getProg();
            const bool doubleSpace = (prog == QLatin1String("markdown") || prog == QLatin1String("fountain"));
            const bool singleSpace = (prog == QLatin1String("LaTeX"));

            QTextBlock block = doc->firstBlock();
            QTextCursor cur(doc);
            cur.beginEditBlock();
            while (block.isValid()) {
                const QString bt = block.text();
                if (const int num = trailingSpaces(bt)) {
                    cur.setPosition(block.position() + bt.size());
                    if (doubleSpace) {
                        if (num != 2)
                            cur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor,
                                             std::max(1, num - 2));
                    }
                    else if (singleSpace) {
                        if (num > 1)
                            cur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, num - 1);
                    }
                    else {
                        cur.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, num);
                    }
                    cur.removeSelectedText();
                }
                block = block.next();
            }
            cur.endEditBlock();
            unbusy();
        }

        // ensure file ends with a newline if requested
        if (appendEmpty && !doc->lastBlock().text().isEmpty()) {
            QTextCursor c(doc);
            c.beginEditBlock();
            c.movePosition(QTextCursor::End);
            c.insertBlock();
            c.endEditBlock();
        }

        QTextDocumentWriter writer(fname, "plaintext");
        if (writer.write(doc)) {
            inactiveTabModified_ = (i != currentIndex);
            doc->setModified(false);

            const QFileInfo fInfo(fname);
            te->setSize(fInfo.size());
            te->setLastModified(fInfo.lastModified());
            setTitle(fname, inactiveTabModified_ ? i : -1);
            addRecentFile(fname);  // saving also implies recently opened

            // update syntax if language changed by saving
            const QString prevLang = te->getProg();
            setProgLang(te);
            const QString newLang = te->getProg();

            if (prevLang != newLang) {
                if (config.getShowLangSelector() && config.getSyntaxByDefault()) {
                    if (te->getLang() == newLang)
                        te->setLang(QString());  // not enforced when it matches detected syntax
                    if (!inactiveTabModified_)
                        updateLangBtn(te);
                }

                if (!inactiveTabModified_ && ui->statusBar->isVisible() && te->getWordNumber() != -1)
                    disconnect(doc, &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);

                if (te->getLang().isEmpty()) {  // restart highlighter only when not forced
                    syntaxHighlighting(te, false);
                    if (ui->actionSyntax->isChecked())
                        syntaxHighlighting(te);
                }

                // update only the syntax segment of the status label
                if (!inactiveTabModified_ && ui->statusBar->isVisible()) {
                    QLabel* statusLabel = ui->statusBar->findChild<QLabel*>("statusLabel");
                    QString str = statusLabel->text();
                    const QString syntaxKey = tr("Syntax");
                    int iSyntax = str.indexOf(syntaxKey);

                    if (iSyntax == -1) {
                        const QString linesTag = QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1").arg(tr("Lines"));
                        const int j = str.indexOf(linesTag);
                        const QString insert =
                            QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1:</b> <i>%2</i>").arg(tr("Syntax"), newLang);
                        if (j >= 0)
                            str.insert(j, insert);
                    }
                    else {
                        if (newLang == QLatin1String("url")) {
                            const QString syntaxTag = QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1").arg(tr("Syntax"));
                            const QString linesTag = QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1").arg(tr("Lines"));
                            const int j = str.indexOf(syntaxTag);
                            const int k = str.indexOf(linesTag);
                            if (j >= 0 && k > j)
                                str.remove(j, k - j);
                        }
                        else {
                            const QString linesEnd = QStringLiteral("</i>&nbsp;&nbsp;&nbsp;<b>%1").arg(tr("Lines"));
                            const int j = str.indexOf(linesEnd);
                            const int offset = syntaxKey.size() + 9;  // size of ":</b> <i>"
                            if (j > iSyntax + offset)
                                str.replace(iSyntax + offset, j - iSyntax - offset, newLang);
                        }
                    }
                    statusLabel->setText(str);
                    if (te->getWordNumber() != -1)
                        connect(doc, &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
                }
            }

            inactiveTabModified_ = false;
        }
        else {
            error = true;
        }
    }

    if (showWarning && error)
        showWarningBar(QStringLiteral("<center><b><big>%1</big></b></center>").arg(tr("Some files cannot be saved!")));
}
/*************************/
void FPwin::aboutDialog() {
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
    dialog.setMainIcon(QIcon::fromTheme("featherpad", QIcon(":icons/featherpad.svg")));
    dialog.settMainTitle(QString("<center><b><big>%1 %2</big></b></center><br>")
                             .arg(qApp->applicationName(), qApp->applicationVersion()));
    dialog.setMainText("<center> " + tr("A lightweight, tabbed, plain-text editor") + " </center>\n<center> " +
                       tr("based on Qt") + " </center><br><center> " + tr("Author") +
                       ": <a href='mailto:share@1g4.org?Subject=My%20Subject'>jopamo</a> </center><p></p>");
    dialog.setWindowTitle(tr("About FeatherPad"));
    dialog.setWindowModality(Qt::WindowModal);
    dialog.exec();
    updateShortcuts(false);
}
/*************************/
void FPwin::helpDoc() {
    int index = ui->tabWidget->currentIndex();
    if (index == -1)
        newTab();
    else {
        for (int i = 0; i < ui->tabWidget->count(); ++i) {
            TabPage* thisTabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
            TextEdit* thisTextEdit = thisTabPage->textEdit();
            if (thisTextEdit->getFileName().isEmpty() && !thisTextEdit->document()->isModified() &&
                !thisTextEdit->document()->isEmpty()) {
                if (sidePane_ && !sideItems_.isEmpty()) {
                    if (QListWidgetItem* wi = sideItems_.key(thisTabPage))
                        sidePane_->listWidget()->setCurrentItem(wi);  // sets the current widget at changeTab()
                }
                else
                    ui->tabWidget->setCurrentWidget(thisTabPage);
                return;
            }
        }
    }

    /* see if a translated help file exists */
    /*QString lang;
    QStringList langs (QLocale::system().uiLanguages());
    if (!langs.isEmpty())
        lang = langs.first().replace ('-', '_');

#if defined (Q_OS_HAIKU)
    QString helpPath (QStringLiteral (DATADIR) + "/help_" + lang);
#elif defined (Q_OS_MAC)
    QString helpPath (qApp->applicationDirPath() + QStringLiteral ("/../Resources/") + "/help_" + lang);
#else
    QString helpPath (QStringLiteral (DATADIR) + "/featherpad/help_" + lang);
#endif

    if (!QFile::exists (helpPath) && !langs.isEmpty())
    {
        lang = langs.first().split (QLatin1Char ('_')).first();
#if defined(Q_OS_HAIKU)
        helpPath = QStringLiteral (DATADIR) + "/help_" + lang;
#elif defined(Q_OS_MAC)
        helpPath = qApp->applicationDirPath() + QStringLiteral ("/../Resources/") + "/help_" + lang;
#else
        helpPath = QStringLiteral (DATADIR) + "/featherpad/help_" + lang;
#endif
    }

    if (!QFile::exists (helpPath))
    {*/
#if defined(Q_OS_HAIKU)
    QString helpPath = QStringLiteral(DATADIR) + "/help";
#elif defined(Q_OS_MAC)
    QString helpPath = qApp->applicationDirPath() + QStringLiteral("/../Resources/") + "/help";
#else
    QString helpPath = QStringLiteral(DATADIR) + "/featherpad/help";
#endif
    //}

    QFile helpFile(helpPath);
    if (!helpFile.exists())
        return;
    if (!helpFile.open(QFile::ReadOnly))
        return;

    TextEdit* textEdit = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())->textEdit();
    if (!textEdit->document()->isEmpty() || textEdit->document()->isModified() ||
        !textEdit->getFileName().isEmpty())  // an empty file is just opened
    {
        createEmptyTab(!isLoading(), false);
        textEdit = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())->textEdit();
    }
    else if (textEdit->getHighlighter())
        syntaxHighlighting(textEdit, false);

    if (!textEdit->getLang().isEmpty()) {  // remove the enforced syntax, if any
        textEdit->setLang(QString());
        updateLangBtn(textEdit);
    }

    QByteArray data = helpFile.readAll();
    helpFile.close();
    auto decoder = QStringDecoder(QStringDecoder::Utf8);
    QString str = decoder.decode(data);
    textEdit->setPlainText(str);

    textEdit->setReadOnly(true);
    if (!textEdit->hasDarkScheme())
        textEdit->viewport()->setStyleSheet(
            ".QWidget {"
            "color: black;"
            "background-color: rgb(225, 238, 255);}");
    else
        textEdit->viewport()->setStyleSheet(
            ".QWidget {"
            "color: white;"
            "background-color: rgb(0, 60, 110);}");
    ui->actionCut->setDisabled(true);
    ui->actionPaste->setDisabled(true);
    ui->actionSoftTab->setDisabled(true);
    ui->actionDate->setDisabled(true);
    ui->actionDelete->setDisabled(true);
    ui->actionUpperCase->setDisabled(true);
    ui->actionLowerCase->setDisabled(true);
    ui->actionStartCase->setDisabled(true);
    disconnect(textEdit, &TextEdit::canCopy, ui->actionCut, &QAction::setEnabled);
    disconnect(textEdit, &TextEdit::canCopy, ui->actionDelete, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionUpperCase, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionLowerCase, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionStartCase, &QAction::setEnabled);

    index = ui->tabWidget->currentIndex();
    textEdit->setEncoding("UTF-8");
    textEdit->setWordNumber(-1);
    textEdit->setProg("help");  // just for marking
    if (ui->statusBar->isVisible()) {
        statusMsgWithLineCount(textEdit->document()->blockCount());
        if (QToolButton* wordButton = ui->statusBar->findChild<QToolButton*>("wordButton"))
            wordButton->setVisible(true);
    }
    if (QToolButton* langButton = ui->statusBar->findChild<QToolButton*>("langButton"))
        langButton->setEnabled(false);
    encodingToCheck("UTF-8");
    QString title = "** " + tr("Help") + " **";
    ui->tabWidget->setTabText(index, title);
    setWindowTitle(title + "[*]");
    if (auto mbt = qobject_cast<MenuBarTitle*>(ui->menuBar->cornerWidget()))
        mbt->setTitle(title);
    setWindowModified(false);
    ui->tabWidget->setTabToolTip(index, title);
    if (sidePane_) {
        if (QListWidgetItem* cur = sidePane_->listWidget()->currentItem()) {
            cur->setText(title);
            cur->setToolTip(title);
        }
    }
}
/*************************/
void FPwin::stealFocus(QWidget* w) {
    if (w->isMinimized())
        w->setWindowState((w->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
#ifdef HAS_X11
    else if (static_cast<FPsingleton*>(qApp)->isX11()) {
        if (isWindowShaded(w->winId()))
            unshadeWindow(w->winId());
    }
#endif

    raise();
    /* WARNING: Under Wayland, this warning is shown by qtwayland -> qwaylandwindow.cpp
                -> QWaylandWindow::requestActivateWindow():
                "Wayland does not support QWindow::requestActivate()" */
    if (!static_cast<FPsingleton*>(qApp)->isWayland()) {
        w->activateWindow();
        QTimer::singleShot(0, w, [w]() {
            if (QWindow* win = w->windowHandle())
                win->requestActivate();
        });
    }
    else if (!w->isActiveWindow()) {
        /* This is the only way to demand attention under Wayland,
           although Wayland WMs may ignore it. */
        QApplication::alert(w);
    }
}
/*************************/
void FPwin::stealFocus() {
    /* if there is a (sessions) dialog, let it keep the focus */
    const auto dialogs = findChildren<QDialog*>();
    if (!dialogs.isEmpty()) {
        stealFocus(dialogs.at(0));
        return;
    }

    stealFocus(this);
}

}  // namespace FeatherPad
