#include "texxy_ui_prelude.h"

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
    ui->actionSortLines->setEnabled(false);
    ui->actionRSortLines->setEnabled(false);
    ui->ActionRmDupeSort->setEnabled(false);
    ui->ActionRmDupeRSort->setEnabled(false);
    ui->ActionSpaceDupeSort->setEnabled(false);
    ui->ActionSpaceDupeRSort->setEnabled(false);
    ui->actionPaste->setEnabled(false);

    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        if (!textEdit->isReadOnly()) {
            ui->actionPaste->setEnabled(textEdit->pastingIsPossible());
            const QTextCursor cursor = textEdit->textCursor();
            if (cursor.hasSelection()) {
                const QString selection = cursor.selectedText();
                if (!selection.isEmpty()) {
                    ui->ActionSpaceDupeSort->setEnabled(true);
                    ui->ActionSpaceDupeRSort->setEnabled(true);
                    if (selection.contains(QChar(QChar::ParagraphSeparator))) {
                        ui->actionSortLines->setEnabled(true);
                        ui->actionRSortLines->setEnabled(true);
                        ui->ActionRmDupeSort->setEnabled(true);
                        ui->ActionRmDupeRSort->setEnabled(true);
                    }
                }
            }
        }
    }
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

void TexxyWindow::addRemoveLangBtn(bool add) {
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
        connect(aGroup, &QActionGroup::triggered, this, &TexxyWindow::enforceLang);

        /* update the language button if this is called from outside c-tor
           (otherwise, tabswitch() will do it) */
        if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
            updateLangBtn(tabPage->textEdit());
    }
}
void TexxyWindow::editorContextMenu(const QPoint& p) {
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
            const QString selection = textEdit->textCursor().selectedText();
            bool addedSortSeparator = false;
            if (selection.contains(QChar(QChar::ParagraphSeparator))) {
                menu->addSeparator();
                addedSortSeparator = true;
                ui->actionSortLines->setEnabled(true);
                ui->actionRSortLines->setEnabled(true);
                ui->ActionRmDupeSort->setEnabled(true);
                ui->ActionRmDupeRSort->setEnabled(true);
                menu->addAction(ui->actionSortLines);
                menu->addAction(ui->actionRSortLines);
                menu->addAction(ui->ActionRmDupeSort);
                menu->addAction(ui->ActionRmDupeRSort);
            }
            if (!selection.isEmpty()) {
                if (!addedSortSeparator)
                    menu->addSeparator();
                ui->ActionSpaceDupeSort->setEnabled(true);
                ui->ActionSpaceDupeRSort->setEnabled(true);
                menu->addAction(ui->ActionSpaceDupeSort);
                menu->addAction(ui->ActionSpaceDupeRSort);
            }
            menu->addSeparator();
        }
        menu->addAction(ui->actionDate);
    }

    menu->exec(textEdit->viewport()->mapToGlobal(p));
    delete menu;
}
void TexxyWindow::reformat(TextEdit* textEdit) {
    formatTextRect();  // in "syntax.cpp"
    if (!textEdit->getSearchedText().isEmpty())
        hlight();  // in "find.cpp"
    textEdit->selectionHlight();
}
void TexxyWindow::zoomIn() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->zooming(1.f);
}
void TexxyWindow::zoomOut() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        textEdit->zooming(-1.f);
    }
}
void TexxyWindow::zoomZero() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        textEdit->zooming(0.f);
    }
}
void TexxyWindow::defaultSize() {
    QSize s = static_cast<TexxyApplication*>(qApp)->getConfig().getStartSize();
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
    // QTimer::singleShot (0, this, &TexxyWindow::show);
}
void TexxyWindow::focusView() {
    if (TabPage* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->setFocus();
}
void TexxyWindow::focusSidePane() {
    if (sidePane_) {
        QList<int> sizes = ui->splitter->sizes();
        if (sizes.size() == 2 && sizes.at(0) == 0)  // with RTL too
        {                                           // first, ensure its visibility (see toggleSidePane())
            sizes.clear();
            Config config = static_cast<TexxyApplication*>(qApp)->getConfig();
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
void TexxyWindow::showHideSearch() {
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
            QList<QTextEdit::ExtraSelection> emptySelections;
            textEdit->setGreenSel(emptySelections);  // not needed
            textEdit->setExtraSelections(composeSelections(textEdit, emptySelections));
            /* ... and empty all search entries */
            page->clearSearchEntry();
        }
        page->setSearchBarVisible(!isFocused);
    }
}
void TexxyWindow::jumpTo() {
    if (!isReady())
        return;

    bool visibility = ui->spinBox->isVisible();

    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        TextEdit* thisTextEdit = qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit();
        if (!ui->actionLineNumbers->isChecked())
            thisTextEdit->showLineNumbers(!visibility);

        if (!visibility) {
            /* setMaximum() isn't a slot */
            connect(thisTextEdit->document(), &QTextDocument::blockCountChanged, this, &TexxyWindow::setMax);
        }
        else
            disconnect(thisTextEdit->document(), &QTextDocument::blockCountChanged, this, &TexxyWindow::setMax);
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
void TexxyWindow::setMax(const int max) {
    ui->spinBox->setMaximum(max);
}
void TexxyWindow::goTo() {
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
void TexxyWindow::showLN(bool checked) {
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
void TexxyWindow::toggleWrapping() {
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
void TexxyWindow::toggleIndent() {
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
void TexxyWindow::stealFocus(QWidget* w) {
    if (w->isMinimized())
        w->setWindowState((w->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
#ifdef HAS_X11
    else if (static_cast<TexxyApplication*>(qApp)->isX11()) {
        if (isWindowShaded(w->winId()))
            unshadeWindow(w->winId());
    }
#endif

    raise();
    /* WARNING: Under Wayland, this warning is shown by qtwayland -> qwaylandwindow.cpp
                -> QWaylandWindow::requestActivateWindow():
                "Wayland does not support QWindow::requestActivate()" */
    if (!static_cast<TexxyApplication*>(qApp)->isWayland()) {
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
void TexxyWindow::stealFocus() {
    /* if there is a (sessions) dialog, let it keep the focus */
    const auto dialogs = findChildren<QDialog*>();
    if (!dialogs.isEmpty()) {
        stealFocus(dialogs.at(0));
        return;
    }

    stealFocus(this);
}

}  // namespace Texxy
