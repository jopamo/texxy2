// src/core/save.cpp
/*
 * texxy/save.cpp
 *
 * Saving-related functionality extracted from the legacy monolithic
 * implementation to keep `texxy.cpp` focused on window wiring.
 */

#include "texxywindow.h"
#include "ui_texxywindow.h"

#include "filedialog.h"
#include "messagebox.h"
#include "singleton.h"
#include "ui/tabpage.h"
#include "textedit/textedit.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QLabel>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentWriter>
#include <QTimer>
#include <QToolTip>
#include <QStringEncoder>

#include <algorithm>

namespace Texxy {

namespace {

int trailingSpaces(const QString& str) {
    int i = 0;
    while (i < str.length()) {
        if (!str.at(str.length() - 1 - i).isSpace())
            return i;
        ++i;
    }
    return i;
}

QStringEncoder getEncoder(const QString& encoding) {
    if (encoding.compare("UTF-16", Qt::CaseInsensitive) == 0) {
        return QStringEncoder(QStringConverter::Utf16, QStringConverter::Flag::WriteBom);
    }
    if (encoding.compare("UTF-8", Qt::CaseInsensitive) == 0)
        return QStringEncoder(QStringConverter::Utf8);
    if (encoding.compare("UTF-32", Qt::CaseInsensitive) == 0)
        return QStringEncoder(QStringConverter::Utf32);

    return QStringEncoder(QStringConverter::Latin1);
}

}  // namespace

void TexxyWindow::removeTrailingSpacesIfNeeded(TextEdit* textEdit) {
    const QString lang = textEdit->getFileName().isEmpty() ? textEdit->getLang() : textEdit->getProg();

    if (lang == QLatin1String("diff") || textEdit->getFileName().endsWith("/locale.gen"))
        return;

    makeBusy();

    QTextBlock block = textEdit->document()->firstBlock();
    QTextCursor cursor = textEdit->textCursor();
    cursor.beginEditBlock();

    const bool doubleSpace = (lang == QLatin1String("markdown") || lang == QLatin1String("fountain"));
    const bool singleSpace = (lang == QLatin1String("LaTeX"));

    while (block.isValid()) {
        const int count = trailingSpaces(block.text());
        if (count > 0) {
            cursor.setPosition(block.position() + block.text().length());
            if (doubleSpace) {
                if (count != 2)
                    cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor,
                                        std::max(1, count - 2));
            }
            else if (singleSpace) {
                if (count > 1)
                    cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, count - 1);
            }
            else {
                cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, count);
            }
            cursor.removeSelectedText();
        }
        block = block.next();
    }
    cursor.endEditBlock();
    unbusy();
}

bool TexxyWindow::showSaveDialogAndSetFileName(QString& fname, const QString& filter, const QString& title) {
    if (hasAnotherDialog())
        return false;

    updateShortcuts(true);

    FileDialog dialog(this, static_cast<TexxyApplication*>(qApp)->getConfig().getNativeDialog());
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setWindowTitle(title);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilter(filter);

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

bool TexxyWindow::writeFileWithEncoding(const QString& fname, TextEdit* textEdit, bool& MSWinLineEnd) {
    const QString encoding = checkToEncoding();
    if (encoding == QLatin1String("UTF-16")) {
        MSWinLineEnd = true;
        return writeUtf16File(fname, textEdit);
    }

    return promptAndWriteWithChosenEOL(fname, textEdit, encoding, MSWinLineEnd);
}

bool TexxyWindow::writeUtf16File(const QString& fname, TextEdit* textEdit) {
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

bool TexxyWindow::promptAndWriteWithChosenEOL(const QString& fname,
                                              TextEdit* textEdit,
                                              const QString& encoding,
                                              bool& MSWinLineEnd) {
    QStringEncoder encoder = getEncoder(encoding);

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

void TexxyWindow::handleSaveFailure(const QString& fname) {
    closePreviousPages_ = false;

    const QString errorTitle = tr("Cannot be saved!");
    const QString errorInfo = tr("Could not save the file: %1").arg(fname);

    QTimer::singleShot(0, this, [this, errorTitle, errorInfo] {
        showWarningBar(QStringLiteral("<center><b><big>%1</big></b></center>\n<center><i>%2</i></center>")
                           .arg(errorTitle, errorInfo),
                       15);
    });
}

bool TexxyWindow::saveFile(bool keepSyntax,
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

    QString filter = tr("All Files") + QStringLiteral(" (*)");
    if (!fname.isEmpty()) {
        const QString ext = QFileInfo(fname).suffix();
        if (!ext.isEmpty())
            filter = QStringLiteral("*.%1;;").arg(ext) + tr("All Files") + QStringLiteral(" (*)");
    }
    else if (!lastFile_.isEmpty()) {
        fname = lastFile_;
    }

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

    Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();
    if (config.getRemoveTrailingSpaces())
        removeTrailingSpacesIfNeeded(textEdit);

    if (config.getAppendEmptyLine() && !textEdit->document()->lastBlock().text().isEmpty()) {
        QTextCursor cursor = textEdit->textCursor();
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::End);
        cursor.insertBlock();
        cursor.endEditBlock();
    }

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
    }

    if (!success) {
        handleSaveFailure(fname);
        return false;
    }

    const QFileInfo fi(fname);

    textEdit->document()->setModified(false);
    textEdit->setFileName(fname);
    textEdit->setSize(fi.size());
    textEdit->setLastModified(fi.lastModified());
    ui->actionReload->setDisabled(false);
    setTitle(fname);

    if (sidePane_)
        sidePane_->revealFile(fname);

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

    const QString targetEncoding = checkToEncoding();
    if (textEdit->getEncoding() != targetEncoding) {
        textEdit->setEncoding(targetEncoding);

        if (ui->statusBar->isVisible()) {
            if (auto* currentPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
                if (currentPage->textEdit() == textEdit) {
                    if (QLabel* statusLabel = ui->statusBar->findChild<QLabel*>(QStringLiteral("statusLabel"))) {
                        QString labelText = statusLabel->text();
                        const QString encodingToken = tr("Encoding");
                        const QString linesToken = QStringLiteral("</i>&nbsp;&nbsp;&nbsp;<b>%1").arg(tr("Lines"));
                        const int encodingIndex = labelText.indexOf(encodingToken);
                        const int linesIndex = labelText.indexOf(linesToken);
                        const int valueOffset = encodingToken.size() + 9;  // size of ":</b> <i>"
                        if (encodingIndex >= 0 && linesIndex > encodingIndex)
                            labelText.replace(encodingIndex + valueOffset, linesIndex - encodingIndex - valueOffset,
                                              targetEncoding);
                        statusLabel->setText(labelText);
                    }
                }
            }
        }
    }

    if (!keepSyntax) {
        reloadSyntaxHighlighter(textEdit);
    }

    if (textEdit->isReadOnly() && !alreadyOpen(curPage))
        QTimer::singleShot(0, this, &TexxyWindow::makeEditable);

    return true;
}

void TexxyWindow::saveAllFiles(bool showWarning) {
    const int currentIndex = ui->tabWidget->currentIndex();
    if (currentIndex == -1)
        return;

    Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();
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

        if (appendEmpty && !doc->lastBlock().text().isEmpty()) {
            QTextCursor c(doc);
            c.beginEditBlock();
            c.movePosition(QTextCursor::End);
            c.insertBlock();
            c.endEditBlock();
        }

        QTextDocumentWriter writer(fname, "plaintext");
        const bool saved = writer.write(doc);

        if (saved) {
            inactiveTabModified_ = (i != currentIndex);
            doc->setModified(false);

            const QFileInfo fInfo(fname);
            te->setSize(fInfo.size());
            te->setLastModified(fInfo.lastModified());
            setTitle(fname, inactiveTabModified_ ? i : -1);
            addRecentFile(fname);

            const QString prevLang = te->getProg();
            setProgLang(te);
            const QString newLang = te->getProg();

            if (prevLang != newLang) {
                if (config.getShowLangSelector() && config.getSyntaxByDefault()) {
                    if (te->getLang() == newLang)
                        te->setLang(QString());
                    if (!inactiveTabModified_)
                        updateLangBtn(te);
                }

                if (!inactiveTabModified_ && ui->statusBar->isVisible() && te->getWordNumber() != -1)
                    disconnect(doc, &QTextDocument::contentsChange, this, &TexxyWindow::updateWordInfo);

                if (te->getLang().isEmpty()) {
                    syntaxHighlighting(te, false);
                    if (ui->actionSyntax->isChecked())
                        syntaxHighlighting(te);
                }

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
                            const int offset = syntaxKey.size() + 9;
                            if (j > iSyntax + offset)
                                str.replace(iSyntax + offset, j - iSyntax - offset, newLang);
                        }
                    }
                    statusLabel->setText(str);
                    if (te->getWordNumber() != -1)
                        connect(doc, &QTextDocument::contentsChange, this, &TexxyWindow::updateWordInfo);
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

}  // namespace Texxy
