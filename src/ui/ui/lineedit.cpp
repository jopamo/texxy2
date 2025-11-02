// src/ui/ui/lineedit.cpp
/*
 * texxy/lineedit.cpp
 */

#include "ui/lineedit.h"
#include <QToolButton>
#include <QAbstractButton>
#include <QKeyEvent>
#include <algorithm>  // std::find_if
#include <utility>    // std::as_const

namespace Texxy {

LineEdit::LineEdit(QWidget* parent) : QLineEdit(parent) {
    setClearButtonEnabled(true);

    // single child lookup and std::find_if to pick the clear button if it exists
    const auto buttons = findChildren<QToolButton*>();
    if (buttons.isEmpty())
        return;

    const auto it = std::find_if(buttons.cbegin(), buttons.cend(), [](const QToolButton* b) {
        return b && b->objectName().contains(QStringLiteral("qt_edit_clear"), Qt::CaseInsensitive);
    });
    QToolButton* clearButton = (it != buttons.cend()) ? *it : buttons.constFirst();

    clearButton->setToolTip(tr("Clear text (Ctrl+K)"));
    // connect to returnPressed to clear any match highlights upstream
    connect(clearButton, &QAbstractButton::clicked, this, &LineEdit::returnPressed);
}
/*************************/
void LineEdit::keyPressEvent(QKeyEvent* event) {
    // work around Qt bug where ZWNJ might not be inserted with Shift+Space on some platforms
    if (event->key() == 0x200C) {
        insert(QChar(0x200C));
        event->accept();
        return;
    }

    const auto mods = event->modifiers();
    const int key = event->key();

    // bit test for modifiers because Qt may set additional flags like KeypadModifier
    if (mods & Qt::ControlModifier) {
        // if embedded in a combo, Ctrl+Up/Down should open the popup
        if (key == Qt::Key_Up || key == Qt::Key_Down) {
            emit showComboPopup();
            event->accept();
            return;
        }
        // Ctrl+K clears the line edit and triggers returnPressed to clear highlights
        if (key == Qt::Key_K) {
            clear();
            emit returnPressed();
            event->accept();
            return;
        }
    }

    QLineEdit::keyPressEvent(event);
}
/*************************/
void LineEdit::focusInEvent(QFocusEvent* ev) {
    // first do what QLineEdit does
    QLineEdit::focusInEvent(ev);
    emit receivedFocus();
}

}  // namespace Texxy
