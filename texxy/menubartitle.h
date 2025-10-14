#ifndef MENUBARTITLE_H
#define MENUBARTITLE_H

#include <QLabel>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QResizeEvent>  // for resizeEvent

namespace Texxy {

class MenuBarTitle : public QLabel {
    Q_OBJECT

   public:
    explicit MenuBarTitle(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

    void setTitle(const QString& title);
    void setStart(int s) { start_ = s; }
    void setHeightOverride(int h) { heightOverride_ = h; }

   signals:
    void doubleClicked();

   protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QSize sizeHint() const override;
    void resizeEvent(QResizeEvent* event) override;

   private:
    void updateElidedText();  // helper

    QString elidedText_;
    QString lastText_;
    int lastWidth_ = 0;
    int start_ = 0;
    int heightOverride_ = 0;
};

}  // namespace Texxy

#endif  // MENUBARTITLE_H
