// src/ui/window_sessions.cpp
/*
 * window_sessions.cpp
 */

#include "texxy_ui_prelude.h"

namespace Texxy {

void TexxyWindow::executeProcess() {
    QList<QDialog*> dialogs = findChildren<QDialog*>();
    for (int i = 0; i < dialogs.count(); ++i) {
        if (dialogs.at(i)->isModal())
            return;  // shortcut may work when there's a modal dialog
    }
    closeWarningBar();

    Config config = static_cast<TexxyApplication*>(qApp)->getConfig();
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
        connect(process, &QProcess::readyReadStandardOutput, this, &TexxyWindow::displayOutput);
        connect(process, &QProcess::readyReadStandardError, this, &TexxyWindow::displayError);
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

void TexxyWindow::exitProcess() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        if (QProcess* process = tabPage->findChild<QProcess*>(QString(), Qt::FindDirectChildrenOnly))
            process->kill();
    }
}

void TexxyWindow::displayOutput() {
    displayMessage(false);
}

void TexxyWindow::displayError() {
    displayMessage(true);
}

void TexxyWindow::manageSessions() {
    if (!isReady())
        return;

    /* first see whether the Sessions dialog is already open... */
    TexxyApplication* singleton = static_cast<TexxyApplication*>(qApp);
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

void TexxyWindow::pauseAutoSaving(bool pause) {
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

void TexxyWindow::startAutoSaving(bool start, int interval) {
    if (start) {
        if (!autoSaver_) {
            autoSaver_ = new QTimer(this);
            connect(autoSaver_, &QTimer::timeout, this, &TexxyWindow::autoSave);
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

void TexxyWindow::autoSave() {
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

}  // namespace Texxy
