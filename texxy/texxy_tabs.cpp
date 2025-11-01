#include "texxywindow.h"

#include "menubartitle.h"
#include "messagebox.h"
#include "singleton.h"
#include "tabbar.h"
#include "ui_texxywindow.h"

#include <QApplication>
#include <QClipboard>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QIcon>
#include <QToolButton>
#include <QTimer>
#include <QUrl>

namespace Texxy {

bool TexxyWindow::closePages(int first, int last, bool saveFilesList) {
    if (!isReady()) {
        closePreviousPages_ = false;
        return true;
    }

    // RAII guards for autosave pause and making sure we unbusy on all paths
    struct AutoPause {
        TexxyWindow* self;
        explicit AutoPause(TexxyWindow* s) : self(s) { self->pauseAutoSaving(true); }
        ~AutoPause() { self->pauseAutoSaving(false); }
    } autoPause(this);

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
void TexxyWindow::copyTabFileName() {
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
void TexxyWindow::copyTabFilePath() {
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
    /* NOTE: Because saving as root is possible, we can't close the previous pages
             here. They will be closed by closePages() if needed. */
    closePreviousPages_ = true;
    closePages(rightClicked_, -1);
}
/*************************/
void TexxyWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (locked_ || findChildren<QDialog*>().count() > 0)
        return;
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    /* check if this comes from one of our windows (and not from a root instance, for example) */
    else if (event->mimeData()->hasFormat("application/texxy-tab") && event->source() != nullptr) {
        event->acceptProposedAction();
    }
}
/*************************/
void TexxyWindow::dropEvent(QDropEvent* event) {
    if (locked_)
        return;
    if (event->mimeData()->hasFormat("application/texxy-tab")) {
        if (QObject* sourseObject = event->source()) {
            /* announce that the drop is accepted by us (see "TabBar::mouseMoveEvent") */
            sourseObject->setProperty(TabBar::tabDropped, true);
            /* the tab will be dropped after the DND is finished */
            auto data = event->mimeData()->data("application/texxy-tab");
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
    else {
        if (count == 1)
            updateGUIForSingleTab(true);

        if (curItem)
            sidePane_->listWidget()->setCurrentItem(curItem);

        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
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
        shownName.replace("\n", " ");
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

    shownName.replace("&", "&&");
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
void TexxyWindow::changeTab(QListWidgetItem* current) {
    if (!sidePane_ || sideItems_.isEmpty())
        return;
    ui->tabWidget->setCurrentWidget(sideItems_.value(current));
}
/*************************/
void TexxyWindow::onTabChanged(int index) {
    if (index > -1) {
        QString fname = qobject_cast<TabPage*>(ui->tabWidget->widget(index))->textEdit()->getFileName();
        if (fname.isEmpty() || QFile::exists(fname))
            closeWarningBar();
    }
    else
        closeWarningBar();
}
/*************************/
void TexxyWindow::tabSwitch(int index) {
    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(index));
    if (tabPage == nullptr) {
        setWindowTitle("Texxy[*]");
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

    encodingToCheck(textEdit->getEncoding());

    Config config = static_cast<TexxyApplication*>(qApp)->getConfig();

    ui->actionUndo->setEnabled(textEdit->document()->isUndoAvailable());
    ui->actionRedo->setEnabled(textEdit->document()->isRedoAvailable());
    bool readOnly = textEdit->isReadOnly();
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

    if (ui->spinBox->isVisible())
        ui->spinBox->setMaximum(textEdit->document()->blockCount());

    if (ui->statusBar->isVisible()) {
        statusMsgWithLineCount(textEdit->document()->blockCount());
        QToolButton* wordButton = ui->statusBar->findChild<QToolButton*>("wordButton");
        if (textEdit->getWordNumber() == -1) {
            if (wordButton)
                wordButton->setVisible(true);
            if (textEdit->document()->isEmpty())
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
TexxyWindow::DOCSTATE TexxyWindow::savePrompt(int tabIndex,
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
void TexxyWindow::enableWidgets(bool enable) const {
    if (!enable && ui->dockReplace->isVisible())
        ui->dockReplace->setVisible(false);
    if (!enable && ui->spinBox->isVisible()) {
        ui->spinBox->setVisible(false);
        ui->label->setVisible(false);
        ui->checkBox->setVisible(false);
    }
    if ((!enable && ui->statusBar->isVisible()) ||
        (enable && static_cast<TexxyApplication*>(qApp)->getConfig().getShowStatusbar()))  // starting from no tab
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

}  // namespace Texxy
