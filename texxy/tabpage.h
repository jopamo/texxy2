/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2019 <tsujan2000@gmail.com>
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

#ifndef TABPAGE_H
#define TABPAGE_H

#include <QPointer>
#include "searchbar.h"
#include "textedit.h"

namespace Texxy {

class TabPage : public QWidget {
    Q_OBJECT
   public:
    TabPage(int bgColorValue = 255,
            const QList<QKeySequence>& searchShortcuts = QList<QKeySequence>(),
            QWidget* parent = nullptr);

    QPointer<TextEdit> textEdit() const { return textEdit_; }

    void setSearchModel(QStandardItemModel* model) { searchBar_->setSearchModel(model); }

    void setSearchBarVisible(bool visible);
    bool isSearchBarVisible() const;
    void focusSearchBar();
    bool searchBarHasFocus() const;

    QString searchEntry() const;
    void clearSearchEntry();

    bool matchCase() const;
    bool matchWhole() const;
    bool matchRegex() const;

    void updateShortcuts(bool disable);

    void lockPage(bool lock);

   signals:
    void find(bool forward);
    void searchFlagChanged();

   private:
    QPointer<TextEdit> textEdit_;
    QPointer<SearchBar> searchBar_;
};

}  // namespace Texxy

#endif  // TABPAGE_H
