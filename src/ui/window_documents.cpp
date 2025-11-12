// src/ui/window_documents.cpp

#include "texxy_ui_prelude.h"
#include <algorithm>  // for std::clamp

namespace Texxy {

void TexxyWindow::deleteTabPage(int tabIndex, bool saveToList, bool closeWithLastTab) {
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(tabIndex));
    if (!tabPage)
        return;

    if (sidePane_ && !sideItems_.isEmpty()) {
        if (QListWidgetItem* wi = sideItems_.key(tabPage)) {
            sideItems_.remove(wi);
            delete sidePane_->listWidget()->takeItem(sidePane_->listWidget()->row(wi));
        }
    }

    TextEdit* textEdit = tabPage->textEdit();
    const QString fileName = textEdit->getFileName();
    Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();

    if (!fileName.isEmpty()) {
        if (textEdit->getSaveCursor())
            config.saveCursorPos(fileName, textEdit->textCursor().position());
        if (saveToList && config.getSaveLastFilesList() && QFile::exists(fileName))
            lastWinFilesCur_.insert(fileName, textEdit->textCursor().position());
    }

    // deleting the syntax highlighter changes the text, disconnect contentsChange to prevent a crash
    disconnect(textEdit, &QPlainTextEdit::textChanged, this, &TexxyWindow::hlight);
    disconnect(textEdit->document(), &QTextDocument::contentsChange, this, &TexxyWindow::updateWordInfo);
    if (config.getSelectionHighlighting())
        disconnect(textEdit->document(), &QTextDocument::contentsChange, textEdit, &TextEdit::onContentsChange);

    syntaxHighlighting(textEdit, false);
    ui->tabWidget->removeTab(tabIndex);
    delete tabPage;
    tabPage = nullptr;

    if (closeWithLastTab && config.getCloseWithLastTab() && ui->tabWidget->count() == 0)
        close();
}

void TexxyWindow::newTab() {
    createEmptyTab(!isLoading());
}

TabPage* TexxyWindow::createEmptyTab(bool setCurrent, bool allowNormalHighlighter) {
    TexxyApplication* singleton = static_cast<TexxyApplication*>(qApp);
    const Config config = singleton->getConfig();

    static const QList<QKeySequence> searchShortcuts = {
        QKeySequence(Qt::Key_F3),
        QKeySequence(Qt::Key_F4),
        QKeySequence(Qt::Key_F5),
        QKeySequence(Qt::Key_F6),
        QKeySequence(Qt::Key_F7)
    };

    TabPage* tabPage =
        new TabPage(config.getDarkColScheme() ? config.getDarkBgColorValue() : config.getLightBgColorValue(),
                    searchShortcuts, nullptr);

    tabPage->setSearchModel(singleton->searchModel());

    TextEdit* textEdit = tabPage->textEdit();
    connect(textEdit, &QWidget::customContextMenuRequested, this, &TexxyWindow::editorContextMenu);
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
        syntaxHighlighting(textEdit);  // the default url syntax highlighter

    int index = ui->tabWidget->currentIndex();
    if (index == -1)
        enableWidgets(true);

    // hide the searchbar consistently
    if ((index == -1 && config.getHideSearchbar()) ||
        (index > -1 && !qobject_cast<TabPage*>(ui->tabWidget->widget(index))->isSearchBarVisible())) {
        tabPage->setSearchBarVisible(false);
    }

    ui->tabWidget->insertTab(index + 1, tabPage, tr("Untitled"));

    // set all preliminary properties
    if (index >= 0)
        updateGUIForSingleTab(false);

    ui->tabWidget->setTabToolTip(index + 1, tr("Unsaved"));
    if (!ui->actionWrap->isChecked())
        textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    if (!ui->actionIndent->isChecked())
        textEdit->setAutoIndentation(false);
    if (lineContextVisible())
        textEdit->showLineNumbers(true);
    if (ui->spinBox->isVisible())
        connect(textEdit->document(), &QTextDocument::blockCountChanged, this, &TexxyWindow::setMax);

    if (ui->statusBar->isVisible() || config.getShowStatusbar()) {
        // if this becomes the current tab, tabSwitch will take care of labels and buttons
        connect(textEdit, &QPlainTextEdit::blockCountChanged, this, &TexxyWindow::statusMsgWithLineCount);
        connect(textEdit, &TextEdit::selChanged, this, &TexxyWindow::statusMsg);
        if (config.getShowCursorPos())
            connect(textEdit, &QPlainTextEdit::cursorPositionChanged, this, &TexxyWindow::showCursorPos);
    }

    connect(textEdit->document(), &QTextDocument::undoAvailable, ui->actionUndo, &QAction::setEnabled);
    connect(textEdit->document(), &QTextDocument::redoAvailable, ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        connect(textEdit->document(), &QTextDocument::modificationChanged, this, &TexxyWindow::enableSaving);
    connect(textEdit->document(), &QTextDocument::modificationChanged, this, &TexxyWindow::asterisk);
    connect(textEdit, &TextEdit::canCopy, ui->actionCut, &QAction::setEnabled);
    connect(textEdit, &TextEdit::canCopy, ui->actionDelete, &QAction::setEnabled);
    connect(textEdit, &TextEdit::canCopy, ui->actionCopy, &QAction::setEnabled);
    connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionUpperCase, &QAction::setEnabled);
    connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionLowerCase, &QAction::setEnabled);
    connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionStartCase, &QAction::setEnabled);

    connect(textEdit, &TextEdit::filePasted, this, &TexxyWindow::newTabFromName);
    connect(textEdit, &TextEdit::zoomedOut, this, &TexxyWindow::reformat);
    connect(textEdit, &TextEdit::hugeColumn, this, &TexxyWindow::columnWarning);

    connect(tabPage, &TabPage::find, this, &TexxyWindow::find);
    connect(tabPage, &TabPage::searchFlagChanged, this, &TexxyWindow::searchFlagChanged);

    // workaround: under KDE the first selection may not reach the selection clipboard
    // QApplication::clipboard()->text(QClipboard::Selection);

    if (sidePane_) {
        ListWidget* lw = sidePane_->listWidget();
        auto* lwi = new ListWidgetItem(tr("Untitled"), lw);
        lwi->setToolTip(tr("Unsaved"));
        sideItems_.insert(lwi, tabPage);
        lw->addItem(lwi);
        if (setCurrent || index == -1)
            lw->setCurrentItem(lwi);
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
    else if (static_cast<TexxyApplication*>(qApp)->isX11()) {
        if (isWindowShaded(winId()))
            unshadeWindow(winId());
    }
#endif

    return tabPage;
}

void TexxyWindow::updateRecenMenu() {
    const Config config = static_cast<TexxyApplication*>(qApp)->getConfig();
    const QStringList recentFiles = config.getRecentFiles();
    const int recentSize = recentFiles.count();
    const int recentNumber = config.getCurRecentFilesNumber();

    QList<QAction*> actions = ui->menuOpenRecently->actions();
    const QFontMetrics metrics(ui->menuOpenRecently->font());
    const int w = 150 * metrics.horizontalAdvance(QLatin1Char(' '));
    QMimeDatabase mimeDatabase;

    for (int i = 0; i < recentNumber; ++i) {
        if (i < recentSize) {
            actions.at(i)->setText(metrics.elidedText(recentFiles.at(i), Qt::ElideMiddle, w));
            QIcon icon;
            const auto mimes = mimeDatabase.mimeTypesForFileName(recentFiles.at(i).section(QLatin1Char('/'), -1));
            if (!mimes.isEmpty())
                icon = QIcon::fromTheme(mimes.at(0).iconName());
            actions.at(i)->setIcon(icon);
            actions.at(i)->setData(recentFiles.at(i));
            actions.at(i)->setVisible(true);
        } else {
            actions.at(i)->setText(QString());
            actions.at(i)->setIcon(QIcon());
            actions.at(i)->setData(QVariant());
            actions.at(i)->setVisible(false);
        }
    }
    ui->actionClearRecent->setEnabled(recentSize != 0);
}

void TexxyWindow::clearRecentMenu() {
    Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();
    config.clearRecentFiles();
    updateRecenMenu();
}

void TexxyWindow::addRecentFile(const QString& file) {
    Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();
    config.addRecentFile(file);

    // try to make other windows know about this file
    TexxyApplication* singleton = static_cast<TexxyApplication*>(qApp);
    if (singleton->isStandAlone())
        singleton->sendRecentFile(file, config.getRecentOpened());
}

bool TexxyWindow::isScriptLang(const QString& lang) const {
    return lang == QLatin1String("sh") || lang == QLatin1String("python") || lang == QLatin1String("ruby") ||
           lang == QLatin1String("lua") || lang == QLatin1String("perl");
}

void TexxyWindow::enableSaving(bool modified) {
    if (!inactiveTabModified_)
        ui->actionSave->setEnabled(modified);
}

void TexxyWindow::asterisk(bool modified) {
    if (inactiveTabModified_)
        return;

    const int index = ui->tabWidget->currentIndex();
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index));
    if (!tabPage)
        return;

    const QString fname = tabPage->textEdit()->getFileName();
    QString shownName;

    if (fname.isEmpty()) {
        shownName = tr("Untitled");
        setWinTitle((modified ? QStringLiteral("*") : QString()) + shownName);
    } else {
        shownName = fname.section(QLatin1Char('/'), -1);
        setWinTitle((modified ? QStringLiteral("*") : QString()) +
                    (fname.contains(QLatin1Char('/')) ? fname
                                                      : QFileInfo(fname).absolutePath() + QLatin1Char('/') + fname));
    }
    shownName.replace(QLatin1Char('\n'), QLatin1Char(' '));

    if (sidePane_ && !sideItems_.isEmpty()) {
        if (QListWidgetItem* wi = sideItems_.key(tabPage))
            wi->setText(modified ? shownName + QLatin1Char('*') : shownName);
    }

    if (modified)
        shownName.prepend(QLatin1Char('*'));
    shownName.replace(QLatin1Char('&'), QStringLiteral("&&"));
    shownName.replace(QLatin1Char('\t'), QLatin1Char(' '));
    ui->tabWidget->setTabText(index, shownName);
}

void TexxyWindow::loadText(const QString& fileName,
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

    auto* thread =
        new Loading(fileName, charset, reload, restoreCursor, posInLine, enforceUneditable, multiple);
    thread->setSkipNonText(static_cast<TexxyApplication*>(qApp)->getConfig().getSkipNonText());
    connect(thread, &Loading::completed, this, &TexxyWindow::addText);
    connect(thread, &Loading::finished, thread, &QObject::deleteLater);
    thread->start();

    makeBusy();
    ui->tabWidget->tabBar()->lockTabs(true);
    updateShortcuts(true, false);
}

void TexxyWindow::addText(const QString& text,
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
            connect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onOpeningHugeFiles, Qt::UniqueConnection);
        else if (fileName.isEmpty() && !charset.isEmpty())  // non-text file that shouldn't be opened
            connect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onOpeninNonTextFiles,
                    Qt::UniqueConnection);
        else
            connect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onPermissionDenied, Qt::UniqueConnection);

        --loadingProcesses_;  // cannot become negative
        if (!isLoading()) {
            ui->tabWidget->tabBar()->lockTabs(false);
            updateShortcuts(false, false);
            closeWarningBar();
            emit finishedLoading();
            QTimer::singleShot(0, this, &TexxyWindow::unbusy);
            stealFocus();
        }
        return;
    }

    if (enforceEncod || reload)
        multiple = false;

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
    } else {
        if (sidePane_ && !reload && !enforceEncod)  // an unused empty tab
            scrollToFirstItem = true;
    }

    textEdit->setSaveCursor(restoreCursor == 1);
    textEdit->setLang(QString());  // remove enforced syntax

    // capture view position before changing highlighter on reload
    TextEdit::viewPosition vPos;
    if (reload) {
        textEdit->forgetTxtCurHPos();
        vPos = textEdit->getViewPosition();
    }

    // temporarily remove highlighter to avoid redundant work during setPlainText
    if (textEdit->getHighlighter()) {
        textEdit->setGreenSel(QList<QTextEdit::ExtraSelection>());  // previous finds are meaningless after load
        syntaxHighlighting(textEdit, false);
    }

    const QFileInfo fInfo(fileName);
    Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();

    // set the text
    inactiveTabModified_ = true;   // ignore modificationChanged during initial set
    textEdit->setPlainText(text);  // resets undo/redo
    inactiveTabModified_ = false;

    // restore cursor position if requested
    if (!reload && restoreCursor != 0) {
        if (restoreCursor == 1 || restoreCursor == -1) {
            // restore cursor from settings
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
        } else if (restoreCursor < -1) {
            // doc end in commandline
            QTextCursor cur = textEdit->textCursor();
            cur.movePosition(QTextCursor::End);
            QTimer::singleShot(0, textEdit, [textEdit, cur] { textEdit->setTextCursor(cur); });
        } else {
            // restoreCursor >= 2 means 1-based line number
            const int line0 = restoreCursor - 2;  // Qt blocks start at 0
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
            } else {
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

    if (sidePane_ && !fileName.isEmpty())
        sidePane_->revealFile(fileName);

    textEdit->makeUneditable(uneditable);
    if (uneditable) {
        if (!reload)  // on reload this will be connected later
            connect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onOpeningUneditable, Qt::UniqueConnection);
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
        } else {
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
    } else if (textEdit->isReadOnly()) {
        QTimer::singleShot(0, this, &TexxyWindow::makeEditable);
    }

    // UI updates for the active tab when not opening multiple files at once
    if (!multiple || openInCurrentTab) {
        if (!fInfo.exists())
            connect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onOpeningNonexistent,
                    Qt::UniqueConnection);

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
            lambdaConnection_ = QObject::connect(this, &TexxyWindow::finishedLoading, textEdit, [this, textEdit, vPos] {
                QTimer::singleShot(0, textEdit, [textEdit, vPos] { textEdit->setViewPostion(vPos); });
                disconnectLambda();
            });
            if (uneditable)
                connect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onOpeningUneditable,
                        Qt::UniqueConnection);
        } else if (firstItem) {
            // select the first item when sidePane exists
            sidePane_->listWidget()->setCurrentItem(firstItem);
        }

        // reset static side-pane helpers
        scrollToFirstItem = false;
        firstItem = nullptr;

        closeWarningBar(true);  // allow closing animation to finish
        emit finishedLoading();
        QTimer::singleShot(0, this, &TexxyWindow::unbusy);  // remove busy cursor after pending events like highlighting
        stealFocus();
    }
}

void TexxyWindow::disconnectLambda() {
    QObject::disconnect(lambdaConnection_);
}

void TexxyWindow::onOpeningHugeFiles() {
    disconnect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onOpeningHugeFiles);
    QTimer::singleShot(0, this, [=]() {
        showWarningBar(QStringLiteral("<center><b><big>%1</big></b></center>\n<center>%2</center>")
                           .arg(tr("Huge file(s) not opened!"),
                                tr("Texxy does not open files larger than 100 MiB.")));
    });
}

void TexxyWindow::onOpeninNonTextFiles() {
    disconnect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onOpeninNonTextFiles);
    QTimer::singleShot(0, this, [=]() {
        showWarningBar(QStringLiteral("<center><b><big>%1</big></b></center>\n<center><i>%2</i></center>")
                           .arg(tr("Non-text file(s) not opened!"),
                                tr("See Preferences → Files → Do not permit opening of non-text files")),
                       20);
    });
}

void TexxyWindow::onPermissionDenied() {
    disconnect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onPermissionDenied);
    QTimer::singleShot(0, this, [=]() {
        showWarningBar(QStringLiteral("<center><b><big>%1</big></b></center>\n<center>%2</center>")
                           .arg(tr("Some file(s) could not be opened!"),
                                tr("You may not have the permission to read.")));
    });
}

void TexxyWindow::onOpeningUneditable() {
    disconnect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onOpeningUneditable);
    // a timer is needed because the scrollbar position is restored on reloading by a lambda connection
    QTimer::singleShot(0, this, [=]() {
        showWarningBar(QStringLiteral("<center><b><big>%1</big></b></center>\n<center>%2</center>")
                           .arg(tr("Uneditable file(s)!"),
                                tr("Non-text files or files with huge lines cannot be edited.")));
    });
}

void TexxyWindow::onOpeningNonexistent() {
    disconnect(this, &TexxyWindow::finishedLoading, this, &TexxyWindow::onOpeningNonexistent);
    QTimer::singleShot(0, this, [=]() {
        // show the bar only if the current file doesn't exist at this very moment
        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
            const QString fname = tabPage->textEdit()->getFileName();
            if (!fname.isEmpty() && !QFile::exists(fname))
                showWarningBar(QStringLiteral("<center><b><big>%1</big></b></center>")
                                   .arg(tr("The file does not exist.")));
        }
    });
}

void TexxyWindow::columnWarning() {
    showWarningBar(QStringLiteral("<center><b><big>%1</big></b></center>\n<center>%2</center>")
                       .arg(tr("Huge column!"),
                            tr("Columns with more than 1000 rows are not supported.")));
}

void TexxyWindow::newTabFromName(const QString& fileName, int restoreCursor, int posInLine, bool multiple) {
    if (!fileName.isEmpty())
        loadText(fileName, false, false, restoreCursor, posInLine, false, multiple);
}

void TexxyWindow::newTabFromRecent() {
    QAction* action = qobject_cast<QAction*>(QObject::sender());
    if (!action)
        return;
    loadText(action->data().toString(), false, false);
}

void TexxyWindow::fileOpen() {
    openFilesFromDialog();
}

void TexxyWindow::openFilesFromDialog() {
    if (isLoading())
        return;

    // find a suitable directory
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
    } else {
        // remember the last opened file
        fname = lastFile_;
        if (!fname.isEmpty()) {
            QDir dir = QFileInfo(fname).absoluteDir();
            if (!dir.exists())
                dir = QDir::home();
            path = dir.path();
        } else {
            QDir dir = QDir::home();
            path = dir.path();
        }
    }

    if (hasAnotherDialog())
        return;

    updateShortcuts(true);
    QString filter = tr("All Files") + QStringLiteral(" (*)");
    if (!fname.isEmpty() && QFileInfo(fname).fileName().contains(QLatin1Char('.'))) {
        // if relevant, do filtering to make opening of similar files easier
        filter = tr("All Files") + QStringLiteral(" (*);;*.%1").arg(fname.section(QLatin1Char('.'), -1, -1));
    }

    FileDialog dialog(this, static_cast<TexxyApplication*>(qApp)->getConfig().getNativeDialog());
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setWindowTitle(tr("Open file..."));
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter(filter);
    // dialog.setLabelText(QFileDialog::Accept, tr("Open"));
    // dialog.setLabelText(QFileDialog::Reject, tr("Cancel"));
    if (QFileInfo(path).isDir())
        dialog.setDirectory(path);
    else {
        dialog.setDirectory(path.section(QLatin1Char('/'), 0, -2));  // KDE file dialog is buggy and needs this
        dialog.selectFile(path);
        dialog.autoScroll();
    }

    if (dialog.exec()) {
        const QStringList files = dialog.selectedFiles();
        const bool multiple = files.count() > 1 || isLoading();
        for (const QString& file : files)
            loadText(file, false, false, 0, 0, false, multiple);
    }
    updateShortcuts(false);
}

bool TexxyWindow::alreadyOpen(TabPage* tabPage) const {
    bool res = false;

    const QString fileName = tabPage->textEdit()->getFileName();
    const QFileInfo info(fileName);
    const bool exists = info.exists();
    const QString target = info.isSymLink() ? info.symLinkTarget() : fileName;

    TexxyApplication* singleton = static_cast<TexxyApplication*>(qApp);
    for (int i = 0; i < singleton->Wins.count(); ++i) {
        TexxyWindow* thisOne = singleton->Wins.at(i);
        for (int j = 0; j < thisOne->ui->tabWidget->count(); ++j) {
            TabPage* thisTabPage = qobject_cast<TabPage*>(thisOne->ui->tabWidget->widget(j));
            if (thisOne == this && thisTabPage == tabPage)
                continue;

            TextEdit* thisTextEdit = thisTabPage->textEdit();
            if (thisTextEdit->isReadOnly())
                continue;

            const QFileInfo thisInfo(thisTextEdit->getFileName());
            const QString thisTarget = thisInfo.isSymLink() ? thisInfo.symLinkTarget() : thisTextEdit->getFileName();
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

void TexxyWindow::enforceEncoding(QAction* a) {
    // encoding has no keyboard shortcut or tool button
    if (isLoading())
        return;

    const int index = ui->tabWidget->currentIndex();
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index));
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    const QString fname = textEdit->getFileName();

    if (!fname.isEmpty()) {
        if (savePrompt(index, false) != SAVED) {
            // back to the previous encoding
            if (!locked_)
                encodingToCheck(textEdit->getEncoding());
            return;
        }
        // if the file is removed, close its tab to open a new one
        if (!QFile::exists(fname))
            deleteTabPage(index, false, false);

        a->setChecked(true);  // the checked action might have been changed to UTF-8 with saving
        loadText(fname, true, true, 0, 0, textEdit->isUneditable(), false);
    } else {
        // just change the statusbar text, the doc might be saved later with the new encoding
        textEdit->setEncoding(checkToEncoding());
        if (ui->statusBar->isVisible()) {
            QLabel* statusLabel = ui->statusBar->findChild<QLabel*>(QStringLiteral("statusLabel"));
            QString str = statusLabel->text();
            const QString encodStr = tr("Encoding");
            const QString lineStr = QStringLiteral("</i>&nbsp;&nbsp;&nbsp;<b>%1").arg(tr("Lines"));
            const int i = str.indexOf(encodStr);
            const int j = str.indexOf(lineStr);
            const int offset = encodStr.size() + 9;  // size of ":</b> <i>"
            str.replace(i + offset, j - i - offset, checkToEncoding());
            statusLabel->setText(str);
        }
    }
}

void TexxyWindow::reload() {
    if (isLoading())
        return;

    const int index = ui->tabWidget->currentIndex();
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index));
    if (!tabPage)
        return;

    if (savePrompt(index, false) != SAVED)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    const QString fname = textEdit->getFileName();

    // if the file is removed, close its tab to open a new one
    if (!QFile::exists(fname))
        deleteTabPage(index, false, false);

    if (!fname.isEmpty())
        loadText(fname, false, true, textEdit->getSaveCursor() ? 1 : 0);
}

void TexxyWindow::reloadSyntaxHighlighter(TextEdit* textEdit) {
    // uninstall and reinstall the syntax highlighter if the programming language is changed
    const QString prevLan = textEdit->getProg();
    setProgLang(textEdit);
    if (prevLan == textEdit->getProg())
        return;

    Config config = static_cast<TexxyApplication*>(qApp)->getConfig();
    if (config.getShowLangSelector() && config.getSyntaxByDefault()) {
        if (textEdit->getLang() == textEdit->getProg())
            textEdit->setLang(QString());  // not enforced because it's the real syntax
        updateLangBtn(textEdit);
    }

    if (ui->statusBar->isVisible() && textEdit->getWordNumber() != -1) {
        // we want to change the statusbar text below
        disconnect(textEdit->document(), &QTextDocument::contentsChange, this, &TexxyWindow::updateWordInfo);
    }

    if (textEdit->getLang().isEmpty()) {
        // restart highlighting only when the language isn't forced
        syntaxHighlighting(textEdit, false);
        if (ui->actionSyntax->isChecked())
            syntaxHighlighting(textEdit);
    }

    if (ui->statusBar->isVisible()) {
        // correct the statusbar text by replacing the old syntax info
        QLabel* statusLabel = ui->statusBar->findChild<QLabel*>(QStringLiteral("statusLabel"));
        QString str = statusLabel->text();
        QString syntaxStr = tr("Syntax");
        int i = str.indexOf(syntaxStr);
        if (i == -1) {
            // there was no real language before saving, prevLan was url
            const QString lineStr = QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1").arg(tr("Lines"));
            const int j = str.indexOf(lineStr);
            syntaxStr = QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1</b> <i>%2</i>")
                            .arg(tr("Syntax:"))
                            .arg(textEdit->getProg());
            str.insert(j, syntaxStr);
        } else {
            if (textEdit->getProg() == QLatin1String("url")) {
                // there's no real language after saving
                const QString syntaxTag = QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1").arg(tr("Syntax"));
                const QString lineTag = QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1").arg(tr("Lines"));
                const int j = str.indexOf(syntaxTag);
                const int k = str.indexOf(lineTag);
                str.remove(j, k - j);
            } else {
                // the language is changed by saving
                const QString linesEnd = QStringLiteral("</i>&nbsp;&nbsp;&nbsp;<b>%1").arg(tr("Lines"));
                const int j = str.indexOf(linesEnd);
                const int offset = syntaxStr.size() + 9;  // size of ":</b> <i>"
                str.replace(i + offset, j - i - offset, textEdit->getProg());
            }
        }
        statusLabel->setText(str);
        if (textEdit->getWordNumber() != -1)
            connect(textEdit->document(), &QTextDocument::contentsChange, this, &TexxyWindow::updateWordInfo);
    }
}

void TexxyWindow::lockWindow(TabPage* tabPage, bool lock) {
    locked_ = lock;
    if (lock) {
        pauseAutoSaving(true);
        // close Session Manager
        const QList<QDialog*> dialogs = findChildren<QDialog*>();
        for (int i = 0; i < dialogs.count(); ++i) {
            if (dialogs.at(i)->objectName() == QLatin1String("sessionDialog")) {
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

void TexxyWindow::fontDialog() {
    if (isLoading())
        return;

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;
    if (hasAnotherDialog())
        return;

    updateShortcuts(true);

    TextEdit* textEdit = tabPage->textEdit();

    const QFont currentFont = textEdit->getDefaultFont();
    FontDialog fd(currentFont, this);
    fd.setWindowModality(Qt::WindowModal);

    if (fd.exec()) {
        const QFont newFont = fd.selectedFont();
        Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();
        if (config.getRemFont()) {
            config.setFont(newFont);
            config.writeConfig();

            TexxyApplication* singleton = static_cast<TexxyApplication*>(qApp);
            for (int i = 0; i < singleton->Wins.count(); ++i) {
                TexxyWindow* thisWin = singleton->Wins.at(i);
                for (int j = 0; j < thisWin->ui->tabWidget->count(); ++j) {
                    TextEdit* thisTextEdit = qobject_cast<TabPage*>(thisWin->ui->tabWidget->widget(j))->textEdit();
                    thisTextEdit->setEditorFont(newFont);
                }
            }
        } else {
            textEdit->setEditorFont(newFont);
        }

        // the font can become larger
        textEdit->adjustScrollbars();
        // ... or smaller
        reformat(textEdit);
    }
    updateShortcuts(false);
}

void TexxyWindow::encodingToCheck(const QString& encoding) {
    ui->actionOther->setDisabled(true);

    if (encoding == QLatin1String("UTF-8"))
        ui->actionUTF_8->setChecked(true);
    else if (encoding == QLatin1String("UTF-16"))
        ui->actionUTF_16->setChecked(true);
    else if (encoding == QLatin1String("ISO-8859-1"))
        ui->actionISO_8859_1->setChecked(true);
    else {
        ui->actionOther->setDisabled(false);
        ui->actionOther->setChecked(true);
    }
}

const QString TexxyWindow::checkToEncoding() const {
    QString encoding;

    if (ui->actionUTF_8->isChecked())
        encoding = QStringLiteral("UTF-8");
    else if (ui->actionUTF_16->isChecked())
        encoding = QStringLiteral("UTF-16");
    else if (ui->actionISO_8859_1->isChecked())
        encoding = QStringLiteral("ISO-8859-1");
    else
        encoding = QStringLiteral("UTF-8");

    return encoding;
}

void TexxyWindow::docProp() {
    const bool showCurPos = static_cast<TexxyApplication*>(qApp)->getConfig().getShowCursorPos();
    if (ui->statusBar->isVisible()) {
        for (int i = 0; i < ui->tabWidget->count(); ++i) {
            TextEdit* thisTextEdit = qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit();
            disconnect(thisTextEdit, &QPlainTextEdit::blockCountChanged, this, &TexxyWindow::statusMsgWithLineCount);
            disconnect(thisTextEdit, &TextEdit::selChanged, this, &TexxyWindow::statusMsg);
            if (showCurPos)
                disconnect(thisTextEdit, &QPlainTextEdit::cursorPositionChanged, this, &TexxyWindow::showCursorPos);
            // don't delete the cursor position label because the statusbar might be shown later
        }
        ui->statusBar->setVisible(false);
        return;
    }

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    statusMsgWithLineCount(tabPage->textEdit()->document()->blockCount());
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        TextEdit* thisTextEdit = qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit();
        connect(thisTextEdit, &QPlainTextEdit::blockCountChanged, this, &TexxyWindow::statusMsgWithLineCount);
        connect(thisTextEdit, &TextEdit::selChanged, this, &TexxyWindow::statusMsg);
        if (showCurPos)
            connect(thisTextEdit, &QPlainTextEdit::cursorPositionChanged, this, &TexxyWindow::showCursorPos);
    }

    ui->statusBar->setVisible(true);
    if (showCurPos) {
        addCursorPosLabel();
        showCursorPos();
    }
    if (QToolButton* wordButton = ui->statusBar->findChild<QToolButton*>(QStringLiteral("wordButton")))
        wordButton->setVisible(true);
    updateWordInfo();
}

void TexxyWindow::statusMsgWithLineCount(const int lines) {
    TextEdit* textEdit = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())->textEdit();
    // ensure that the signal comes from the active tab if this is about a connection
    if (qobject_cast<TextEdit*>(QObject::sender()) && QObject::sender() != textEdit)
        return;

    QLabel* statusLabel = ui->statusBar->findChild<QLabel*>(QStringLiteral("statusLabel"));

    // order: Encoding -> Syntax -> Lines -> Sel. Chars -> Words
    const QString encodStr =
        QStringLiteral("<b>%1</b> <i>%2</i>").arg(tr("Encoding:"), textEdit->getEncoding());

    QString syntaxStr;
    if (textEdit->getProg() != QLatin1String("help") && textEdit->getProg() != QLatin1String("url"))
        syntaxStr =
            QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1</b> <i>%2</i>").arg(tr("Syntax:"), textEdit->getProg());

    const QLocale l = locale();
    const QString lineStr =
        QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1</b> <i>%2</i>").arg(tr("Lines:"), l.toString(lines));
    const QString selStr = QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1</b> <i>%2</i>")
                               .arg(tr("Sel. Chars:"), l.toString(textEdit->selectionSize()));
    const QString wordStr = QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1</b>").arg(tr("Words"));

    statusLabel->setText(encodStr + syntaxStr + lineStr + selStr + wordStr);
}

void TexxyWindow::statusMsg() {
    const QLocale l = locale();
    QLabel* statusLabel = ui->statusBar->findChild<QLabel*>(QStringLiteral("statusLabel"));
    const int sel = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())->textEdit()->selectionSize();
    QString str = statusLabel->text();
    const QString selStr = tr("Sel. Chars");
    const QString wordStr = QStringLiteral("&nbsp;&nbsp;&nbsp;<b>%1").arg(tr("Words"));
    const int i = str.indexOf(selStr) + selStr.size();
    const int j = str.indexOf(wordStr);
    if (sel == 0) {
        const QString prevSel = str.mid(i + 9, j - i - 13);  // j - (i + 9[":</b> <i>"]) - 4["</i>"]
        if (l.toInt(prevSel) == 0)
            return;
    }
    const QString charN = l.toString(sel);
    str.replace(i + 9, j - i - 13, charN);
    statusLabel->setText(str);
}

void TexxyWindow::showCursorPos() {
    QLabel* posLabel = ui->statusBar->findChild<QLabel*>(QStringLiteral("posLabel"));
    if (!posLabel)
        return;

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    const int pos = tabPage->textEdit()->textCursor().positionInBlock();
    const QString charN = QStringLiteral("<i> %1</i>").arg(locale().toString(pos));
    QString str = posLabel->text();
    const QString scursorStr = QStringLiteral("<b>%1</b>").arg(tr("Position:"));
    const int i = scursorStr.size();
    str.replace(i, str.size() - i, charN);
    posLabel->setText(str);
}

void TexxyWindow::updateLangBtn(TextEdit* textEdit) {
    QToolButton* langButton = ui->statusBar->findChild<QToolButton*>(QStringLiteral("langButton"));
    if (!langButton)
        return;

    langButton->setEnabled(!textEdit->isUneditable() && textEdit->getHighlighter());

    QString lang = textEdit->getLang().isEmpty() ? textEdit->getProg() : textEdit->getLang();
    QAction* action = langs_.value(lang);
    if (!action) {
        // it's help, url or a bug where the language isn't included
        lang = tr("Normal");
        action = langs_.value(lang);  // Normal is the last action
    }
    langButton->setText(lang);
    if (action)
        action->setChecked(true);
}

void TexxyWindow::enforceLang(QAction* action) {
    QToolButton* langButton = ui->statusBar->findChild<QToolButton*>(QStringLiteral("langButton"));
    if (!langButton)
        return;

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    QString lang = action->text();
    lang.remove(QLatin1Char('&'));  // because of KAcceleratorManager
    langButton->setText(lang);

    if (lang == tr("Normal")) {
        if (textEdit->getProg() == QLatin1String("desktop") || textEdit->getProg() == QLatin1String("theme") ||
            textEdit->getProg() == QLatin1String("openbox") || textEdit->getProg() == QLatin1String("changelog") ||
            textEdit->getProg() == QLatin1String("srt") || textEdit->getProg() == QLatin1String("gtkrc")) {
            // not listed by the language button
            lang = textEdit->getProg();
        } else {
            lang = QStringLiteral("url");  // the default highlighter
        }
    }

    if (textEdit->getProg() == lang || textEdit->getProg() == QLatin1String("help"))
        textEdit->setLang(QString());  // not enforced
    else
        textEdit->setLang(lang);

    if (ui->actionSyntax->isChecked()) {
        syntaxHighlighting(textEdit, false);
        makeBusy();  // may take a while with huge texts
        syntaxHighlighting(textEdit, true, lang);
        QTimer::singleShot(0, this, &TexxyWindow::unbusy);
    }
}

void TexxyWindow::updateWordInfo(int /*position*/, int charsRemoved, int charsAdded) {
    QToolButton* wordButton = ui->statusBar->findChild<QToolButton*>(QStringLiteral("wordButton"));
    if (!wordButton)
        return;

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    TextEdit* textEdit = tabPage->textEdit();

    // ensure that the signal comes from the active tab when the info is going to be removed
    if (qobject_cast<QTextDocument*>(QObject::sender()) && QObject::sender() != textEdit->document())
        return;

    if (wordButton->isVisible()) {
        QLabel* statusLabel = ui->statusBar->findChild<QLabel*>(QStringLiteral("statusLabel"));
        int words = textEdit->getWordNumber();
        if (words == -1) {
            words = textEdit->toPlainText().split(QRegularExpression(QStringLiteral("(\\s|\\n|\\r)+")),
                                                  Qt::SkipEmptyParts).count();
            textEdit->setWordNumber(words);
        }

        wordButton->setVisible(false);
        statusLabel->setText(QStringLiteral("%1 <i>%2</i>").arg(statusLabel->text(), locale().toString(words)));
        connect(textEdit->document(), &QTextDocument::contentsChange, this, &TexxyWindow::updateWordInfo);
    } else if (charsRemoved > 0 || charsAdded > 0) {
        // not if only the format is changed
        disconnect(textEdit->document(), &QTextDocument::contentsChange, this, &TexxyWindow::updateWordInfo);
        textEdit->setWordNumber(-1);
        wordButton->setVisible(true);
        statusMsgWithLineCount(textEdit->document()->blockCount());
    }
}

}  // namespace Texxy
