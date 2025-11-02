// src/features/highlighter/textblockdata.cpp
/*
 * texxy/highlighter/textblockdata.cpp
 */

#include "highlighter.h"

namespace {

template <typename T>
void clearPointerList(QList<T*>& container) {
    for (T* ptr : container) {
        delete ptr;
    }
    container.clear();
}

template <typename T>
void insertByPosition(QList<T*>& container, T* info) {
    int index = 0;
    while (index < container.size() && info->position > container.at(index)->position) {
        ++index;
    }
    container.insert(index, info);
}

}  // namespace

namespace Texxy {

TextBlockData::~TextBlockData() {
    clearPointerList(allParentheses);
    clearPointerList(allBraces);
    clearPointerList(allBrackets);
}
/*************************/
QList<ParenthesisInfo*> TextBlockData::parentheses() const {
    return allParentheses;
}
/*************************/
QList<BraceInfo*> TextBlockData::braces() const {
    return allBraces;
}
/*************************/
QList<BracketInfo*> TextBlockData::brackets() const {
    return allBrackets;
}
/*************************/
QString TextBlockData::labelInfo() const {
    return label;
}
/*************************/
bool TextBlockData::isHighlighted() const {
    return Highlighted;
}
/*************************/
bool TextBlockData::getProperty() const {
    return Property;
}
/*************************/
int TextBlockData::lastState() const {
    return LastState;
}
/*************************/
int TextBlockData::openNests() const {
    return OpenNests;
}
/*************************/
int TextBlockData::lastFormattedQuote() const {
    return LastFormattedQuote;
}
/*************************/
int TextBlockData::lastFormattedRegex() const {
    return LastFormattedRegex;
}
/*************************/
QSet<int> TextBlockData::openQuotes() const {
    return OpenQuotes;
}
/*************************/
void TextBlockData::insertInfo(ParenthesisInfo* info) {
    insertByPosition(allParentheses, info);
}
/*************************/
void TextBlockData::insertInfo(BraceInfo* info) {
    insertByPosition(allBraces, info);
}
/*************************/
void TextBlockData::insertInfo(BracketInfo* info) {
    insertByPosition(allBrackets, info);
}
/*************************/
void TextBlockData::insertInfo(const QString& str) {
    label = str;
}
/*************************/
void TextBlockData::setHighlighted() {
    Highlighted = true;
}
/*************************/
void TextBlockData::setProperty(bool p) {
    Property = p;
}
/*************************/
void TextBlockData::setLastState(int state) {
    LastState = state;
}
/*************************/
void TextBlockData::insertNestInfo(int nests) {
    OpenNests = nests;
}
/*************************/
void TextBlockData::insertLastFormattedQuote(int last) {
    LastFormattedQuote = last;
}
/*************************/
void TextBlockData::insertLastFormattedRegex(int last) {
    LastFormattedRegex = last;
}
/*************************/
void TextBlockData::insertOpenQuotes(const QSet<int>& openQuotes) {
    OpenQuotes.unite(openQuotes);
}

}  // namespace Texxy
