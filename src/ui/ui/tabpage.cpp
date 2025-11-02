

#include <QGridLayout>
#include "ui/tabpage.h"

namespace Texxy {

TabPage::TabPage(int bgColorValue, const QList<QKeySequence>& searchShortcuts, QWidget* parent) : QWidget(parent) {
    textEdit_ = new TextEdit(this, bgColorValue);
    searchBar_ = new SearchBar(this, searchShortcuts);

    QGridLayout* mainGrid = new QGridLayout;
    mainGrid->setVerticalSpacing(4);
    mainGrid->setContentsMargins(0, 0, 0, 0);
    mainGrid->addWidget(textEdit_, 0, 0);
    mainGrid->addWidget(searchBar_, 1, 0);
    setLayout(mainGrid);

    connect(searchBar_, &SearchBar::find, this, &TabPage::find);
    connect(searchBar_, &SearchBar::searchFlagChanged, this, &TabPage::searchFlagChanged);
}
/*************************/
void TabPage::setSearchBarVisible(bool visible) {
    searchBar_->setVisible(visible);
}
/*************************/
bool TabPage::isSearchBarVisible() const {
    return searchBar_->isVisible();
}
/*************************/
void TabPage::focusSearchBar() {
    searchBar_->focusLineEdit();
}
/*************************/
bool TabPage::searchBarHasFocus() const {
    return searchBar_->lineEditHasFocus();
}
/*************************/
QString TabPage::searchEntry() const {
    return searchBar_->searchEntry();
}
/*************************/
void TabPage::clearSearchEntry() {
    return searchBar_->clearSearchEntry();
}
/*************************/
bool TabPage::matchCase() const {
    return searchBar_->matchCase();
}
/*************************/
bool TabPage::matchWhole() const {
    return searchBar_->matchWhole();
}
/*************************/
bool TabPage::matchRegex() const {
    return searchBar_->matchRegex();
}
/*************************/
void TabPage::updateShortcuts(bool disable) {
    searchBar_->updateShortcuts(disable);
}
/*************************/
void TabPage::lockPage(bool lock) {
    searchBar_->setEnabled(!lock);
    textEdit_->setEnabled(!lock);
}

}  // namespace Texxy
