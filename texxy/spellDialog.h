/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2019 <tsujan2000@gmail.com>
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

#ifndef SPELLDIALOG_H
#define SPELLDIALOG_H

#include <QDialog>

namespace Texxy {

class SpellChecker;

namespace Ui {
class SpellDialog;
}

class SpellDialog : public QDialog {
    Q_OBJECT

   public:
    enum SpellAction { CorrectOnce, IgnoreOnce, CorrectAll, IgnoreAll, AddToDict };

    explicit SpellDialog(SpellChecker* spellChecker, const QString& word, bool correction, QWidget* parent = nullptr);
    ~SpellDialog();

    SpellChecker* spellChecker() const { return spellChecker_; }
    QString replacement() const;
    void checkWord(const QString& word);

   signals:
    void spellChecked(int result);

   private:
    Ui::SpellDialog* ui;
    SpellChecker* spellChecker_;
};

}  // namespace Texxy

#endif  // SPELLDIALOG_H
