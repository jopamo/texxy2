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

#ifndef FONTDIALOG_H
#define FONTDIALOG_H

#include <QDialog>

namespace Texxy {

namespace Ui {
class FontDialog;
}

class FontDialog : public QDialog {
    Q_OBJECT

   public:
    explicit FontDialog(const QFont& font, QWidget* parent = nullptr);
    ~FontDialog();

    QFont selectedFont() const { return font_; }

   private:
    Ui::FontDialog* ui;
    QFont font_;
};

}  // namespace Texxy

#endif  // FONTDIALOG_H
