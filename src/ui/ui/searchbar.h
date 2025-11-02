

#ifndef SEARCHBAR_H
#define SEARCHBAR_H

#include <QPointer>
#include <QToolButton>
#include <QComboBox>
#include <QStandardItemModel>
#include "ui/lineedit.h"

namespace Texxy {

class ComboBox : public QComboBox {
    Q_OBJECT
   public:
    enum Move { NoMove = 0, MoveUp, MoveDown, MoveFirst, MoveLast };

    ComboBox(QWidget* parent = nullptr);
    ~ComboBox() {}

   signals:
    void moveInHistory(int move);

   protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
};

class SearchBar : public QFrame {
    Q_OBJECT
   public:
    SearchBar(QWidget* parent = nullptr,
              const QList<QKeySequence>& shortcuts = QList<QKeySequence>(),
              Qt::WindowFlags f = Qt::WindowFlags());

    void setSearchModel(QStandardItemModel* model);
    void focusLineEdit();
    bool lineEditHasFocus() const;
    QString searchEntry() const;
    void clearSearchEntry();

    bool matchCase() const;
    bool matchWhole() const;
    bool matchRegex() const;

    void updateShortcuts(bool disable);

   signals:
    void searchFlagChanged();
    void find(bool forward);

   private:
    void searchStarted();
    void findForward();
    void findBackward();

    QPointer<LineEdit> lineEdit_;
    QPointer<ComboBox> combo_;
    QPointer<QToolButton> toolButton_nxt_;
    QPointer<QToolButton> toolButton_prv_;
    QPointer<QToolButton> button_case_;
    QPointer<QToolButton> button_whole_;
    QPointer<QToolButton> button_regex_;
    QList<QKeySequence> shortcuts_;
    bool searchStarted_;
    QString searchText_;
};

}  // namespace Texxy

#endif  // SEARCHBAR_H
