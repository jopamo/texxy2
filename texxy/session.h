/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2022 <tsujan2000@gmail.com>
 *
 * Texxy is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Texxy is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @license GPL-3.0+ <https://spdx.org/licenses/GPL-3.0+.html>
 */

#ifndef SESSION_H
#define SESSION_H

#include <QDialog>
#include <QTimer>

namespace Texxy {

namespace Ui {
class SessionDialog;
}

class SessionDialog : public QDialog {
    Q_OBJECT

   public:
    explicit SessionDialog(QWidget* parent = nullptr);
    ~SessionDialog();

   protected:
    bool eventFilter(QObject* watched, QEvent* event);

   private slots:
    void showContextMenu(const QPoint& p);
    void saveSession();
    void reallySaveSession();
    void selectionChanged();
    void openSessions();
    void closePrompt();
    void removeSelected();
    void removeAll();
    void showMainPage();
    void showPromptPage();
    void renameSession();
    void reallyRenameSession();
    void OnCommittingName(QWidget* editor);
    void filter(const QString&);
    void reallyApplyFilter();

   private:
    enum PROMPT { NAME, RENAME, REMOVE, CLEAR };

    struct Rename {
        QString oldName;
        QString newName;
    };

    void showPrompt(PROMPT prompt);
    void showPrompt(const QString& message);
    void onEmptinessChanged(bool empty);

    Ui::SessionDialog* ui;
    QWidget* parent_;
    Rename rename_;
    /* Used only for filtering: */
    QStringList allItems_;
    QTimer* filterTimer_;
};

}  // namespace Texxy

#endif  // SESSION_H
