

#ifndef PREF_H
#define PREF_H

#include <QDialog>
#include <QStyledItemDelegate>
#include <QTableWidgetItem>
#include <QKeySequenceEdit>
#include "config.h"

namespace Texxy {

class TexxyKeySequenceEdit : public QKeySequenceEdit {
    Q_OBJECT

   public:
    TexxyKeySequenceEdit(QWidget* parent = nullptr);

    /* to be used with Tab and Backtab */
    void pressKey(QKeyEvent* event) { QKeySequenceEdit::keyPressEvent(event); }

   protected:
    virtual void keyPressEvent(QKeyEvent* event);
};

class Delegate : public QStyledItemDelegate {
    Q_OBJECT

   public:
    Delegate(QObject* parent = nullptr);

    virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const;

   protected:
    virtual bool eventFilter(QObject* object, QEvent* event);
};

namespace Ui {
class PrefDialog;
}

class PrefDialog : public QDialog {
    Q_OBJECT

   public:
    explicit PrefDialog(QWidget* parent = nullptr);
    ~PrefDialog();

   private slots:
    void onClosing();
    void prefSize(int checked);
    void prefStartSize(int value);
    void prefPos(int checked);
    void prefToolbar(int checked);
    void prefMenubar(int checked);
    void prefMenubarTitle(int checked);
    void prefSearchbar(int checked);
    void prefSearchHistory(int checked);
    void prefStatusbar(int checked);
    void prefStatusCursor(int checked);
    void prefFont(int checked);
    void prefWrap(int checked);
    void prefIndent(int checked);
    void prefAutoBracket(int checked);
    void prefAutoReplace(int checked);
    void prefLine(int checked);
    void prefSyntax(int checked);
    void prefWhiteSpace(int checked);
    void prefVLine(int checked);
    void prefVLineDistance(int value);
    void prefEndings(int checked);
    void prefTextMargin(int checked);
    void prefDarkColScheme(int checked);
    void prefColValue(int value);
    void prefAppendEmptyLine(int checked);
    void prefRemoveTrailingSpaces(int checked);
    void prefSkipNontext(int checked);
    void prefTabWrapAround(int checked);
    void prefHideSingleTab(int checked);
    void prefMaxSHSize(int value);
    void prefExecute(int checked);
    void prefCommand(const QString& command);
    void prefRecentFilesNumber(int value);
    void prefSaveLastFilesList(int checked);
    void prefOpenInWindows(int checked);
    void prefNativeDialog(int checked);
    void prefSidePaneMode(int checked);
    void prefSplitterPos(int checked);
    void prefInertialScrolling(int checked);
    void showWhatsThis();
    void prefShortcuts();
    void restoreDefaultShortcuts();
    void onShortcutChange(QTableWidgetItem* item);
    void prefAutoSave(int checked);
    void prefSaveUnmodified();
    void prefTextTabSize(int value);
    void prefTextTab();
    void prefCloseWithLastTab(int checked);
    void changeSyntaxColor(int row, int column);
    void changeWhitespaceValue(int value);
    void restoreDefaultSyntaxColors();
    void changeCurLineHighlight(int value);
    void disableMenubarAccel(int checked);
    void prefIcon(int checked);

   protected:
    bool eventFilter(QObject* object, QEvent* event);

   private:
    void closeEvent(QCloseEvent* event);
    void prefTabPosition();
    void prefRecentFilesKind();
    void prefApplyAutoSave();
    void prefApplySyntax();
    void prefApplyDateFormat();
    void prefThickCursor();
    void prefSelHighlight();
    void prefPastePaths();
    void showPrompt(const QString& str = QString(), bool temporary = false);

    Ui::PrefDialog* ui;
    QWidget* parent_;
    bool darkBg_, showWhiteSpace_, showEndings_, textMargin_, saveUnmodified_, sharedSearchHistory_, selHighlighting_,
        pastePaths_, disableMenubarAccel_, sysIcons_;
    int vLineDistance_, darkColValue_, lightColValue_, recentNumber_, textTabSize_, whiteSpaceValue_, curLineHighlight_;
    QHash<QString, QString> shortcuts_, newShortcuts_;
    QString prevtMsg_;
    QTimer* promptTimer_;
    QHash<QString, QColor> prefCustomSyntaxColors_;  // customization in Preferences
    QHash<QString, QColor> origSyntaxColors_;        // to know if a syntax color is really changed
};

}  // namespace Texxy

#endif  // PREF_H
