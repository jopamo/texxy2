// src/ui/window_edit.cpp

#include "texxy_ui_prelude.h"

namespace Texxy {

namespace {
// get current tab page safely
static inline TabPage* curTab(TexxyWindow* w) {
    return qobject_cast<TabPage*>(w->ui->tabWidget->currentWidget());
}

// get current text editor safely
static inline TextEdit* curEdit(TexxyWindow* w) {
    if (TabPage* p = curTab(w))
        return p->textEdit();
    return nullptr;
}
}  // namespace

void TexxyWindow::cutText() {
    if (TextEdit* te = curEdit(this))
        te->cut();
}

void TexxyWindow::copyText() {
    if (TextEdit* te = curEdit(this))
        te->copy();
}

void TexxyWindow::pasteText() {
    if (TextEdit* te = curEdit(this))
        te->paste();
}

void TexxyWindow::toSoftTabs() {
    if (TextEdit* te = curEdit(this)) {
        makeBusy();
        const bool res = te->toSoftTabs();
        unbusy();
        if (res) {
            removeGreenSel();
            showWarningBar(
                QStringLiteral("<center><b><big>%1</big></b></center>").arg(tr("Text tabs are converted to spaces.")));
        }
    }
}

void TexxyWindow::insertDate() {
    if (TextEdit* te = curEdit(this)) {
        const auto& config = static_cast<TexxyApplication*>(qApp)->getConfig();
        const QString format = config.getDateFormat();
        te->insertPlainText(format.isEmpty() ? locale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat)
                                             : locale().toString(QDateTime::currentDateTime(), format));
    }
}

void TexxyWindow::deleteText() {
    if (TextEdit* te = curEdit(this)) {
        if (!te->isReadOnly())
            te->deleteText();
    }
}

void TexxyWindow::selectAllText() {
    if (TextEdit* te = curEdit(this))
        te->selectAll();
}

void TexxyWindow::upperCase() {
    if (TextEdit* te = curEdit(this)) {
        if (!te->isReadOnly())
            te->insertPlainText(locale().toUpper(te->textCursor().selectedText()));
    }
}

void TexxyWindow::lowerCase() {
    if (TextEdit* te = curEdit(this)) {
        if (!te->isReadOnly())
            te->insertPlainText(locale().toLower(te->textCursor().selectedText()));
    }
}

void TexxyWindow::startCase() {
    if (TextEdit* te = curEdit(this)) {
        if (te->isReadOnly())
            return;

        bool showWarn = false;
        QTextCursor cur = te->textCursor();
        int start = std::min(cur.anchor(), cur.position());
        int end = std::max(cur.anchor(), cur.position());
        if (end > start + 100000) {
            showWarn = true;
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

        int idx = 0;
        QRegularExpressionMatch match;
        // WARNING: QTextCursor::selectedText() uses U+2029 instead of \n
        while ((idx = str.indexOf(QRegularExpression(QStringLiteral("[^\\s\\-\\.\\n\\x{2029}]+")), idx, &match)) > -1) {
            QChar c = str.at(idx);
            // find the first letter from the start of the word
            int i = 0;
            while (!c.isLetter() && i + 1 < match.capturedLength()) {
                ++i;
                c = str.at(idx + i);
            }
            str.replace(idx + i, 1, c.toUpper());
            idx += match.capturedLength();
        }

        cur.beginEditBlock();
        te->setTextCursor(cur);
        te->insertPlainText(str);
        te->ensureCursorVisible();
        cur.endEditBlock();

        if (showWarn) {
            showWarningBar(QStringLiteral("<center><b><big>%1</big></b></center>\n<center>%2</center>")
                               .arg(tr("The selected text was too long."), tr("It is not fully processed.")));
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

    if (TextEdit* te = curEdit(this)) {
        if (!te->isReadOnly()) {
            ui->actionPaste->setEnabled(te->pastingIsPossible());
            const QTextCursor cursor = te->textCursor();
            if (cursor.hasSelection()) {
                const QString sel = cursor.selectedText();
                if (!sel.isEmpty()) {
                    ui->ActionSpaceDupeSort->setEnabled(true);
                    ui->ActionSpaceDupeRSort->setEnabled(true);
                    if (sel.contains(QChar(QChar::ParagraphSeparator))) {
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
    if (TextEdit* te = curEdit(this))
        ui->actionPaste->setEnabled(!te->isReadOnly());
    else
        ui->actionPaste->setEnabled(false);
}

void TexxyWindow::sortLines() {
    if (TextEdit* te = curEdit(this))
        te->sortLines(qobject_cast<QAction*>(QObject::sender()) == ui->actionRSortLines);
}

void TexxyWindow::rmDupeSort() {
    if (TextEdit* te = curEdit(this))
        te->rmDupeSort(qobject_cast<QAction*>(QObject::sender()) == ui->ActionRmDupeRSort);
}

void TexxyWindow::spaceDupeSort() {
    if (TextEdit* te = curEdit(this))
        te->spaceDupeSort(qobject_cast<QAction*>(QObject::sender()) == ui->ActionSpaceDupeRSort);
}

void TexxyWindow::makeEditable() {
    if (!isReady())
        return;

    TabPage* tabPage = curTab(this);
    if (!tabPage)
        return;

    TextEdit* te = tabPage->textEdit();
    const bool hasSel = te->textCursor().hasSelection();
    const bool hasColumn = !te->getColSel().isEmpty();

    te->setReadOnly(false);
    const auto& config = static_cast<TexxyApplication*>(qApp)->getConfig();
    if (!te->hasDarkScheme()) {
        te->viewport()->setStyleSheet(QStringLiteral(".QWidget {color: black; background-color: rgb(%1, %1, %1);}")
                                          .arg(config.getLightBgColorValue()));
    }
    else {
        te->viewport()->setStyleSheet(QStringLiteral(".QWidget {color: white; background-color: rgb(%1, %1, %1);}")
                                          .arg(config.getDarkBgColorValue()));
    }
    ui->actionEdit->setVisible(false);

    ui->actionPaste->setEnabled(true);  // might change temporarily in showingEditMenu
    ui->actionSoftTab->setEnabled(true);
    ui->actionDate->setEnabled(true);
    ui->actionCopy->setEnabled(hasSel || hasColumn);
    ui->actionCut->setEnabled(hasSel || hasColumn);
    ui->actionDelete->setEnabled(hasSel || hasColumn);
    ui->actionUpperCase->setEnabled(hasSel);
    ui->actionLowerCase->setEnabled(hasSel);
    ui->actionStartCase->setEnabled(hasSel);
    connect(te, &TextEdit::canCopy, ui->actionCut, &QAction::setEnabled);
    connect(te, &TextEdit::canCopy, ui->actionDelete, &QAction::setEnabled);
    connect(te, &QPlainTextEdit::copyAvailable, ui->actionUpperCase, &QAction::setEnabled);
    connect(te, &QPlainTextEdit::copyAvailable, ui->actionLowerCase, &QAction::setEnabled);
    connect(te, &QPlainTextEdit::copyAvailable, ui->actionStartCase, &QAction::setEnabled);
    if (config.getSaveUnmodified())
        ui->actionSave->setEnabled(true);
}

void TexxyWindow::undoing() {
    if (TextEdit* te = curEdit(this))
        te->undo();
}

void TexxyWindow::redoing() {
    if (TextEdit* te = curEdit(this))
        te->redo();
}

void TexxyWindow::addRemoveLangBtn(bool add) {
    static QStringList langList;
    if (langList.isEmpty()) {  // no "url" for the language button
        langList << QStringLiteral("c") << QStringLiteral("cmake") << QStringLiteral("config") << QStringLiteral("cpp")
                 << QStringLiteral("css") << QStringLiteral("dart") << QStringLiteral("deb") << QStringLiteral("diff")
                 << QStringLiteral("fountain") << QStringLiteral("html") << QStringLiteral("java")
                 << QStringLiteral("javascript") << QStringLiteral("json") << QStringLiteral("LaTeX")
                 << QStringLiteral("go") << QStringLiteral("log") << QStringLiteral("lua") << QStringLiteral("m3u")
                 << QStringLiteral("markdown") << QStringLiteral("makefile") << QStringLiteral("pascal")
                 << QStringLiteral("perl") << QStringLiteral("php") << QStringLiteral("python")
                 << QStringLiteral("qmake") << QStringLiteral("qml") << QStringLiteral("reST") << QStringLiteral("ruby")
                 << QStringLiteral("rust") << QStringLiteral("scss") << QStringLiteral("sh") << QStringLiteral("tcl")
                 << QStringLiteral("toml") << QStringLiteral("troff") << QStringLiteral("xml")
                 << QStringLiteral("yaml");
        langList.sort(Qt::CaseInsensitive);
    }

    QToolButton* langButton = ui->statusBar->findChild<QToolButton*>(QStringLiteral("langButton"));
    if (!add) {
        langs_.clear();
        if (langButton) {
            delete langButton;  // deletes the menu and its actions
            langButton = nullptr;
        }

        for (int i = 0; i < ui->tabWidget->count(); ++i) {
            TextEdit* te = qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit();
            if (!te->getLang().isEmpty()) {
                te->setLang(QString());  // remove the enforced syntax
                if (ui->actionSyntax->isChecked()) {
                    syntaxHighlighting(te, false);
                    syntaxHighlighting(te);
                }
            }
        }
    }
    else if (!langButton && langs_.isEmpty()) {
        const QString normal = tr("Normal");
        langButton = new QToolButton();
        langButton->setObjectName(QStringLiteral("langButton"));
        langButton->setFocusPolicy(Qt::NoFocus);
        langButton->setAutoRaise(true);
        langButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
        langButton->setText(normal);
        langButton->setPopupMode(QToolButton::InstantPopup);

        // searchable menu
        class Menu : public QMenu {
           public:
            Menu(QWidget* parent = nullptr) : QMenu(parent), selectionTimer_(nullptr) {}
            ~Menu() override {
                if (selectionTimer_) {
                    if (selectionTimer_->isActive())
                        selectionTimer_->stop();
                    delete selectionTimer_;
                }
            }

           protected:
            void keyPressEvent(QKeyEvent* e) override {
                if (!selectionTimer_) {
                    selectionTimer_ = new QTimer();
                    connect(selectionTimer_, &QTimer::timeout, this, [this] {
                        if (txt_.isEmpty())
                            return;
                        const auto acts = actions();
                        // search in starting strings first
                        for (QAction* a : acts) {
                            QString t = a->text();
                            t.remove('&');
                            if (t.startsWith(txt_, Qt::CaseInsensitive)) {
                                setActiveAction(a);
                                txt_.clear();
                                return;
                            }
                        }
                        // search for containing strings
                        for (QAction* a : acts) {
                            QString t = a->text();
                            t.remove('&');
                            if (t.contains(txt_, Qt::CaseInsensitive)) {
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

        auto* menu = new Menu(langButton);
        auto* aGroup = new QActionGroup(langButton);
        QAction* action = nullptr;
        for (int i = 0; i < langList.count(); ++i) {
            const QString lang = langList.at(i);
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

        // update the language button if called from outside ctor
        if (TabPage* tabPage = curTab(this))
            updateLangBtn(tabPage->textEdit());
    }
}

void TexxyWindow::editorContextMenu(const QPoint& p) {
    // the editor's customized context menu lives here to avoid duplicating actions

    TextEdit* textEdit = qobject_cast<TextEdit*>(QObject::sender());
    if (!textEdit)
        return;

    // announce that the mouse button is released, as TextEdit::mouseReleaseEvent is not called when showing the menu
    QTimer::singleShot(0, textEdit, [textEdit]() { textEdit->releaseMouse(); });

    // put the cursor at the right-click position if it has no selection
    if (!textEdit->textCursor().hasSelection())
        textEdit->setTextCursor(textEdit->cursorForPosition(p));

    QMenu* menu = textEdit->createStandardContextMenu(p);
    const QList<QAction*> actions = menu->actions();
    if (!actions.isEmpty()) {
        const bool hasColumn = !textEdit->getColSel().isEmpty();
        for (QAction* const thisAction : actions) {
            // remove the shortcut strings because shortcuts may change
            QString txt = thisAction->text();
            if (!txt.isEmpty())
                txt = txt.split('\t').first();
            if (!txt.isEmpty())
                thisAction->setText(txt);
            // correct the slots of some actions
            if (thisAction->objectName() == QStringLiteral("edit-copy")) {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::copy);
                if (hasColumn && !thisAction->isEnabled())
                    thisAction->setEnabled(true);
            }
            else if (thisAction->objectName() == QStringLiteral("edit-cut")) {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::cut);
                if (hasColumn && !thisAction->isEnabled())
                    thisAction->setEnabled(true);
            }
            else if (thisAction->objectName() == QStringLiteral("edit-paste")) {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::paste);
                // correct the enabled state of paste by consulting TextEdit::pastingIsPossible
                thisAction->setEnabled(textEdit->pastingIsPossible());
            }
            else if (thisAction->objectName() == QStringLiteral("edit-delete")) {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::deleteText);
                if (hasColumn && !thisAction->isEnabled())
                    thisAction->setEnabled(true);
            }
            else if (thisAction->objectName() == QStringLiteral("edit-undo")) {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::undo);
            }
            else if (thisAction->objectName() == QStringLiteral("edit-redo")) {
                disconnect(thisAction, &QAction::triggered, nullptr, nullptr);
                connect(thisAction, &QAction::triggered, textEdit, &TextEdit::redo);
            }
            else if (thisAction->objectName() == QStringLiteral("select-all")) {
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
                    url = QUrl::fromUserInput(str, QStringLiteral("/"));
                // prefer gio over xdg-open when available
                if (QStandardPaths::findExecutable(QStringLiteral("gio")).isEmpty() ||
                    !QProcess::startDetached(QStringLiteral("gio"), QStringList()
                                                                        << QStringLiteral("open") << url.toString())) {
                    QDesktopServices::openUrl(url);
                }
            });
            if (str.startsWith(QStringLiteral("mailto:")))  // see getUrl
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
    formatTextRect();  // in syntax.cpp
    if (!textEdit->getSearchedText().isEmpty())
        hlight();  // in find.cpp
    textEdit->selectionHlight();
}

void TexxyWindow::zoomIn() {
    if (TextEdit* te = curEdit(this))
        te->zooming(1.f);
}

void TexxyWindow::zoomOut() {
    if (TextEdit* te = curEdit(this))
        te->zooming(-1.f);
}

void TexxyWindow::zoomZero() {
    if (TextEdit* te = curEdit(this))
        te->zooming(0.f);
}

void TexxyWindow::defaultSize() {
    const QSize s = static_cast<TexxyApplication*>(qApp)->getConfig().getStartSize();
    if (size() == s)
        return;
    if (isMaximized() || isFullScreen())
        showNormal();
    resize(s);
}

void TexxyWindow::focusView() {
    if (TextEdit* te = curEdit(this))
        te->setFocus();
}

void TexxyWindow::focusSidePane() {
    if (!sidePane_)
        return;

    QList<int> sizes = ui->splitter->sizes();
    if (sizes.size() == 2 && sizes.at(0) == 0) {  // with RTL too
        // first, ensure its visibility
        sizes.clear();
        const auto& config = static_cast<TexxyApplication*>(qApp)->getConfig();
        if (config.getRemSplitterPos()) {
            sizes.append(std::min(std::max(16, config.getSplitterPos()), size().width() / 2));
            sizes.append(100);
        }
        else {
            const int s = std::min(size().width() / 5, 40 * sidePane_->fontMetrics().horizontalAdvance(' '));
            sizes << s << size().width() - s;
        }
        ui->splitter->setSizes(sizes);
    }
    sidePane_->listWidget()->setFocus();
}

void TexxyWindow::showHideSearch() {
    if (!isReady())
        return;

    TabPage* tabPage = curTab(this);
    if (!tabPage)
        return;

    const bool isFocused = tabPage->isSearchBarVisible() && tabPage->searchBarHasFocus();

    if (!isFocused)
        tabPage->focusSearchBar();
    else {
        ui->dockReplace->setVisible(false);  // searchbar is needed by replace dock
        // return focus to the document
        tabPage->textEdit()->setFocus();
    }

    const int count = ui->tabWidget->count();
    for (int i = 0; i < count; ++i) {
        TabPage* page = qobject_cast<TabPage*>(ui->tabWidget->widget(i));
        if (isFocused) {
            // remove all yellow and green highlights
            TextEdit* te = page->textEdit();
            te->setSearchedText(QString());
            QList<QTextEdit::ExtraSelection> emptySelections;
            te->setGreenSel(emptySelections);
            te->setExtraSelections(composeSelections(te, emptySelections));
            // empty all search entries
            page->clearSearchEntry();
        }
        page->setSearchBarVisible(!isFocused);
    }
}

void TexxyWindow::jumpTo() {
    if (!isReady())
        return;

    const bool visibility = ui->spinBox->isVisible();

    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        TextEdit* te = qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit();
        if (!ui->actionLineNumbers->isChecked())
            te->showLineNumbers(!visibility);

        if (!visibility)
            connect(te->document(), &QTextDocument::blockCountChanged, this, &TexxyWindow::setMax);
        else
            disconnect(te->document(), &QTextDocument::blockCountChanged, this, &TexxyWindow::setMax);
    }

    if (TabPage* tabPage = curTab(this)) {
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
    else if (TabPage* tp = curTab(this)) {
        tp->textEdit()->setFocus();
    }
}

void TexxyWindow::setMax(const int max) {
    ui->spinBox->setMaximum(max);
}

void TexxyWindow::goTo() {
    // workaround for not being able to use returnPressed because of protectedness of spinbox's QLineEdit
    if (!ui->spinBox->hasFocus())
        return;

    if (TextEdit* te = curEdit(this)) {
        QTextBlock block = te->document()->findBlockByNumber(ui->spinBox->value() - 1);
        const int pos = block.position();
        QTextCursor start = te->textCursor();
        if (ui->checkBox->isChecked())
            start.setPosition(pos, QTextCursor::KeepAnchor);
        else
            start.setPosition(pos);
        te->setTextCursor(start);
    }
}

void TexxyWindow::showLN(bool checked) {
    const int count = ui->tabWidget->count();
    if (count == 0)
        return;

    if (checked) {
        for (int i = 0; i < count; ++i)
            qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit()->showLineNumbers(true);
    }
    else if (!ui->spinBox->isVisible()) {
        for (int i = 0; i < count; ++i)
            qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit()->showLineNumbers(false);
    }
}

void TexxyWindow::toggleWrapping() {
    const int count = ui->tabWidget->count();
    if (count == 0)
        return;

    const bool wrapLines = ui->actionWrap->isChecked();
    for (int i = 0; i < count; ++i) {
        auto te = qobject_cast<TabPage*>(ui->tabWidget->widget(i))->textEdit();
        te->setLineWrapMode(wrapLines ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
        te->removeColumnHighlight();
    }
    if (TextEdit* te = curEdit(this))
        reformat(te);
}

void TexxyWindow::toggleIndent() {
    const int count = ui->tabWidget->count();
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
    // WARNING: Under Wayland, this warning is shown by qtwayland -> qwaylandwindow.cpp
    //          -> QWaylandWindow::requestActivateWindow(): Wayland does not support QWindow::requestActivate
    if (!static_cast<TexxyApplication*>(qApp)->isWayland()) {
        w->activateWindow();
        QTimer::singleShot(0, w, [w]() {
            if (QWindow* win = w->windowHandle())
                win->requestActivate();
        });
    }
    else if (!w->isActiveWindow()) {
        // demand attention under Wayland, WMs may ignore it
        QApplication::alert(w);
    }
}

void TexxyWindow::stealFocus() {
    // if there is a dialog, let it keep the focus
    const auto dialogs = findChildren<QDialog*>();
    if (!dialogs.isEmpty()) {
        stealFocus(dialogs.at(0));
        return;
    }

    stealFocus(this);
}

}  // namespace Texxy
