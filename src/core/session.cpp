// src/core/session.cpp
/*
 texxy/session.cpp
*/

#include "singleton.h"
#include "ui_texxywindow.h"

#include "session.h"
#include "ui_sessionDialog.h"

#include <QAbstractItemDelegate>
#include <QAbstractScrollArea>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSettings>
#include <QTimer>

namespace Texxy {

// make the session dialog behave like a prompt dialog when needed
SessionDialog::SessionDialog(QWidget* parent) : QDialog(parent), ui(new Ui::SessionDialog) {
    ui->setupUi(this);
    parent_ = parent;
    setObjectName(QStringLiteral("sessionDialog"));

    // style prompt banner
    ui->promptLabel->setStyleSheet(QStringLiteral(
        "QLabel {background-color: #7d0000; color: white; border-radius: 3px; margin: 2px; padding: 5px;}"));

    // list widget setup
    ui->listWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    ui->listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    // debounce timer for filter, parented for auto cleanup
    filterTimer_ = new QTimer(this);
    filterTimer_->setSingleShot(true);
    connect(filterTimer_, &QTimer::timeout, this, &SessionDialog::reallyApplyFilter);

    // populate existing session names
    {
        QSettings settings(QStringLiteral("texxy"), QStringLiteral("texxy"));
        settings.beginGroup(QStringLiteral("sessions"));
        allItems_ = settings.allKeys();
        settings.endGroup();
    }

    if (!allItems_.isEmpty()) {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
        for (const auto& item : std::as_const(allItems_))
#else
        for (const auto& item : qAsConst(allItems_))
#endif
        {
            auto* lwi = new ListWidgetItem(item, ui->listWidget);
            ui->listWidget->addItem(lwi);
        }
        ui->listWidget->setCurrentRow(0);
        QTimer::singleShot(0, ui->listWidget, QOverload<>::of(&QWidget::setFocus));
    }
    else {
        onEmptinessChanged(true);
        QTimer::singleShot(0, ui->lineEdit, QOverload<>::of(&QWidget::setFocus));
    }

    ui->listWidget->installEventFilter(this);

    // actions and signals
    connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &SessionDialog::openSessions);
    connect(ui->listWidget, &QListWidget::itemSelectionChanged, this, &SessionDialog::selectionChanged);
    connect(ui->listWidget, &QWidget::customContextMenuRequested, this, &SessionDialog::showContextMenu);
    connect(ui->listWidget->itemDelegate(), &QAbstractItemDelegate::commitData, this, &SessionDialog::OnCommittingName);

    connect(ui->saveBtn, &QAbstractButton::clicked, this, &SessionDialog::saveSession);
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &SessionDialog::saveSession);
    connect(ui->lineEdit, &LineEdit::receivedFocus, [this] { ui->openBtn->setDefault(false); });
    connect(ui->lineEdit, &QLineEdit::textEdited,
            [this](const QString& text) { ui->saveBtn->setEnabled(!text.isEmpty()); });

    connect(ui->openBtn, &QAbstractButton::clicked, this, &SessionDialog::openSessions);
    connect(ui->actionOpen, &QAction::triggered, this, &SessionDialog::openSessions);

    connect(ui->clearBtn, &QAbstractButton::clicked, [this] { showPrompt(CLEAR); });
    connect(ui->removeBtn, &QAbstractButton::clicked, [this] { showPrompt(REMOVE); });
    connect(ui->actionRemove, &QAction::triggered, [this] { showPrompt(REMOVE); });

    connect(ui->actionRename, &QAction::triggered, this, &SessionDialog::renameSession);

    connect(ui->cancelBtn, &QAbstractButton::clicked, this, &SessionDialog::closePrompt);
    connect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::closePrompt);

    connect(ui->closeButton, &QAbstractButton::clicked, this, &QDialog::close);

    connect(ui->filterLineEdit, &QLineEdit::textChanged, this, &SessionDialog::filter);

    // workaround for tooltip formatting issues on some Qt versions
    const auto widgets = findChildren<QWidget*>();
    for (QWidget* w : widgets) {
        const QString tip = w->toolTip();
        if (!tip.isEmpty())
            w->setToolTip(QStringLiteral("<p style='white-space:pre'>") + tip + QStringLiteral("</p>"));
    }

    // initial size proportional to parent
    if (parent_) {
        resize(QSize(parent_->size().width() / 2, 3 * parent_->size().height() / 4));
    }
    else {
        resize(QSize(800, 600));
    }
}

SessionDialog::~SessionDialog() {
    // filterTimer_ is parented, no manual delete needed
    delete ui;
    ui = nullptr;
}

bool SessionDialog::eventFilter(QObject* watched, QEvent* event) {
    // when typing in the list, forward to filter line edit
    if (watched == ui->listWidget && event->type() == QEvent::KeyPress) {
        if (auto* ke = static_cast<QKeyEvent*>(event)) {
            ui->filterLineEdit->pressKey(ke);
            return false;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void SessionDialog::showContextMenu(const QPoint& p) {
    const QModelIndex index = ui->listWidget->indexAt(p);
    if (!index.isValid())
        return;

    ui->listWidget->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);

    QMenu menu(this);  // keep parent for Wayland focus cases
    menu.addAction(ui->actionOpen);
    menu.addAction(ui->actionRemove);
    menu.addSeparator();
    menu.addAction(ui->actionRename);
    menu.exec(ui->listWidget->mapToGlobal(p));
}

void SessionDialog::saveSession() {
    const QString name = ui->lineEdit->text();
    if (name.isEmpty())
        return;

    // check quickly whether there is at least one tab with a file
    auto hasAnyFileInWin = [](TexxyWindow* win) -> bool {
        const int n = win->ui->tabWidget->count();
        for (int i = 0; i < n; ++i) {
            auto* page = qobject_cast<TabPage*>(win->ui->tabWidget->widget(i));
            if (page && !page->textEdit()->getFileName().isEmpty())
                return true;
        }
        return false;
    };

    bool hasFile = false;
    if (ui->windowBox->isChecked()) {
        if (auto* win = static_cast<TexxyWindow*>(parent_))
            hasFile = hasAnyFileInWin(win);
    }
    else {
        auto* singleton = static_cast<TexxyApplication*>(qApp);
        for (int i = 0; i < singleton->Wins.count() && !hasFile; ++i)
            hasFile = hasAnyFileInWin(singleton->Wins.at(i));
    }

    if (!hasFile) {
        showPrompt(tr("Nothing saved.<br>No file was opened."));
        return;
    }

    if (allItems_.contains(name)) {
        showPrompt(NAME);
        return;
    }

    reallySaveSession();
}

void SessionDialog::reallySaveSession() {
    const QString name = ui->lineEdit->text();
    if (name.isEmpty())
        return;

    // remove any existing visible duplicates before inserting
    const auto sameItems = ui->listWidget->findItems(name, Qt::MatchExactly);
    for (QListWidgetItem* it : sameItems)
        delete ui->listWidget->takeItem(ui->listWidget->row(it));

    QStringList files;
    files.reserve(16);  // heuristic to reduce reallocs

    auto collectFilesFromWin = [&](TexxyWindow* win) {
        const int n = win->ui->tabWidget->count();
        for (int i = 0; i < n; ++i) {
            auto* page = qobject_cast<TabPage*>(win->ui->tabWidget->widget(i));
            if (!page)
                continue;
            TextEdit* te = page->textEdit();
            const QString fn = te->getFileName();
            if (!fn.isEmpty()) {
                files << fn;
                te->setSaveCursor(true);
            }
        }
    };

    if (ui->windowBox->isChecked()) {
        if (auto* win = static_cast<TexxyWindow*>(parent_))
            collectFilesFromWin(win);
    }
    else {
        auto* singleton = static_cast<TexxyApplication*>(qApp);
        for (int i = 0; i < singleton->Wins.count(); ++i)
            collectFilesFromWin(singleton->Wins.at(i));
    }

    // there is always at least one file here
    allItems_ << name;
    allItems_.removeDuplicates();

    // add to view if it matches current filter
    {
        const QRegularExpression exp(ui->filterLineEdit->text(), QRegularExpression::CaseInsensitiveOption);
        if (ui->filterLineEdit->text().isEmpty() || allItems_.filter(exp).contains(name)) {
            auto* lwi = new ListWidgetItem(name, ui->listWidget);
            ui->listWidget->addItem(lwi);
        }
    }

    onEmptinessChanged(false);

    QSettings settings(QStringLiteral("texxy"), QStringLiteral("texxy"));
    settings.beginGroup(QStringLiteral("sessions"));
    settings.setValue(name, files);
    settings.endGroup();
}

void SessionDialog::openSessions() {
    const auto items = ui->listWidget->selectedItems();
    const int count = items.count();
    if (count == 0)
        return;

    QStringList files;
    files.reserve(count * 4);  // rough guess to minimize reallocs

    {
        QSettings settings(QStringLiteral("texxy"), QStringLiteral("texxy"));
        settings.beginGroup(QStringLiteral("sessions"));
        for (int i = 0; i < count; ++i)
            files += settings.value(items.at(i)->text()).toStringList();
        settings.endGroup();
    }

    if (files.isEmpty())
        return;

    if (auto* win = static_cast<TexxyWindow*>(parent_)) {
        Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();

        int broken = 0;
        const bool multiple = (files.count() > 1) || win->isLoading();
        for (const QString& f : files) {
            if (!QFileInfo(f).isFile()) {
                // clean cursor config for missing file
                config.removeCursorPos(f);
                ++broken;
                continue;
            }
            win->newTabFromName(f, 1, 0, multiple);  // 1: save cursor, posInLine ignored
        }

        if (broken > 0) {
            if (broken == files.count())
                showPrompt(tr("No file exists or can be opened."));
            else
                showPrompt(tr("Not all files exist or can be opened."));
        }
    }
}

// slots are delayed a tick so returnPressed in the line edit is fully emitted
void SessionDialog::showMainPage() {
    if (!rename_.newName.isEmpty() && !rename_.oldName.isEmpty()) {
        if (QListWidgetItem* cur = ui->listWidget->currentItem())
            cur->setText(rename_.oldName);
        rename_.newName.clear();
        rename_.oldName.clear();
    }
    ui->stackedWidget->setCurrentIndex(0);
}

void SessionDialog::showPromptPage() {
    ui->stackedWidget->setCurrentIndex(1);
    QTimer::singleShot(0, ui->confirmBtn, QOverload<>::of(&QWidget::setFocus));
}

void SessionDialog::showPrompt(const QString& message) {
    disconnect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeAll);
    disconnect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeSelected);
    disconnect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallySaveSession);
    disconnect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallyRenameSession);

    if (message.isEmpty())
        return;

    QTimer::singleShot(0, this, &SessionDialog::showPromptPage);

    ui->confirmBtn->setText(tr("&OK"));
    ui->cancelBtn->setVisible(false);
    ui->promptLabel->setText(QStringLiteral("<b>") + message + QStringLiteral("</b>"));
}

void SessionDialog::showPrompt(PROMPT prompt) {
    disconnect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeAll);
    disconnect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeSelected);
    disconnect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallySaveSession);
    disconnect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallyRenameSession);

    QTimer::singleShot(0, this, &SessionDialog::showPromptPage);

    ui->confirmBtn->setText(tr("&Yes"));
    ui->cancelBtn->setVisible(true);

    if (prompt == CLEAR) {
        ui->promptLabel->setText(QStringLiteral("<b>") + tr("Do you really want to remove all saved sessions?") +
                                 QStringLiteral("</b>"));
        connect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeAll);
    }
    else if (prompt == REMOVE) {
        if (ui->listWidget->selectedItems().count() > 1)
            ui->promptLabel->setText(QStringLiteral("<b>") + tr("Do you really want to remove the selected sessions?") +
                                     QStringLiteral("</b>"));
        else
            ui->promptLabel->setText(QStringLiteral("<b>") + tr("Do you really want to remove the selected session?") +
                                     QStringLiteral("</b>"));
        connect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeSelected);
    }
    else {  // NAME or RENAME
        ui->promptLabel->setText(QStringLiteral("<b>") +
                                 tr("A session with the same name exists.<br>Do you want to overwrite it?") +
                                 QStringLiteral("</b>"));
        if (prompt == NAME)
            connect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallySaveSession);
        else
            connect(ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallyRenameSession);
    }
}

void SessionDialog::closePrompt() {
    ui->promptLabel->clear();
    QTimer::singleShot(0, this, &SessionDialog::showMainPage);
}

void SessionDialog::removeSelected() {
    const auto items = ui->listWidget->selectedItems();
    const int count = items.count();
    if (count == 0)
        return;

    Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();

    QSettings settings(QStringLiteral("texxy"), QStringLiteral("texxy"));
    settings.beginGroup(QStringLiteral("sessions"));

    for (int i = 0; i < count; ++i) {
        const QString key = items.at(i)->text();

        // clean cursor config entries for files in this session
        const QStringList files = settings.value(key).toStringList();
        for (const QString& f : files)
            config.removeCursorPos(f);

        settings.remove(key);
        allItems_.removeOne(key);
        delete ui->listWidget->takeItem(ui->listWidget->row(items.at(i)));
    }

    settings.endGroup();

    if (config.savedCursorPos().isEmpty()) {
        Settings curSettings(QStringLiteral("texxy"), QStringLiteral("texxy_cursor_pos"));
        curSettings.remove(QStringLiteral("cursorPositions"));
    }

    if (allItems_.isEmpty())
        onEmptinessChanged(true);
}

void SessionDialog::removeAll() {
    // drop all cursor positions first
    Config& config = static_cast<TexxyApplication*>(qApp)->getConfig();
    config.removeAllCursorPos();
    {
        Settings curSettings(QStringLiteral("texxy"), QStringLiteral("texxy_cursor_pos"));
        curSettings.remove(QStringLiteral("cursorPositions"));
    }

    ui->listWidget->clear();
    onEmptinessChanged(true);

    QSettings settings(QStringLiteral("texxy"), QStringLiteral("texxy"));
    settings.beginGroup(QStringLiteral("sessions"));
    settings.remove(QString());  // remove the whole group
    settings.endGroup();
}

void SessionDialog::selectionChanged() {
    const bool noSel = ui->listWidget->selectedItems().isEmpty();
    ui->openBtn->setEnabled(!noSel);
    // enable Enter to open when list has focus
    ui->openBtn->setDefault(true);
}

void SessionDialog::renameSession() {
    if (QListWidgetItem* cur = ui->listWidget->currentItem()) {
        rename_.oldName = cur->text();
        cur->setFlags(cur->flags() | Qt::ItemIsEditable);
        ui->listWidget->editItem(cur);
    }
}

void SessionDialog::OnCommittingName(QWidget* editor) {
    if (QListWidgetItem* cur = ui->listWidget->currentItem())
        cur->setFlags(cur->flags() & ~Qt::ItemIsEditable);

    auto* le = qobject_cast<QLineEdit*>(editor);
    if (!le) {
        rename_.newName.clear();
        rename_.oldName.clear();
        return;
    }

    rename_.newName = le->text();
    if (rename_.newName.isEmpty() || rename_.newName == rename_.oldName) {
        rename_.newName.clear();
        rename_.oldName.clear();
        return;
    }

    if (allItems_.contains(rename_.newName))
        showPrompt(RENAME);
    else
        reallyRenameSession();
}

void SessionDialog::reallyRenameSession() {
    if (rename_.newName.isEmpty() || rename_.oldName.isEmpty()) {
        rename_.newName.clear();
        rename_.oldName.clear();
        return;
    }

    QSettings settings(QStringLiteral("texxy"), QStringLiteral("texxy"));
    settings.beginGroup(QStringLiteral("sessions"));

    const QStringList files = settings.value(rename_.oldName).toStringList();
    settings.remove(rename_.oldName);
    settings.setValue(rename_.newName, files);

    settings.endGroup();

    allItems_.removeOne(rename_.oldName);
    allItems_ << rename_.newName;
    allItems_.removeDuplicates();

    if (QListWidgetItem* cur = ui->listWidget->currentItem()) {
        bool isFiltered = false;

        if (!ui->filterLineEdit->text().isEmpty()) {
            const QRegularExpression exp(ui->filterLineEdit->text(), QRegularExpression::CaseInsensitiveOption);
            if (!allItems_.filter(exp).contains(rename_.newName))
                isFiltered = true;
        }

        // remove any duplicate visible items with the new name
        const auto sameItems = ui->listWidget->findItems(rename_.newName, Qt::MatchExactly);
        for (QListWidgetItem* it : sameItems) {
            if (isFiltered || it != cur)
                delete ui->listWidget->takeItem(ui->listWidget->row(it));
        }

        if (!isFiltered) {
            cur->setText(rename_.newName);
            ui->listWidget->scrollToItem(cur);
        }
        else {
            // if filtered out, keep current item's text untouched and let filter refresh hide it
        }
    }

    rename_.newName.clear();
    rename_.oldName.clear();
}

void SessionDialog::filter(const QString& /*text*/) {
    // debounce to avoid filtering on every keystroke
    filterTimer_->start(200);
}

void SessionDialog::reallyApplyFilter() {
    // capture selection before refresh
    QStringList sel;
    const auto items = ui->listWidget->selectedItems();
    sel.reserve(items.count());
    for (QListWidgetItem* it : items)
        sel << it->text();

    ui->listWidget->clear();

    const QRegularExpression exp(ui->filterLineEdit->text(), QRegularExpression::CaseInsensitiveOption);
    const QStringList filtered = allItems_.filter(exp);

    for (const QString& item : filtered) {
        auto* lwi = new ListWidgetItem(item, ui->listWidget);
        ui->listWidget->addItem(lwi);
    }

    // restore selection where possible
    if (filtered.count() == 1) {
        ui->listWidget->setCurrentRow(0);
    }
    else if (!sel.isEmpty()) {
        for (int i = 0; i < ui->listWidget->count(); ++i) {
            if (sel.contains(ui->listWidget->item(i)->text()))
                ui->listWidget->setCurrentRow(i, QItemSelectionModel::Select);
        }
    }
}

void SessionDialog::onEmptinessChanged(bool empty) {
    ui->clearBtn->setEnabled(!empty);
    if (empty) {
        allItems_.clear();
        ui->filterLineEdit->clear();
    }
    ui->filterLineEdit->setEnabled(!empty);
}

}  // namespace Texxy
