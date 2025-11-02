

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
