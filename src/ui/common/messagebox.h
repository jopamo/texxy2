// src/ui/common/messagebox.h

#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <QMessageBox>
#include <QAbstractButton>
#include <QGridLayout>
#include <QRegularExpression>

#include <algorithm>

namespace Texxy {

/* QMessageBox::setButtonText() is obsolete while we want custom texts, especially
   for translation. We also want to set the width according to the informative text. */
class MessageBox : public QMessageBox {
    Q_OBJECT
   public:
    MessageBox(QWidget* parent = nullptr) : QMessageBox(parent) {}
    MessageBox(Icon icon,
               const QString& title,
               const QString& text,
               StandardButtons buttons = NoButton,
               QWidget* parent = nullptr,
               Qt::WindowFlags f = Qt::Dialog /* | Qt::MSWindowsFixedSizeDialogHint*/)
        : QMessageBox(icon, title, text, buttons, parent, f) {}

    void changeButtonText(QMessageBox::StandardButton btn, const QString& text) {
        if (QAbstractButton* abtn = button(btn))
            abtn->setText(text);
    }

    void setInformativeText(const QString& text) {
        QMessageBox::setInformativeText(text);

        if (!text.isEmpty())  // set an appropriate width
        {
            if (QGridLayout* lo = qobject_cast<QGridLayout*>(layout())) {
                int tw = 0;
                QString t(text);
                /* we suppose that <p> isn't used inside the text */
                t.remove(
                    QRegularExpression("</{0,1}center>|</{0,1}b>|</{0,1}i>|</{0,1}p>|</"
                                       "a>|<a\\s+href\\s*=\\s*[A-Za-z0-9_%@:'\\.\\?\\=]+>"));
                t.replace("<br>", "\n");
                QStringList l = t.split('\n');  // deal with newlines
                for (int i = 0; i < l.size(); i++)
                    tw = std::max(tw, QFontMetrics(font()).horizontalAdvance(l[i]));

                lo->setColumnMinimumWidth(lo->columnCount() - 1, tw + 10);
            }
        }
    }
};

}  // namespace Texxy

#endif  // MESSAGEBOX_H
