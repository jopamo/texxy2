// src/ui/window_sessions.cpp

#include "texxy_ui_prelude.h"

namespace Texxy {

void TexxyWindow::executeProcess() {
    // block when a modal dialog is up so shortcuts don't leak through
    const auto dialogs = findChildren<QDialog*>();
    for (QDialog* d : dialogs) {
        if (d && d->isModal())
            return;  // shortcut may work when there's a modal dialog
    }
    closeWarningBar();

    auto* app = static_cast<TexxyApplication*>(qApp); // non-const so we can call non-const getConfig
    const Config config = app->getConfig();
    if (!config.getExecuteScripts())
        return;

    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    if (tabPage->findChild<QProcess*>(QString(), Qt::FindDirectChildrenOnly)) {
        showWarningBar(
            "<center><b><big>" + tr("Another process is running in this tab!") + "</big></b></center>"
            "<center><i>" + tr("Only one process is allowed per tab.") + "</i></center>",
            15);
        return;
    }

    const QString fName = tabPage->textEdit()->getFileName();
    if (!isScriptLang(tabPage->textEdit()->getProg()) || !QFileInfo(fName).isExecutable()) {
        ui->actionRun->setVisible(false);
        return;
    }

    auto* process = new QProcess(tabPage);
    process->setObjectName(fName);  // used in messages
    process->setProgram(QString()); // set later after command parsing
    process->setProcessChannelMode(QProcess::SeparateChannels); // keep stdout/stderr split
    process->setWorkingDirectory(QFileInfo(fName).absolutePath());

    connect(process, &QProcess::readyReadStandardOutput, this, &TexxyWindow::displayOutput);
    connect(process, &QProcess::readyReadStandardError, this, &TexxyWindow::displayError);
    connect(process, &QProcess::finished, process, &QObject::deleteLater);

    // build command line from config, supporting optional %f placeholder
    QString command = config.getExecuteCommand();
    if (!command.isEmpty()) {
        QStringList parts = QProcess::splitCommand(command);
        if (!parts.isEmpty()) {
            QString prog = parts.takeFirst();
            for (QString& p : parts)
                p.replace("%f", fName);

            bool hadPlaceholder = false;
            for (const QString& p : parts)
                if (p.contains(fName))
                    hadPlaceholder = true;
            if (!hadPlaceholder)
                parts << fName;

            process->start(prog, parts);
            return;
        }
    }

    // default to executing the file directly if no executeCommand configured
    process->start(fName, QStringList());
}

void TexxyWindow::exitProcess() {
    auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (!tabPage)
        return;

    if (auto* process = tabPage->findChild<QProcess*>(QString(), Qt::FindDirectChildrenOnly)) {
        process->kill();  // fast stop on user request
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

    // if a Sessions dialog already exists anywhere, focus it
    auto* singleton = static_cast<TexxyApplication*>(qApp);
    for (int i = 0; i < singleton->Wins.count(); ++i) {
        const auto dialogs = singleton->Wins.at(i)->findChildren<QDialog*>();
        for (QDialog* dialog : dialogs) {
            if (dialog && dialog->objectName() == "sessionDialog") {
                stealFocus(dialog);
                return;
            }
        }
    }

    // otherwise create a non modal Sessions dialog
    auto* dlg = new SessionDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setObjectName("sessionDialog");
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void TexxyWindow::pauseAutoSaving(bool pause) {
    if (!autoSaver_)
        return;

    if (pause) {
        if (!autoSaverPause_.isValid()) { // don't start it again
            autoSaverPause_.start();
            autoSaverRemainingTime_ = autoSaver_->remainingTime();
        }
        return;
    }

    if (!locked_ && autoSaverPause_.isValid()) {
        if (autoSaverPause_.hasExpired(autoSaverRemainingTime_)) {
            autoSaverPause_.invalidate();
            autoSave();
        } else {
            autoSaverPause_.invalidate();
        }
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
        return;
    }

    if (autoSaver_) {
        if (autoSaver_->isActive())
            autoSaver_->stop();
        delete autoSaver_;
        autoSaver_ = nullptr;
    }
}

void TexxyWindow::autoSave() {
    // different from saveFile so we avoid prompts or warnings
    if (autoSaverPause_.isValid())
        return;

    QTimer::singleShot(0, this, [this]() {
        if (!autoSaver_ || !autoSaver_->isActive())
            return;
        saveAllFiles(false);  // without warning
    });
}

}  // namespace Texxy
