// src/ui/ui/lineedit.h
/*
 * texxy/lineedit.h
 */

#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <QLineEdit>
#include <QKeyEvent>
#include <QFocusEvent>

namespace Texxy {

class LineEdit : public QLineEdit {
    Q_OBJECT
   public:
    explicit LineEdit(QWidget* parent = nullptr);
    ~LineEdit() override = default;

    // helper for forwarding synthetic key events in tests or higher-level widgets
    void pressKey(QKeyEvent* event) { keyPressEvent(event); }

   signals:
    void receivedFocus();
    void showComboPopup();  // in case it belongs to a combo box

   protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* ev) override;

   private:
    Q_DISABLE_COPY_MOVE(LineEdit);
};

}  // namespace Texxy

#endif  // LINEEDIT_H
