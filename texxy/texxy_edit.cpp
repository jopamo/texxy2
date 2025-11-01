#include "singleton.h"
#include "texxywindow.h"
#include "ui_texxywindow.h"

#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>
#include <algorithm>

namespace Texxy {

void TexxyWindow::cutText() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->cut();
}

void TexxyWindow::copyText() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->copy();
}

void TexxyWindow::pasteText() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->paste();
}

void TexxyWindow::toSoftTabs() {
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

void TexxyWindow::insertDate() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        Config config = static_cast<TexxyApplication*>(qApp)->getConfig();
        QString format = config.getDateFormat();
        tabPage->textEdit()->insertPlainText(format.isEmpty()
                                                 ? locale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat)
                                                 : locale().toString(QDateTime::currentDateTime(), format));
    }
}

void TexxyWindow::deleteText() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        if (!textEdit->isReadOnly())
            textEdit->deleteText();
    }
}

void TexxyWindow::selectAllText() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->selectAll();
}

void TexxyWindow::upperCase() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        if (!textEdit->isReadOnly())
            textEdit->insertPlainText(locale().toUpper(textEdit->textCursor().selectedText()));
    }
}

void TexxyWindow::lowerCase() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        if (!textEdit->isReadOnly())
            textEdit->insertPlainText(locale().toLower(textEdit->textCursor().selectedText()));
    }
}

void TexxyWindow::startCase() {
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

void TexxyWindow::showingEditMenu() {
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

void TexxyWindow::hidngEditMenu() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        /* QPlainTextEdit::canPaste() isn't consulted because it might change later */
        ui->actionPaste->setEnabled(!tabPage->textEdit()->isReadOnly());
    }
    else
        ui->actionPaste->setEnabled(false);
}

void TexxyWindow::sortLines() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->sortLines(qobject_cast<QAction*>(QObject::sender()) == ui->actionRSortLines);
}

void TexxyWindow::rmDupeSort() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->rmDupeSort(qobject_cast<QAction*>(QObject::sender()) == ui->ActionRmDupeRSort);
}

void TexxyWindow::spaceDupeSort() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->spaceDupeSort(qobject_cast<QAction*>(QObject::sender()) == ui->ActionSpaceDupeRSort);
}

void TexxyWindow::makeEditable() {
    if (!isReady())
        return;

    TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr)
        return;

    TextEdit* textEdit = tabPage->textEdit();
    bool textIsSelected = textEdit->textCursor().hasSelection();
    bool hasColumn = !textEdit->getColSel().isEmpty();

    textEdit->setReadOnly(false);
    Config config = static_cast<TexxyApplication*>(qApp)->getConfig();
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

void TexxyWindow::undoing() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->undo();
}

void TexxyWindow::redoing() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->redo();
}

}  // namespace Texxy
