

#ifndef TABPAGE_H
#define TABPAGE_H

#include <QPointer>
#include "ui/searchbar.h"
#include "textedit/textedit.h"

namespace Texxy {

class TabPage : public QWidget {
    Q_OBJECT
   public:
    TabPage(int bgColorValue = 255,
            const QList<QKeySequence>& searchShortcuts = QList<QKeySequence>(),
            QWidget* parent = nullptr);

    QPointer<TextEdit> textEdit() const { return textEdit_; }

    void setSearchModel(QStandardItemModel* model) { searchBar_->setSearchModel(model); }

    void setSearchBarVisible(bool visible);
    bool isSearchBarVisible() const;
    void focusSearchBar();
    bool searchBarHasFocus() const;

    QString searchEntry() const;
    void clearSearchEntry();

    bool matchCase() const;
    bool matchWhole() const;
    bool matchRegex() const;

    void updateShortcuts(bool disable);

    void lockPage(bool lock);

   signals:
    void find(bool forward);
    void searchFlagChanged();

   private:
    QPointer<TextEdit> textEdit_;
    QPointer<SearchBar> searchBar_;
};

}  // namespace Texxy

#endif  // TABPAGE_H
