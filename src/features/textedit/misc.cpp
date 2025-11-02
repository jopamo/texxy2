// src/features/textedit/misc.cpp
#include "textedit/textedit_prelude.h"

namespace Texxy {

/*************************/
bool TextEdit::eventFilter(QObject* watched, QEvent* event) {
    if (watched == lineNumberArea_) {
        if (event->type() == QEvent::Wheel) {
            if (QWheelEvent* we = static_cast<QWheelEvent*>(event)) {
                wheelEvent(we);
                return false;
            }
        }
        else if (event->type() == QEvent::MouseButtonPress)
            return true;  // prevent the window from being dragged by widget styles (like Kvantum)
    }
    return QPlainTextEdit::eventFilter(watched, event);
}

}  // namespace Texxy
