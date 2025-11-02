// src/core/session.h

#ifndef SESSION_H
#define SESSION_H

#include <QDialog>
#include <QTimer>

namespace Texxy {

namespace Ui {
class SessionDialog;
}

class SessionDialog : public QDialog {
    Q_OBJECT

   public:
    explicit SessionDialog(QWidget* parent = nullptr);
    ~SessionDialog();

   protected:
    bool eventFilter(QObject* watched, QEvent* event);

   private slots:
    void showContextMenu(const QPoint& p);
    void saveSession();
    void reallySaveSession();
    void selectionChanged();
    void openSessions();
    void closePrompt();
    void removeSelected();
    void removeAll();
    void showMainPage();
    void showPromptPage();
    void renameSession();
    void reallyRenameSession();
    void OnCommittingName(QWidget* editor);
    void filter(const QString&);
    void reallyApplyFilter();

   private:
    enum PROMPT { NAME, RENAME, REMOVE, CLEAR };

    struct Rename {
        QString oldName;
        QString newName;
    };

    void showPrompt(PROMPT prompt);
    void showPrompt(const QString& message);
    void onEmptinessChanged(bool empty);

    Ui::SessionDialog* ui;
    QWidget* parent_;
    Rename rename_;
    /* Used only for filtering: */
    QStringList allItems_;
    QTimer* filterTimer_;
};

}  // namespace Texxy

#endif  // SESSION_H
