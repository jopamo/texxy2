// src/ui/window_tabs.cpp
// window_tabs.cpp

#include "texxy_ui_prelude.h"

#include <algorithm>  // std::min, std::max

namespace Texxy {

bool TexxyWindow::closePages(int first, int last, bool saveFilesList) {
    if (!isReady()) {
        closePreviousPages_ = false;
        return true;
    }

    // pause autosave while closing tabs
    struct AutoPause {
        TexxyWindow* self;
        explicit AutoPause(TexxyWindow* s) : self(s) { self->pauseAutoSaving(true); }
        ~AutoPause() { self->pauseAutoSaving(false); }
    } autoPause(this);

    // always clear busy state on exit
    struct FinallyUnbusy {
        TexxyWindow* self;
        explicit FinallyUnbusy(TexxyWindow* s) : self(s) {}
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
        if (lastWinFilesCur_.size() >= kMaxLastWinFiles)  // never remember more than kMaxLastWinFiles
            saveFilesList = false;
    };

    auto nextRightmostIndex = [&](int currentLast, int currentFirst) -> int {
        if (currentLast == 0)  // nothing on the left/top to close
            return -1;         // signal to stop
        if (currentLast < 0)   // close from the end
            return ui->tabWidget->count() - 1;
        if (currentFirst >= currentLast - 1)  // nothing remains in range
            return -1;
        return currentLast - 1;  // last is a left/top boundary, pick the item before it
    };

    bool keep = false;
    const bool closing = saveFilesList;  // saveFilesList is true only with closing
    DOCSTATE state = SAVED;

    while (state == SAVED && ui->tabWidget->count() > 0) {
        makeBusy();

        const int index = nextRightmostIndex(last, first);
        if (index < 0)
            break;

        const int tabIndex = mapIndexToTabIndex(index);

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
                    const int idx = nextRightmostIndex(last, first);
                    if (idx < 0)
                        break;

                    const int t = mapIndexToTabIndex(idx);
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
                            const int t = mapIndexToTabIndex(idx);
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
void TexxyWindow::copyTabFileName() {
    if (rightClicked_ < 0)
        return;

    TabPage* tabPage = nullptr;
    if (sidePane_)
        tabPage = sideItems_.value(sidePane_->listWidget()->item(rightClicked_));
    else
        tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(rightClicked_));

    if (tabPage) {
        const QString fname = tabPage->textEdit()->getFileName();
        QApplication::clipboard()->setText(fname.section(QLatin1Char('/'), -1));
    }
}

/*************************/
void TexxyWindow::copyTabFilePath() {
    if (rightClicked_ < 0)
        return;

    TabPage* tabPage = nullptr;
    if (sidePane_)
        tabPage = sideItems_.value(sidePane_->listWidget()->item(rightClicked_));
    else
        tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(rightClicked_));

    if (tabPage) {
        const QString str = tabPage->textEdit()->getFileName();
        if (!str.isEmpty())
            QApplication::clipboard()->setText(str);
    }
}

/*************************/
void TexxyWindow::closeAllPages() {
    closePages(-1, -1);
}

/*************************/
void TexxyWindow::closeNextPages() {
    closePages(rightClicked_, -1);
}

/*************************/
void TexxyWindow::closePreviousPages() {
    closePages(-1, rightClicked_);
}

/*************************/
void TexxyWindow::closeOtherPages() {
    // saving as root is possible, so prior pages are closed by closePages if needed
    closePreviousPages_ = true;
    closePages(rightClicked_, -1);
}

/*************************/
void TexxyWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (locked_ || !findChildren<QDialog*>().isEmpty())
        return;

    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    // check if this comes from one of our windows and not from a root instance
    else if (event->mimeData()->hasFormat(QStringLiteral("application/texxy-tab")) && event->source() != nullptr) {
        event->acceptProposedAction();
    }
}

/*************************/
void TexxyWindow::dropEvent(QDropEvent* event) {
    if (locked_)
        return;

    if (event->mimeData()->hasFormat(QStringLiteral("application/texxy-tab"))) {
        if (QObject* sourceObject = event->source()) {
            // announce that the drop is accepted by us
            sourceObject->setProperty(TabBar::tabDropped, true);
            // the tab will be dropped after the DND is finished
            const QByteArray data = event->mimeData()->data(QStringLiteral("application/texxy-tab"));
            QTimer::singleShot(0, sourceObject,
                               [this, sourceObject, data]() { dropTab(QString::fromUtf8(data), sourceObject); });
        }
    }
    else {
        const QList<QUrl> urlList = event->mimeData()->urls();
        const bool multiple(urlList.count() > 1 || isLoading());
        for (const QUrl& url : urlList) {
            QString file;
            const QString scheme = url.scheme();
            if (scheme == QStringLiteral("admin")) {  // gvfs admin:///
                file = url.adjusted(QUrl::NormalizePathSegments).path();
            }
            else if (scheme == QStringLiteral("file") || scheme.isEmpty()) {
                // KDE may give a double slash
                file = url.adjusted(QUrl::NormalizePathSegments).toLocalFile();
            }
            else {
                continue;
            }
            newTabFromName(file, 0, 0, multiple);
        }
    }

    event->acceptProposedAction();
}

/*************************/
void TexxyWindow::closePage() {
    if (!isReady())
        return;

    pauseAutoSaving(true);

    QListWidgetItem* curItem = nullptr;
    int tabIndex = -1;
    int index = -1;  // tab index or side-pane row

    if (sidePane_ && rightClicked_ >= 0) {
        index = rightClicked_;
        tabIndex = ui->tabWidget->indexOf(sideItems_.value(sidePane_->listWidget()->item(rightClicked_)));
        if (tabIndex != ui->tabWidget->currentIndex())
            curItem = sidePane_->listWidget()->currentItem();
    }
    else {
        tabIndex = ui->tabWidget->currentIndex();
        if (tabIndex == -1) {
            pauseAutoSaving(false);
            return;
        }
        index = tabIndex;
        if (sidePane_ && !sideItems_.isEmpty()) {
            if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(tabIndex))) {
                if (auto* wi = sideItems_.key(tabPage))
                    index = sidePane_->listWidget()->row(wi);
            }
        }
    }

    if (savePrompt(tabIndex, false, index - 1, index + 1, false, curItem) != SAVED) {
        pauseAutoSaving(false);
        return;
    }

    deleteTabPage(tabIndex);
    const int count = ui->tabWidget->count();
    if (count == 0) {
        ui->actionReload->setDisabled(true);
        ui->actionSave->setDisabled(true);
        enableWidgets(false);
    }
    else {
        if (count == 1)
            updateGUIForSingleTab(true);

        if (curItem)
            sidePane_->listWidget()->setCurrentItem(curItem);

        if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
            tabPage->textEdit()->setFocus();
    }

    pauseAutoSaving(false);
}

/*************************/
void TexxyWindow::closeTabAtIndex(int tabIndex) {
    pauseAutoSaving(true);

    TabPage* curPage = nullptr;
    QListWidgetItem* curItem = nullptr;

    if (tabIndex != ui->tabWidget->currentIndex()) {
        if (sidePane_)
            curItem = sidePane_->listWidget()->currentItem();
        else
            curPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    }

    int index = tabIndex;
    if (sidePane_ && !sideItems_.isEmpty()) {
        if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(tabIndex))) {
            if (auto* wi = sideItems_.key(tabPage))
                index = sidePane_->listWidget()->row(wi);
        }
    }

    if (savePrompt(tabIndex, false, index - 1, index + 1, false, curItem, curPage) != SAVED) {
        pauseAutoSaving(false);
        return;
    }
    closeWarningBar();

    deleteTabPage(tabIndex);
    const int count = ui->tabWidget->count();
    if (count == 0) {
        ui->actionReload->setDisabled(true);
        ui->actionSave->setDisabled(true);
        enableWidgets(false);
    }
    else {
        if (count == 1)
            updateGUIForSingleTab(true);

        if (curPage)
            ui->tabWidget->setCurrentWidget(curPage);
        else if (curItem)
            sidePane_->listWidget()->setCurrentItem(curItem);

        if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
            tabPage->textEdit()->setFocus();
    }

    pauseAutoSaving(false);
}

/*************************/
void TexxyWindow::setWinTitle(const QString& title) {
    setWindowTitle(title);
    if (!ui->menuBar->isHidden()) {
        if (auto mbt = qobject_cast<MenuBarTitle*>(ui->menuBar->cornerWidget()))
            mbt->setTitle(title);
    }
}

/*************************/
void TexxyWindow::setTitle(const QString& fileName, int tabIndex) {
    int index = tabIndex;
    if (index < 0)
        index = ui->tabWidget->currentIndex();

    bool isLink = false;
    bool hasFinalTarget = false;
    QString shownName;

    if (fileName.isEmpty()) {
        shownName = tr("Untitled");
        if (tabIndex < 0)
            setWinTitle(shownName);
    }
    else {
        const QFileInfo fInfo(fileName);
        if (tabIndex < 0)
            setWinTitle(fileName.contains(QLatin1Char('/')) ? fileName
                                                            : fInfo.absolutePath() + QLatin1Char('/') + fileName);
        isLink = fInfo.isSymLink();
        if (!isLink) {
            const QString finalTarget = fInfo.canonicalFilePath();
            hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != fileName);
        }
        shownName = fileName.section(QLatin1Char('/'), -1);
        shownName.replace(QLatin1Char('\n'), QLatin1Char(' '));
    }

    if (sidePane_ && !sideItems_.isEmpty()) {
        if (auto* wi = sideItems_.key(qobject_cast<TabPage*>(ui->tabWidget->widget(index)))) {
            wi->setText(shownName);
            if (isLink)
                wi->setIcon(QIcon(QStringLiteral(":icons/link.svg")));
            else if (hasFinalTarget)
                wi->setIcon(QIcon(QStringLiteral(":icons/hasTarget.svg")));
            else
                wi->setIcon(QIcon());
        }
    }

    shownName.replace(QLatin1Char('&'), QStringLiteral("&&"));
    shownName.replace(QLatin1Char('\t'), QLatin1Char(' '));
    ui->tabWidget->setTabText(index, shownName);
    if (isLink)
        ui->tabWidget->setTabIcon(index, QIcon(QStringLiteral(":icons/link.svg")));
    else if (hasFinalTarget)
        ui->tabWidget->setTabIcon(index, QIcon(QStringLiteral(":icons/hasTarget.svg")));
    else
        ui->tabWidget->setTabIcon(index, QIcon());
}

/*************************/
void TexxyWindow::changeTab(QListWidgetItem* current) {
    if (!sidePane_ || sideItems_.isEmpty())
        return;
    ui->tabWidget->setCurrentWidget(sideItems_.value(current));
}

/*************************/
void TexxyWindow::onTabChanged(int index) {
    if (index > -1) {
        const QString fname = qobject_cast<TabPage*>(ui->tabWidget->widget(index))->textEdit()->getFileName();
        if (fname.isEmpty() || QFile::exists(fname))
            closeWarningBar();
    }
    else {
        closeWarningBar();
    }
}

/*************************/
void TexxyWindow::tabSwitch(int index) {
    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index));
    if (tabPage == nullptr) {
        setWindowTitle(QStringLiteral("Texxy[*]"));
        if (auto* label = qobject_cast<QLabel*>(ui->menuBar->cornerWidget()))
            label->clear();
        setWindowModified(false);
        return;
    }

    QPointer<TextEdit> textEdit = tabPage->textEdit();
    if (!tabPage->isSearchBarVisible() && !sidePane_)
        textEdit->setFocus();

    const QString fname = textEdit->getFileName();
    const bool modified = textEdit->document()->isModified();

    QFileInfo info;
    QString shownName;
    if (fname.isEmpty()) {
        shownName = (textEdit->getProg() == QStringLiteral("help"))
                        ? QStringLiteral("** ") + tr("Help") + QStringLiteral(" **")
                        : tr("Untitled");
    }
    else {
        info.setFile(fname);
        shownName = (fname.contains(QLatin1Char('/')) ? fname : info.absolutePath() + QLatin1Char('/') + fname);
        if (!QFile::exists(fname))
            onOpeningNonexistent();
        else if (textEdit->getLastModified() != info.lastModified())
            showWarningBar(
                QStringLiteral("<center><b><big>") + tr("This file has been modified elsewhere or in another way!") +
                    QStringLiteral("</big></b></center>\n<center>") +
                    tr("Please be careful about reloading or saving this document!") + QStringLiteral("</center>"),
                15);
    }
    if (modified)
        shownName.prepend(QLatin1Char('*'));
    setWinTitle(shownName);

    encodingToCheck(textEdit->getEncoding());

    const Config config = static_cast<TexxyApplication*>(qApp)->getConfig();

    ui->actionUndo->setEnabled(textEdit->document()->isUndoAvailable());
    ui->actionRedo->setEnabled(textEdit->document()->isRedoAvailable());

    const bool readOnly = textEdit->isReadOnly();
    if (!config.getSaveUnmodified())
        ui->actionSave->setEnabled(modified);
    else
        ui->actionSave->setDisabled(readOnly || textEdit->isUneditable());

    ui->actionReload->setEnabled(!fname.isEmpty());
    if (fname.isEmpty() && !modified && !textEdit->document()->isEmpty()) {
        ui->actionEdit->setVisible(false);
        ui->actionSaveAs->setEnabled(true);
        ui->actionSaveCodec->setEnabled(true);
    }
    else {
        ui->actionEdit->setVisible(readOnly && !textEdit->isUneditable());
        ui->actionSaveAs->setEnabled(!textEdit->isUneditable());
        ui->actionSaveCodec->setEnabled(!textEdit->isUneditable());
    }

    ui->actionPaste->setEnabled(!readOnly);
    ui->actionSoftTab->setEnabled(!readOnly);
    ui->actionDate->setEnabled(!readOnly);

    const bool textIsSelected = textEdit->textCursor().hasSelection();
    const bool hasColumn = !textEdit->getColSel().isEmpty();
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

    if (ui->spinBox->isVisible())
        ui->spinBox->setMaximum(textEdit->document()->blockCount());

    if (ui->statusBar->isVisible()) {
        statusMsgWithLineCount(textEdit->document()->blockCount());
        if (auto* wordButton = ui->statusBar->findChild<QToolButton*>(QStringLiteral("wordButton"))) {
            if (textEdit->getWordNumber() == -1) {
                wordButton->setVisible(true);
                if (textEdit->document()->isEmpty())
                    updateWordInfo();
            }
            else {
                wordButton->setVisible(false);
                auto* statusLabel = ui->statusBar->findChild<QLabel*>(QStringLiteral("statusLabel"));
                statusLabel->setText(QStringLiteral("%1 <i>%2</i>")
                                         .arg(statusLabel->text(), locale().toString(textEdit->getWordNumber())));
            }
        }
        showCursorPos();
    }

    if (config.getShowLangSelector() && config.getSyntaxByDefault())
        updateLangBtn(textEdit);

    if (ui->dockReplace->isVisible()) {
        const QString title = textEdit->getReplaceTitle();
        ui->dockReplace->setWindowTitle(title.isEmpty() ? tr("Replacement") : title);
    }
    else {
        textEdit->setReplaceTitle(QString());
    }
}

/*************************/
TexxyWindow::DOCSTATE TexxyWindow::savePrompt(int tabIndex,
                                              bool noToAll,
                                              int first,
                                              int last,
                                              bool closingWindow,
                                              QListWidgetItem* curItem,
                                              TabPage* curPage) {
    DOCSTATE state = SAVED;
    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(tabIndex));
    if (tabPage == nullptr)
        return state;

    QPointer<TextEdit> textEdit = tabPage->textEdit();
    const QString fname = textEdit->getFileName();
    const bool isRemoved = (!fname.isEmpty() && !QFile::exists(fname));  // don't check QFileInfo(fname).isFile()

    if (textEdit->document()->isModified() || isRemoved) {
        unbusy();                  // made busy at closePages
        if (hasAnotherDialog()) {  // cancel
            closePreviousPages_ = false;
            return UNDECIDED;
        }

        if (tabIndex != ui->tabWidget->currentIndex()) {  // switch to the page that needs attention
            if (sidePane_ && !sideItems_.isEmpty())
                sidePane_->listWidget()->setCurrentItem(sideItems_.key(tabPage));  // sets current widget at changeTab
            else
                ui->tabWidget->setCurrentIndex(tabIndex);
        }

        updateShortcuts(true);

        MessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setText(QStringLiteral("<center><b><big>") + tr("Save changes?") +
                       QStringLiteral("</big></b></center>"));
        if (isRemoved)
            msgBox.setInformativeText(QStringLiteral("<center><i>") + tr("The file does not exist.") +
                                      QStringLiteral("</i></center>"));
        else
            msgBox.setInformativeText(QStringLiteral("<center><i>") + tr("The document has been modified.") +
                                      QStringLiteral("</i></center>"));
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

        switch (msgBox.exec()) {
            case QMessageBox::Save:
                if (!saveFile(true, first, last, closingWindow, curItem, curPage)) {
                    state = UNDECIDED;
                    // closePreviousPages_ is set to false by saveFile when there is no root saving
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
void TexxyWindow::enableWidgets(bool enable) const {
    if (!enable && ui->dockReplace->isVisible())
        ui->dockReplace->setVisible(false);

    if (!enable && ui->spinBox->isVisible()) {
        ui->spinBox->setVisible(false);
        ui->label->setVisible(false);
        ui->checkBox->setVisible(false);
    }

    if ((!enable && ui->statusBar->isVisible()) ||
        (enable && static_cast<TexxyApplication*>(qApp)->getConfig().getShowStatusbar())) {
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

void TexxyWindow::toggleSidePane() {
    Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();
    if (sidePane_ == nullptr) {
        sidePane_ = new SidePane();
        ui->splitter->insertWidget(0, sidePane_);
        sidePane_->listWidget()->setFocus();
        ui->splitter->setStretchFactor(1, 1);  // only the text view can be

        connect(sidePane_, &SidePane::openFileRequested, this, [this](const QString& path) {
            const bool multiple = true;  // same behavior as opening several at once
            newTabFromName(path, 0, 0, multiple);
        });

        sidePane_->listWidget()->setFocus();

        QList<int> sizes;
        if (config.getRemSplitterPos()) {
            // make sure the side pane is visible and not wider than half the window
            sizes.append(std::min(std::max(16, config.getSplitterPos()), size().width() / 2));
            sizes.append(100);  // arbitrary because of stretching
        }
        else {
            // don't let the side pane be wider than 1/5 of the window width
            const int s =
                std::min(size().width() / 5, 40 * sidePane_->fontMetrics().horizontalAdvance(QLatin1Char(' ')));
            sizes << s << size().width() - s;
        }
        ui->splitter->setSizes(sizes);
        connect(sidePane_->listWidget(), &QWidget::customContextMenuRequested, this, &TexxyWindow::listContextMenu);
        connect(sidePane_->listWidget(), &ListWidget::currentItemUpdated, this, &TexxyWindow::changeTab);
        connect(sidePane_->listWidget(), &ListWidget::closeSidePane, this, &TexxyWindow::toggleSidePane);
        connect(sidePane_->listWidget(), &ListWidget::closeItem, [this](QListWidgetItem* item) {
            if (!sideItems_.isEmpty())
                closeTabAtIndex(ui->tabWidget->indexOf(sideItems_.value(item)));
        });

        if (ui->tabWidget->count() > 0) {
            updateShortcuts(true);
            const int curIndex = ui->tabWidget->currentIndex();
            auto* lw = sidePane_->listWidget();
            for (int i = 0; i < ui->tabWidget->count(); ++i) {
                auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
                QString fname = tabPage->textEdit()->getFileName();
                bool isLink = false;
                bool hasFinalTarget = false;
                if (fname.isEmpty()) {
                    fname = (tabPage->textEdit()->getProg() == QStringLiteral("help"))
                                ? QStringLiteral("** ") + tr("Help") + QStringLiteral(" **")
                                : tr("Untitled");
                }
                else {
                    const QFileInfo info(fname);
                    isLink = info.isSymLink();
                    if (!isLink) {
                        const QString finalTarget = info.canonicalFilePath();
                        hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != fname);
                    }
                    fname = fname.section(QLatin1Char('/'), -1);
                }
                if (tabPage->textEdit()->document()->isModified())
                    fname.append(QLatin1Char('*'));
                fname.replace(QLatin1Char('\n'), QLatin1Char(' '));
                auto* lwi = new ListWidgetItem(isLink           ? QIcon(QStringLiteral(":icons/link.svg"))
                                               : hasFinalTarget ? QIcon(QStringLiteral(":icons/hasTarget.svg"))
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
        const QString txt = ui->actionFirstTab->text();
        ui->actionFirstTab->setText(ui->actionLastTab->text());
        ui->actionLastTab->setText(txt);
        connect(ui->actionFirstTab, &QAction::triggered, this, &TexxyWindow::lastTab);
        connect(ui->actionLastTab, &QAction::triggered, this, &TexxyWindow::firstTab);
    }
    else {
        const QList<int> sizes = ui->splitter->sizes();
        if (config.getRemSplitterPos())  // remember the position also when the side pane is removed
            config.setSplitterPos(sizes.at(0));
        sideItems_.clear();
        delete sidePane_;
        sidePane_ = nullptr;
        const bool hideSingleTab = config.getHideSingleTab();
        ui->tabWidget->tabBar()->hideSingle(hideSingleTab);
        if (!hideSingleTab || ui->tabWidget->count() > 1)
            ui->tabWidget->tabBar()->show();
        // return focus to the document
        if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
            tabPage->textEdit()->setFocus();

        disconnect(ui->actionLastTab, nullptr, this, nullptr);
        disconnect(ui->actionFirstTab, nullptr, this, nullptr);
        const QString txt = ui->actionFirstTab->text();
        ui->actionFirstTab->setText(ui->actionLastTab->text());
        ui->actionLastTab->setText(txt);
        connect(ui->actionLastTab, &QAction::triggered, this, &TexxyWindow::lastTab);
        connect(ui->actionFirstTab, &QAction::triggered, this, &TexxyWindow::firstTab);
    }
}

void TexxyWindow::nextTab() {
    if (isLoading())
        return;

    int index = ui->tabWidget->currentIndex();
    if (index == -1)
        return;

    if (sidePane_) {
        int curRow = sidePane_->listWidget()->currentRow();
        if (curRow < 0 && !sideItems_.isEmpty()) {
            if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index))) {
                if (auto* wi = sideItems_.key(tabPage))
                    curRow = sidePane_->listWidget()->row(wi);
            }
        }
        if (curRow == sidePane_->listWidget()->count() - 1) {
            if (static_cast<TexxyApplication*>(qApp)->getConfig().getTabWrapAround())
                sidePane_->listWidget()->setCurrentRow(0);
        }
        else {
            sidePane_->listWidget()->setCurrentRow(curRow + 1);
        }
    }
    else {
        if (QWidget* widget = ui->tabWidget->widget(index + 1))
            ui->tabWidget->setCurrentWidget(widget);
        else if (static_cast<TexxyApplication*>(qApp)->getConfig().getTabWrapAround())
            ui->tabWidget->setCurrentIndex(0);
    }
}

void TexxyWindow::previousTab() {
    if (isLoading())
        return;

    int index = ui->tabWidget->currentIndex();
    if (index == -1)
        return;

    if (sidePane_) {
        int curRow = sidePane_->listWidget()->currentRow();
        if (curRow < 0 && !sideItems_.isEmpty()) {
            if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index))) {
                if (auto* wi = sideItems_.key(tabPage))
                    curRow = sidePane_->listWidget()->row(wi);
            }
        }
        if (curRow == 0) {
            if (static_cast<TexxyApplication*>(qApp)->getConfig().getTabWrapAround())
                sidePane_->listWidget()->setCurrentRow(sidePane_->listWidget()->count() - 1);
        }
        else {
            sidePane_->listWidget()->setCurrentRow(curRow - 1);
        }
    }
    else {
        if (QWidget* widget = ui->tabWidget->widget(index - 1))
            ui->tabWidget->setCurrentWidget(widget);
        else if (static_cast<TexxyApplication*>(qApp)->getConfig().getTabWrapAround()) {
            const int count = ui->tabWidget->count();
            if (count > 0)
                ui->tabWidget->setCurrentIndex(count - 1);
        }
    }
}

void TexxyWindow::lastTab() {
    if (isLoading())
        return;

    if (sidePane_) {
        const int count = sidePane_->listWidget()->count();
        if (count > 0)
            sidePane_->listWidget()->setCurrentRow(count - 1);
    }
    else {
        const int count = ui->tabWidget->count();
        if (count > 0)
            ui->tabWidget->setCurrentIndex(count - 1);
    }
}

void TexxyWindow::firstTab() {
    if (isLoading())
        return;

    if (sidePane_) {
        if (sidePane_->listWidget()->count() > 0)
            sidePane_->listWidget()->setCurrentRow(0);
    }
    else if (ui->tabWidget->count() > 0) {
        ui->tabWidget->setCurrentIndex(0);
    }
}

void TexxyWindow::lastActiveTab() {
    if (sidePane_) {
        if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->getLastActiveTab())) {
            if (auto* wi = sideItems_.key(tabPage))
                sidePane_->listWidget()->setCurrentItem(wi);
        }
    }
    else {
        ui->tabWidget->selectLastActiveTab();
    }
}

void TexxyWindow::detachTab() {
    if (!isReady())
        return;

    int index = -1;
    if (sidePane_ && rightClicked_ >= 0)
        index = ui->tabWidget->indexOf(sideItems_.value(sidePane_->listWidget()->item(rightClicked_)));
    else
        index = ui->tabWidget->currentIndex();

    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index));
    if (tabPage == nullptr || ui->tabWidget->count() == 1) {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }

    const Config config = static_cast<TexxyApplication*>(qApp)->getConfig();

    // capture UI state
    const QString tooltip = ui->tabWidget->tabToolTip(index);
    const QString tabText = ui->tabWidget->tabText(index);
    const QString title = windowTitle();
    bool hl = ui->actionSyntax->isChecked();
    const bool spin = ui->spinBox->isVisible();
    const bool ln = ui->actionLineNumbers->isChecked();
    const bool status = ui->statusBar->isVisible();
    const bool statusCurPos = status && ui->statusBar->findChild<QLabel*>(QStringLiteral("posLabel"));

    QPointer<TextEdit> textEdit = tabPage->textEdit();

    // disconnect from this window before moving the widget
    disconnect(textEdit, &TextEdit::resized, this, &TexxyWindow::hlight);
    disconnect(textEdit, &TextEdit::updateRect, this, &TexxyWindow::hlight);
    disconnect(textEdit, &QPlainTextEdit::textChanged, this, &TexxyWindow::hlight);
    if (status) {
        disconnect(textEdit, &QPlainTextEdit::blockCountChanged, this, &TexxyWindow::statusMsgWithLineCount);
        disconnect(textEdit, &TextEdit::selChanged, this, &TexxyWindow::statusMsg);
        if (statusCurPos)
            disconnect(textEdit, &QPlainTextEdit::cursorPositionChanged, this, &TexxyWindow::showCursorPos);
    }
    disconnect(textEdit, &TextEdit::canCopy, ui->actionCut, &QAction::setEnabled);
    disconnect(textEdit, &TextEdit::canCopy, ui->actionDelete, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionUpperCase, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionLowerCase, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionStartCase, &QAction::setEnabled);
    disconnect(textEdit, &TextEdit::canCopy, ui->actionCopy, &QAction::setEnabled);
    disconnect(textEdit, &QWidget::customContextMenuRequested, this, &TexxyWindow::editorContextMenu);
    disconnect(textEdit, &TextEdit::zoomedOut, this, &TexxyWindow::reformat);
    disconnect(textEdit, &TextEdit::hugeColumn, this, &TexxyWindow::columnWarning);
    disconnect(textEdit, &TextEdit::filePasted, this, &TexxyWindow::newTabFromName);
    disconnect(textEdit, &TextEdit::updateBracketMatching, this, &TexxyWindow::matchBrackets);
    disconnect(textEdit, &QPlainTextEdit::blockCountChanged, this, &TexxyWindow::formatOnBlockChange);
    disconnect(textEdit, &TextEdit::updateRect, this, &TexxyWindow::formatTextRect);
    disconnect(textEdit, &TextEdit::resized, this, &TexxyWindow::formatTextRect);

    disconnect(textEdit->document(), &QTextDocument::contentsChange, this, &TexxyWindow::updateWordInfo);
    disconnect(textEdit->document(), &QTextDocument::contentsChange, this, &TexxyWindow::formatOnTextChange);
    disconnect(textEdit->document(), &QTextDocument::blockCountChanged, this, &TexxyWindow::setMax);
    disconnect(textEdit->document(), &QTextDocument::modificationChanged, this, &TexxyWindow::asterisk);
    disconnect(textEdit->document(), &QTextDocument::undoAvailable, ui->actionUndo, &QAction::setEnabled);
    disconnect(textEdit->document(), &QTextDocument::redoAvailable, ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        disconnect(textEdit->document(), &QTextDocument::modificationChanged, this, &TexxyWindow::enableSaving);

    disconnect(tabPage, &TabPage::find, this, &TexxyWindow::find);
    disconnect(tabPage, &TabPage::searchFlagChanged, this, &TexxyWindow::searchFlagChanged);

    // for tabbar to be updated properly with tab reordering during a fast drag-and-drop, mouse should be released
    ui->tabWidget->tabBar()->releaseMouse();

    ui->tabWidget->removeTab(index);
    if (ui->tabWidget->count() == 1)
        updateGUIForSingleTab(true);
    if (sidePane_ && !sideItems_.isEmpty()) {
        if (auto* wi = sideItems_.key(tabPage)) {
            sideItems_.remove(wi);
            delete sidePane_->listWidget()->takeItem(sidePane_->listWidget()->row(wi));
        }
    }

    // create a new window and replace its tab by this widget
    auto* singleton = static_cast<TexxyApplication*>(qApp);
    TexxyWindow* dropTarget = singleton->newWin();

    // remove the single empty tab, as in closeTabAtIndex
    dropTarget->deleteTabPage(0, false, false);
    dropTarget->ui->actionReload->setDisabled(true);
    dropTarget->ui->actionSave->setDisabled(true);
    dropTarget->enableWidgets(false);

    // first, set the new info
    dropTarget->lastFile_ = textEdit->getFileName();
    textEdit->setGreenSel(QList<QTextEdit::ExtraSelection>());
    textEdit->setRedSel(QList<QTextEdit::ExtraSelection>());

    // then insert the detached widget
    dropTarget->enableWidgets(true);  // the tab will be inserted and switched to below
    const QFileInfo lastFileInfo(dropTarget->lastFile_);
    const bool isLink = dropTarget->lastFile_.isEmpty() ? false : lastFileInfo.isSymLink();
    bool hasFinalTarget = false;
    if (!isLink) {
        const QString finalTarget = lastFileInfo.canonicalFilePath();
        hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != dropTarget->lastFile_);
    }
    dropTarget->ui->tabWidget->insertTab(0, tabPage,
                                         isLink           ? QIcon(QStringLiteral(":icons/link.svg"))
                                         : hasFinalTarget ? QIcon(QStringLiteral(":icons/hasTarget.svg"))
                                                          : QIcon(),
                                         tabText);

    if (dropTarget->sidePane_) {
        auto* lw = dropTarget->sidePane_->listWidget();
        QString fname = textEdit->getFileName();
        if (fname.isEmpty()) {
            fname = (textEdit->getProg() == QStringLiteral("help"))
                        ? QStringLiteral("** ") + tr("Help") + QStringLiteral(" **")
                        : tr("Untitled");
        }
        else {
            fname = fname.section(QLatin1Char('/'), -1);
        }
        if (textEdit->document()->isModified())
            fname.append(QLatin1Char('*'));
        fname.replace(QLatin1Char('\n'), QLatin1Char(' '));
        auto* lwi = new ListWidgetItem(isLink           ? QIcon(QStringLiteral(":icons/link.svg"))
                                       : hasFinalTarget ? QIcon(QStringLiteral(":icons/hasTarget.svg"))
                                                        : QIcon(),
                                       fname, lw);
        lw->setToolTip(tooltip);
        dropTarget->sideItems_.insert(lwi, tabPage);
        lw->addItem(lwi);
        lw->setCurrentItem(lwi);
    }

    // remove all yellow and green highlights
    QList<QTextEdit::ExtraSelection> es;
    if (ln || spin)
        es.prepend(textEdit->currentLineSelection());
    es.append(textEdit->getBlueSel());
    textEdit->setExtraSelections(es);

    // set all properties correctly
    dropTarget->setWinTitle(title);
    dropTarget->ui->tabWidget->setTabToolTip(0, tooltip);
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
        connect(textEdit->document(), &QTextDocument::blockCountChanged, dropTarget, &TexxyWindow::setMax);
    }
    if (ln)
        dropTarget->ui->actionLineNumbers->setChecked(true);

    // searching
    if (!textEdit->getSearchedText().isEmpty()) {
        connect(textEdit, &QPlainTextEdit::textChanged, dropTarget, &TexxyWindow::hlight);
        connect(textEdit, &TextEdit::updateRect, dropTarget, &TexxyWindow::hlight);
        connect(textEdit, &TextEdit::resized, dropTarget, &TexxyWindow::hlight);
        dropTarget->hlight();
    }

    // status bar
    if (status) {
        dropTarget->ui->statusBar->setVisible(true);
        dropTarget->statusMsgWithLineCount(textEdit->document()->blockCount());
        if (textEdit->getWordNumber() == -1) {
            if (auto* wordButton = dropTarget->ui->statusBar->findChild<QToolButton*>(QStringLiteral("wordButton")))
                wordButton->setVisible(true);
        }
        else {
            if (auto* wordButton = dropTarget->ui->statusBar->findChild<QToolButton*>(QStringLiteral("wordButton")))
                wordButton->setVisible(false);
            auto* statusLabel = dropTarget->ui->statusBar->findChild<QLabel*>(QStringLiteral("statusLabel"));
            statusLabel->setText(
                QStringLiteral("%1 <i>%2</i>").arg(statusLabel->text(), locale().toString(textEdit->getWordNumber())));
            connect(textEdit->document(), &QTextDocument::contentsChange, dropTarget, &TexxyWindow::updateWordInfo);
        }
        connect(textEdit, &QPlainTextEdit::blockCountChanged, dropTarget, &TexxyWindow::statusMsgWithLineCount);
        connect(textEdit, &TextEdit::selChanged, dropTarget, &TexxyWindow::statusMsg);
        if (statusCurPos) {
            dropTarget->addCursorPosLabel();
            dropTarget->showCursorPos();
            connect(textEdit, &QPlainTextEdit::cursorPositionChanged, dropTarget, &TexxyWindow::showCursorPos);
        }
    }

    if (textEdit->lineWrapMode() == QPlainTextEdit::NoWrap)
        dropTarget->ui->actionWrap->setChecked(false);
    if (!textEdit->getAutoIndentation())
        dropTarget->ui->actionIndent->setChecked(false);

    // the remaining signals
    connect(textEdit->document(), &QTextDocument::undoAvailable, dropTarget->ui->actionUndo, &QAction::setEnabled);
    connect(textEdit->document(), &QTextDocument::redoAvailable, dropTarget->ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        connect(textEdit->document(), &QTextDocument::modificationChanged, dropTarget, &TexxyWindow::enableSaving);
    connect(textEdit->document(), &QTextDocument::modificationChanged, dropTarget, &TexxyWindow::asterisk);
    connect(textEdit, &TextEdit::canCopy, dropTarget->ui->actionCopy, &QAction::setEnabled);

    connect(tabPage, &TabPage::find, dropTarget, &TexxyWindow::find);
    connect(tabPage, &TabPage::searchFlagChanged, dropTarget, &TexxyWindow::searchFlagChanged);

    if (!textEdit->isReadOnly()) {
        connect(textEdit, &TextEdit::canCopy, dropTarget->ui->actionCut, &QAction::setEnabled);
        connect(textEdit, &TextEdit::canCopy, dropTarget->ui->actionDelete, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, dropTarget->ui->actionUpperCase, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, dropTarget->ui->actionLowerCase, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, dropTarget->ui->actionStartCase, &QAction::setEnabled);
    }

    connect(textEdit, &TextEdit::filePasted, dropTarget, &TexxyWindow::newTabFromName);
    connect(textEdit, &TextEdit::zoomedOut, dropTarget, &TexxyWindow::reformat);
    connect(textEdit, &TextEdit::hugeColumn, dropTarget, &TexxyWindow::columnWarning);
    connect(textEdit, &QWidget::customContextMenuRequested, dropTarget, &TexxyWindow::editorContextMenu);

    textEdit->setFocus();
    dropTarget->stealFocus();
}

void TexxyWindow::dropTab(const QString& str, QObject* source) {
    auto* w = qobject_cast<QWidget*>(source);
    if (w == nullptr || str.isEmpty()) {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }
    const int index = str.toInt();
    if (index <= -1) {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }

    auto* dragSource = qobject_cast<TexxyWindow*>(w->window());
    if (dragSource == this || dragSource == nullptr) {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }

    closeWarningBar();
    dragSource->closeWarningBar();

    auto* tabPage = qobject_cast<TabPage*>(dragSource->ui->tabWidget->widget(index));
    if (tabPage == nullptr) {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }
    QPointer<TextEdit> textEdit = tabPage->textEdit();

    const QString tooltip = dragSource->ui->tabWidget->tabToolTip(index);
    const QString tabText = dragSource->ui->tabWidget->tabText(index);
    const bool spin = dragSource->ui->spinBox->isVisible();
    const bool ln = dragSource->ui->actionLineNumbers->isChecked();

    const Config config = static_cast<TexxyApplication*>(qApp)->getConfig();

    // disconnect from source window
    disconnect(textEdit, &TextEdit::resized, dragSource, &TexxyWindow::hlight);
    disconnect(textEdit, &TextEdit::updateRect, dragSource, &TexxyWindow::hlight);
    disconnect(textEdit, &QPlainTextEdit::textChanged, dragSource, &TexxyWindow::hlight);
    if (dragSource->ui->statusBar->isVisible()) {
        disconnect(textEdit, &QPlainTextEdit::blockCountChanged, dragSource, &TexxyWindow::statusMsgWithLineCount);
        disconnect(textEdit, &TextEdit::selChanged, dragSource, &TexxyWindow::statusMsg);
        if (dragSource->ui->statusBar->findChild<QLabel*>(QStringLiteral("posLabel")))
            disconnect(textEdit, &QPlainTextEdit::cursorPositionChanged, dragSource, &TexxyWindow::showCursorPos);
    }
    disconnect(textEdit, &TextEdit::canCopy, dragSource->ui->actionCut, &QAction::setEnabled);
    disconnect(textEdit, &TextEdit::canCopy, dragSource->ui->actionDelete, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, dragSource->ui->actionUpperCase, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, dragSource->ui->actionLowerCase, &QAction::setEnabled);
    disconnect(textEdit, &QPlainTextEdit::copyAvailable, dragSource->ui->actionStartCase, &QAction::setEnabled);
    disconnect(textEdit, &TextEdit::canCopy, dragSource->ui->actionCopy, &QAction::setEnabled);
    disconnect(textEdit, &QWidget::customContextMenuRequested, dragSource, &TexxyWindow::editorContextMenu);
    disconnect(textEdit, &TextEdit::zoomedOut, dragSource, &TexxyWindow::reformat);
    disconnect(textEdit, &TextEdit::hugeColumn, dragSource, &TexxyWindow::columnWarning);
    disconnect(textEdit, &TextEdit::filePasted, dragSource, &TexxyWindow::newTabFromName);
    disconnect(textEdit, &TextEdit::updateBracketMatching, dragSource, &TexxyWindow::matchBrackets);
    disconnect(textEdit, &QPlainTextEdit::blockCountChanged, dragSource, &TexxyWindow::formatOnBlockChange);
    disconnect(textEdit, &TextEdit::updateRect, dragSource, &TexxyWindow::formatTextRect);
    disconnect(textEdit, &TextEdit::resized, dragSource, &TexxyWindow::formatTextRect);

    disconnect(textEdit->document(), &QTextDocument::contentsChange, dragSource, &TexxyWindow::updateWordInfo);
    disconnect(textEdit->document(), &QTextDocument::contentsChange, dragSource, &TexxyWindow::formatOnTextChange);
    disconnect(textEdit->document(), &QTextDocument::blockCountChanged, dragSource, &TexxyWindow::setMax);
    disconnect(textEdit->document(), &QTextDocument::modificationChanged, dragSource, &TexxyWindow::asterisk);
    disconnect(textEdit->document(), &QTextDocument::undoAvailable, dragSource->ui->actionUndo, &QAction::setEnabled);
    disconnect(textEdit->document(), &QTextDocument::redoAvailable, dragSource->ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        disconnect(textEdit->document(), &QTextDocument::modificationChanged, dragSource, &TexxyWindow::enableSaving);

    disconnect(tabPage, &TabPage::find, dragSource, &TexxyWindow::find);
    disconnect(tabPage, &TabPage::searchFlagChanged, dragSource, &TexxyWindow::searchFlagChanged);

    // ensure the source tabbar updates correctly during fast dnd
    dragSource->ui->tabWidget->tabBar()->releaseMouse();

    dragSource->ui->tabWidget->removeTab(index);  // there can't be a side pane here
    const int count = dragSource->ui->tabWidget->count();
    if (count == 1)
        dragSource->updateGUIForSingleTab(true);

    // insert into this window
    int insertIndex = ui->tabWidget->currentIndex() + 1;

    // first, set the new info
    lastFile_ = textEdit->getFileName();
    textEdit->setGreenSel(QList<QTextEdit::ExtraSelection>());
    textEdit->setRedSel(QList<QTextEdit::ExtraSelection>());

    // ensure searchbar visibility is consistent
    if (!textEdit->getSearchedText().isEmpty()) {
        if (insertIndex == 0 || !qobject_cast<TabPage*>(ui->tabWidget->widget(insertIndex - 1))->isSearchBarVisible()) {
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

    const QFileInfo lastFileInfo(lastFile_);
    const bool isLink = lastFile_.isEmpty() ? false : lastFileInfo.isSymLink();
    bool hasFinalTarget = false;
    if (!isLink) {
        const QString finalTarget = lastFileInfo.canonicalFilePath();
        hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != lastFile_);
    }
    ui->tabWidget->insertTab(insertIndex, tabPage,
                             isLink           ? QIcon(QStringLiteral(":icons/link.svg"))
                             : hasFinalTarget ? QIcon(QStringLiteral(":icons/hasTarget.svg"))
                                              : QIcon(),
                             tabText);

    if (sidePane_) {
        auto* lw = sidePane_->listWidget();
        QString fname = textEdit->getFileName();
        if (fname.isEmpty()) {
            fname = (textEdit->getProg() == QStringLiteral("help"))
                        ? QStringLiteral("** ") + tr("Help") + QStringLiteral(" **")
                        : tr("Untitled");
        }
        else {
            fname = fname.section(QLatin1Char('/'), -1);
        }
        if (textEdit->document()->isModified())
            fname.append(QLatin1Char('*'));
        fname.replace(QLatin1Char('\n'), QLatin1Char(' '));
        auto* lwi = new ListWidgetItem(isLink           ? QIcon(QStringLiteral(":icons/link.svg"))
                                       : hasFinalTarget ? QIcon(QStringLiteral(":icons/hasTarget.svg"))
                                                        : QIcon(),
                                       fname, lw);
        lw->setToolTip(tooltip);
        sideItems_.insert(lwi, tabPage);
        lw->addItem(lwi);
        lw->setCurrentItem(lwi);
    }
    ui->tabWidget->setCurrentIndex(insertIndex);

    // remove all yellow and green highlights
    QList<QTextEdit::ExtraSelection> es;
    if ((ln || spin) && lineContextVisible())
        es.prepend(textEdit->currentLineSelection());
    es.append(textEdit->getBlueSel());
    textEdit->setExtraSelections(es);

    // set all properties correctly
    ui->tabWidget->setTabToolTip(insertIndex, tooltip);

    if (ui->actionSyntax->isChecked()) {
        makeBusy();  // may take a while with huge texts
        syntaxHighlighting(textEdit, true, textEdit->getLang());
        QTimer::singleShot(0, this, &TexxyWindow::unbusy);
    }
    else if (!ui->actionSyntax->isChecked() && textEdit->getHighlighter()) {
        textEdit->setDrawIndetLines(false);
        if (auto* hl = textEdit->getHighlighter())
            delete hl;
    }

    if (ui->spinBox->isVisible())
        connect(textEdit->document(), &QTextDocument::blockCountChanged, this, &TexxyWindow::setMax);
    textEdit->showLineNumbers(lineContextVisible());

    // searching
    if (!textEdit->getSearchedText().isEmpty()) {
        connect(textEdit, &QPlainTextEdit::textChanged, this, &TexxyWindow::hlight);
        connect(textEdit, &TextEdit::updateRect, this, &TexxyWindow::hlight);
        connect(textEdit, &TextEdit::resized, this, &TexxyWindow::hlight);
        hlight();
    }

    // status bar
    if (ui->statusBar->isVisible()) {
        connect(textEdit, &QPlainTextEdit::blockCountChanged, this, &TexxyWindow::statusMsgWithLineCount);
        connect(textEdit, &TextEdit::selChanged, this, &TexxyWindow::statusMsg);
        if (ui->statusBar->findChild<QLabel*>(QStringLiteral("posLabel"))) {
            showCursorPos();
            connect(textEdit, &QPlainTextEdit::cursorPositionChanged, this, &TexxyWindow::showCursorPos);
        }
        if (textEdit->getWordNumber() != -1)
            connect(textEdit->document(), &QTextDocument::contentsChange, this, &TexxyWindow::updateWordInfo);
    }

    if (ui->actionWrap->isChecked() && textEdit->lineWrapMode() == QPlainTextEdit::NoWrap)
        textEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    else if (!ui->actionWrap->isChecked() && textEdit->lineWrapMode() == QPlainTextEdit::WidgetWidth)
        textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

    if (ui->actionIndent->isChecked() && !textEdit->getAutoIndentation())
        textEdit->setAutoIndentation(true);
    else if (!ui->actionIndent->isChecked() && textEdit->getAutoIndentation())
        textEdit->setAutoIndentation(false);

    // remaining signals
    connect(textEdit->document(), &QTextDocument::undoAvailable, ui->actionUndo, &QAction::setEnabled);
    connect(textEdit->document(), &QTextDocument::redoAvailable, ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        connect(textEdit->document(), &QTextDocument::modificationChanged, this, &TexxyWindow::enableSaving);
    connect(textEdit->document(), &QTextDocument::modificationChanged, this, &TexxyWindow::asterisk);
    connect(textEdit, &TextEdit::canCopy, ui->actionCopy, &QAction::setEnabled);

    connect(tabPage, &TabPage::find, this, &TexxyWindow::find);
    connect(tabPage, &TabPage::searchFlagChanged, this, &TexxyWindow::searchFlagChanged);

    if (!textEdit->isReadOnly()) {
        connect(textEdit, &TextEdit::canCopy, ui->actionCut, &QAction::setEnabled);
        connect(textEdit, &TextEdit::canCopy, ui->actionDelete, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionUpperCase, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionLowerCase, &QAction::setEnabled);
        connect(textEdit, &QPlainTextEdit::copyAvailable, ui->actionStartCase, &QAction::setEnabled);
    }
    connect(textEdit, &TextEdit::filePasted, this, &TexxyWindow::newTabFromName);
    connect(textEdit, &TextEdit::zoomedOut, this, &TexxyWindow::reformat);
    connect(textEdit, &TextEdit::hugeColumn, this, &TexxyWindow::columnWarning);
    connect(textEdit, &QWidget::customContextMenuRequested, this, &TexxyWindow::editorContextMenu);

    textEdit->setFocus();
    stealFocus();

    if (count == 0)
        QTimer::singleShot(0, dragSource, &QWidget::close);
}

void TexxyWindow::tabContextMenu(const QPoint& p) {
    auto mbt = qobject_cast<MenuBarTitle*>(QObject::sender());
    rightClicked_ = mbt == nullptr ? ui->tabWidget->tabBar()->tabAt(p) : ui->tabWidget->currentIndex();
    if (rightClicked_ < 0)
        return;

    const QString fname = qobject_cast<TabPage*>(ui->tabWidget->widget(rightClicked_))->textEdit()->getFileName();
    QMenu menu(this);  // for Wayland when the window isn't active
    bool showMenu = false;

    if (mbt == nullptr) {
        const int tabNum = ui->tabWidget->count();
        if (tabNum > 1) {
            auto* labelAction = new QWidgetAction(&menu);
            auto* label = new QLabel(QStringLiteral("<center><b>") + tr("%1 Pages").arg(tabNum) +
                                     QStringLiteral("</b></center>"));
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
        const QFileInfo info(fname);
        const QString finalTarget = info.canonicalFilePath();
        bool hasFinalTarget = false;

        if (info.isSymLink()) {
            menu.addSeparator();
            const QString symTarget = info.symLinkTarget();
            hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != symTarget);

            QAction* action = menu.addAction(QIcon(QStringLiteral(":icons/link.svg")), tr("Copy Target Path"));
            connect(action, &QAction::triggered, [symTarget] { QApplication::clipboard()->setText(symTarget); });

            action = menu.addAction(QIcon(QStringLiteral(":icons/link.svg")), tr("Open Target Here"));
            connect(action, &QAction::triggered, this, [this, symTarget] {
                for (int i = 0; i < ui->tabWidget->count(); ++i) {
                    auto* thisTabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
                    if (symTarget == thisTabPage->textEdit()->getFileName()) {
                        ui->tabWidget->setCurrentWidget(thisTabPage);
                        return;
                    }
                }
                newTabFromName(symTarget, 0, 0);
            });
        }
        else {
            hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != fname);
        }

        if (hasFinalTarget) {
            menu.addSeparator();
            QAction* action =
                menu.addAction(QIcon(QStringLiteral(":icons/hasTarget.svg")), tr("Copy Final Target Path"));
            connect(action, &QAction::triggered, [finalTarget] { QApplication::clipboard()->setText(finalTarget); });

            action = menu.addAction(QIcon(QStringLiteral(":icons/hasTarget.svg")), tr("Open Final Target Here"));
            connect(action, &QAction::triggered, this, [this, finalTarget] {
                for (int i = 0; i < ui->tabWidget->count(); ++i) {
                    auto* thisTabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
                    if (finalTarget == thisTabPage->textEdit()->getFileName()) {
                        if (auto* wi = sideItems_.key(thisTabPage))
                            sidePane_->listWidget()->setCurrentItem(wi);
                        return;
                    }
                }
                newTabFromName(finalTarget, 0, 0);
            });
        }

        if (!static_cast<TexxyApplication*>(qApp)->isRoot() && QFile::exists(fname)) {
            menu.addSeparator();
            QAction* action = menu.addAction(static_cast<TexxyApplication*>(qApp)->getConfig().getSysIcons()
                                                 ? QIcon::fromTheme(QStringLiteral("folder"))
                                                 : symbolicIcon::icon(QStringLiteral(":icons/document-open.svg")),
                                             tr("Open Containing Folder"));
            connect(action, &QAction::triggered, this, [fname] {
                QDBusMessage methodCall = QDBusMessage::createMethodCall(
                    QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
                    QString(), QStringLiteral("ShowItems"));
                methodCall.setAutoStartService(false);  // switch to URL opening if service doesn't exist
                QList<QVariant> args;
                args.append(QStringList() << fname);
                args.append(QStringLiteral("0"));
                methodCall.setArguments(args);
                QDBusMessage response = QDBusConnection::sessionBus().call(methodCall, QDBus::Block, 1000);
                if (response.type() == QDBusMessage::ErrorMessage) {
                    const QString folder = fname.section(QLatin1Char('/'), 0, -2);
                    if (QStandardPaths::findExecutable(QStringLiteral("gio")).isEmpty() ||
                        !QProcess::startDetached(QStringLiteral("gio"), QStringList()
                                                                            << QStringLiteral("open") << folder)) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
                    }
                }
            });
        }
    }

    if (showMenu) {
        if (mbt == nullptr)
            menu.exec(ui->tabWidget->tabBar()->mapToGlobal(p));
        else
            menu.exec(mbt->mapToGlobal(p));
    }
    rightClicked_ = -1;  // reset
}

void TexxyWindow::listContextMenu(const QPoint& p) {
    if (!sidePane_ || sideItems_.isEmpty() || locked_)
        return;

    auto* lw = sidePane_->listWidget();
    const QModelIndex index = lw->indexAt(p);
    if (!index.isValid())
        return;

    auto* item = lw->getItemFromIndex(index);
    rightClicked_ = lw->row(item);
    const QString fname = sideItems_.value(item)->textEdit()->getFileName();

    QMenu menu(this);  // for Wayland when the window isn't active
    menu.addAction(ui->actionClose);

    if (lw->count() > 1) {
        auto* labelAction = new QWidgetAction(&menu);
        auto* label = new QLabel(QStringLiteral("<center><b>") + tr("%1 Pages").arg(lw->count()) +
                                 QStringLiteral("</b></center>"));
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
        if (!static_cast<TexxyApplication*>(qApp)->isStandAlone()) {
            menu.addSeparator();
            menu.addAction(ui->actionDetachTab);
        }
    }
    if (!fname.isEmpty()) {
        menu.addSeparator();
        menu.addAction(ui->actionCopyName);
        menu.addAction(ui->actionCopyPath);

        const QFileInfo info(fname);
        const QString finalTarget = info.canonicalFilePath();
        bool hasFinalTarget = false;

        if (info.isSymLink()) {
            menu.addSeparator();
            const QString symTarget = info.symLinkTarget();
            hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != symTarget);

            QAction* action = menu.addAction(QIcon(QStringLiteral(":icons/link.svg")), tr("Copy Target Path"));
            connect(action, &QAction::triggered, [symTarget] { QApplication::clipboard()->setText(symTarget); });

            action = menu.addAction(QIcon(QStringLiteral(":icons/link.svg")), tr("Open Target Here"));
            connect(action, &QAction::triggered, this, [this, symTarget] {
                for (int i = 0; i < this->ui->tabWidget->count(); ++i) {
                    auto* thisTabPage = qobject_cast<TabPage*>(this->ui->tabWidget->widget(i));
                    if (symTarget == thisTabPage->textEdit()->getFileName()) {
                        if (auto* wi = this->sideItems_.key(thisTabPage))
                            this->sidePane_->listWidget()->setCurrentItem(wi);  // sets the current widget at changeTab
                        return;
                    }
                }
                this->newTabFromName(symTarget, 0, 0);
            });
        }
        else {
            hasFinalTarget = (!finalTarget.isEmpty() && finalTarget != fname);
        }
        if (hasFinalTarget) {
            menu.addSeparator();
            QAction* action =
                menu.addAction(QIcon(QStringLiteral(":icons/hasTarget.svg")), tr("Copy Final Target Path"));
            connect(action, &QAction::triggered, [finalTarget] { QApplication::clipboard()->setText(finalTarget); });
            action = menu.addAction(QIcon(QStringLiteral(":icons/hasTarget.svg")), tr("Open Final Target Here"));
            connect(action, &QAction::triggered, this, [this, finalTarget] {
                for (int i = 0; i < this->ui->tabWidget->count(); ++i) {
                    auto* thisTabPage = qobject_cast<TabPage*>(this->ui->tabWidget->widget(i));
                    if (finalTarget == thisTabPage->textEdit()->getFileName()) {
                        if (auto* wi = this->sideItems_.key(thisTabPage))
                            this->sidePane_->listWidget()->setCurrentItem(wi);  // sets the current widget at changeTab
                        return;
                    }
                }
                this->newTabFromName(finalTarget, 0, 0);
            });
        }
        if (!static_cast<TexxyApplication*>(qApp)->isRoot() && QFile::exists(fname)) {
            menu.addSeparator();
            QAction* action = menu.addAction(static_cast<TexxyApplication*>(qApp)->getConfig().getSysIcons()
                                                 ? QIcon::fromTheme(QStringLiteral("folder"))
                                                 : symbolicIcon::icon(QStringLiteral(":icons/document-open.svg")),
                                             tr("Open Containing Folder"));
            connect(action, &QAction::triggered, this, [fname] {
                QDBusMessage methodCall = QDBusMessage::createMethodCall(
                    QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
                    QString(), QStringLiteral("ShowItems"));
                methodCall.setAutoStartService(false);
                QList<QVariant> args;
                args.append(QStringList() << fname);
                args.append(QStringLiteral("0"));
                methodCall.setArguments(args);
                QDBusMessage response = QDBusConnection::sessionBus().call(methodCall, QDBus::Block, 1000);
                if (response.type() == QDBusMessage::ErrorMessage) {
                    const QString folder = fname.section(QLatin1Char('/'), 0, -2);
                    if (QStandardPaths::findExecutable(QStringLiteral("gio")).isEmpty() ||
                        !QProcess::startDetached(QStringLiteral("gio"), QStringList()
                                                                            << QStringLiteral("open") << folder)) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
                    }
                }
            });
        }
    }
    menu.exec(lw->viewport()->mapToGlobal(p));
    rightClicked_ = -1;  // reset
}

}  // namespace Texxy
