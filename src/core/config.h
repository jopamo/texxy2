/*
 * texxy/config.h
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <QSettings>
#include <QSize>
#include <QPoint>
#include <QFont>
#include <QColor>
#include <QVariant>
#include <QStringList>
#include <QHash>

#include <algorithm>

namespace Texxy {

// Prevent redundant writings! (Why does QSettings write to the config file when no setting is changed?)
class Settings : public QSettings {
    Q_OBJECT
   public:
    Settings(const QString& organization, const QString& application = QString(), QObject* parent = nullptr)
        : QSettings(organization, application, parent) {}
    Settings(const QString& fileName, QSettings::Format format, QObject* parent = nullptr)
        : QSettings(fileName, format, parent) {}

    // QSettings::setValue is not virtual in Qt, so do not mark this as override
    // this wrapper skips writes when the value is unchanged
    void setValue(const QString& key, const QVariant& v) {
        if (value(key) == v)
            return;
        QSettings::setValue(key, v);
    }
};

class Config {
   public:
    // centralized limits for guards and UI ranges
    static constexpr int kRecentFilesMax = 50;
    static constexpr int kMaxTabPos = 3;  // 0..3 where 0 = default platform
    static constexpr int kMinTabPos = 0;

    Config();
    ~Config();

    void readConfig();
    void readShortcuts();
    void writeConfig();

    [[nodiscard]] bool getRemSize() const { return remSize_; }
    void setRemSize(bool rem) { remSize_ = rem; }

    [[nodiscard]] bool getRemPos() const { return remPos_; }
    void setRemPos(bool rem) { remPos_ = rem; }

    [[nodiscard]] bool getRemSplitterPos() const { return remSplitterPos_; }
    void setRemSplitterPos(bool rem) { remSplitterPos_ = rem; }

    [[nodiscard]] bool getIsMaxed() const { return isMaxed_; }
    void setIsMaxed(bool isMaxed) { isMaxed_ = isMaxed; }

    [[nodiscard]] bool getIsFull() const { return isFull_; }
    void setIsFull(bool isFull) { isFull_ = isFull; }

    [[nodiscard]] bool getDarkColScheme() const { return darkColScheme_; }
    void setDarkColScheme(bool dark) { darkColScheme_ = dark; }

    [[nodiscard]] bool getThickCursor() const { return thickCursor_; }
    void setThickCursor(bool thick) { thickCursor_ = thick; }

    [[nodiscard]] int getLightBgColorValue() const { return lightBgColorValue_; }
    void setLightBgColorValue(int lightness) { lightBgColorValue_ = lightness; }

    [[nodiscard]] int getDarkBgColorValue() const { return darkBgColorValue_; }
    void setDarkBgColorValue(int darkness) { darkBgColorValue_ = darkness; }

    [[nodiscard]] QString getDateFormat() const { return dateFormat_; }
    void setDateFormat(const QString& format) { dateFormat_ = format; }

    [[nodiscard]] int getTextTabSize() const { return textTabSize_; }
    void setTextTabSize(int textTab) { textTabSize_ = textTab; }

    [[nodiscard]] int getDefaultRecentFilesNumber() const { return 10; }
    [[nodiscard]] int getRecentFilesNumber() const { return recentFilesNumber_; }
    void setRecentFilesNumber(int number) { recentFilesNumber_ = std::clamp(number, 0, kRecentFilesMax); }
    [[nodiscard]] int getCurRecentFilesNumber() const { return curRecentFilesNumber_; }

    [[nodiscard]] bool getTabWrapAround() const { return tabWrapAround_; }
    void setTabWrapAround(bool wrap) { tabWrapAround_ = wrap; }

    [[nodiscard]] bool getHideSingleTab() const { return hideSingleTab_; }
    void setHideSingleTab(bool hide) { hideSingleTab_ = hide; }

    [[nodiscard]] QSize getWinSize() const { return winSize_; }
    void setWinSize(const QSize& s) { winSize_ = s; }

    [[nodiscard]] QSize getPrefSize() const { return prefSize_; }
    void setPrefSize(const QSize& s) { prefSize_ = s; }

    [[nodiscard]] QSize getDefaultStartSize() const { return QSize(700, 500); }
    [[nodiscard]] QSize getStartSize() const { return startSize_; }
    void setStartSize(const QSize& s) { startSize_ = s; }

    [[nodiscard]] QPoint getWinPos() const { return winPos_; }
    void setWinPos(const QPoint& p) { winPos_ = p; }

    [[nodiscard]] int getSplitterPos() const { return splitterPos_; }
    void setSplitterPos(int pos) { splitterPos_ = pos; }

    [[nodiscard]] bool getNoToolbar() const { return noToolbar_; }
    void setNoToolbar(bool noTB) { noToolbar_ = noTB; }

    [[nodiscard]] bool getNoMenubar() const { return noMenubar_; }
    void setNoMenubar(bool noMB) { noMenubar_ = noMB; }

    [[nodiscard]] bool getMenubarTitle() const { return menubarTitle_; }
    void setMenubarTitle(bool mt) { menubarTitle_ = mt; }

    [[nodiscard]] bool getHideSearchbar() const { return hideSearchbar_; }
    void setHideSearchbar(bool hide) { hideSearchbar_ = hide; }

    [[nodiscard]] bool getShowStatusbar() const { return showStatusbar_; }
    void setShowStatusbar(bool show) { showStatusbar_ = show; }

    [[nodiscard]] bool getShowCursorPos() const { return showCursorPos_; }
    void setShowCursorPos(bool show) { showCursorPos_ = show; }

    [[nodiscard]] bool getShowLangSelector() const { return showLangSelector_; }
    void setShowLangSelector(bool show) { showLangSelector_ = show; }

    [[nodiscard]] bool getSidePaneMode() const { return sidePaneMode_; }
    void setSidePaneMode(bool sidePane) { sidePaneMode_ = sidePane; }

    [[nodiscard]] int getTabPosition() const { return tabPosition_; }
    void setTabPosition(int pos) { tabPosition_ = std::clamp(pos, kMinTabPos, kMaxTabPos); }

    [[nodiscard]] QFont getFont() const { return font_; }
    void setFont(const QFont& font) { font_ = font; }
    void resetFont();

    [[nodiscard]] bool getRemFont() const { return remFont_; }
    void setRemFont(bool rem) { remFont_ = rem; }

    [[nodiscard]] bool getWrapByDefault() const { return wrapByDefault_; }
    void setWrapByDefault(bool wrap) { wrapByDefault_ = wrap; }

    [[nodiscard]] bool getIndentByDefault() const { return indentByDefault_; }
    void setIndentByDefault(bool indent) { indentByDefault_ = indent; }

    [[nodiscard]] bool getAutoReplace() const { return autoReplace_; }
    void setAutoReplace(bool autoR) { autoReplace_ = autoR; }

    [[nodiscard]] bool getAutoBracket() const { return autoBracket_; }
    void setAutoBracket(bool autoB) { autoBracket_ = autoB; }

    [[nodiscard]] bool getLineByDefault() const { return lineByDefault_; }
    void setLineByDefault(bool line) { lineByDefault_ = line; }

    [[nodiscard]] bool getSyntaxByDefault() const { return syntaxByDefault_; }
    void setSyntaxByDefault(bool syntax) { syntaxByDefault_ = syntax; }

    [[nodiscard]] bool getShowWhiteSpace() const { return showWhiteSpace_; }
    void setShowWhiteSpace(bool show) { showWhiteSpace_ = show; }

    [[nodiscard]] bool getShowEndings() const { return showEndings_; }
    void setShowEndings(bool show) { showEndings_ = show; }

    [[nodiscard]] bool getTextMargin() const { return textMargin_; }
    void setTextMargin(bool margin) { textMargin_ = margin; }

    [[nodiscard]] int getDefaultVLineDistance() const { return 80; }
    [[nodiscard]] int getVLineDistance() const { return vLineDistance_; }
    void setVLineDistance(int distance) { vLineDistance_ = distance; }

    [[nodiscard]] int getDefaultMaxSHSize() const { return 2; }
    [[nodiscard]] int getMaxSHSize() const { return maxSHSize_; }
    void setMaxSHSize(int max) { maxSHSize_ = max; }

    [[nodiscard]] bool getSkipNonText() const { return skipNonText_; }
    void setSkipNonText(bool skip) { skipNonText_ = skip; }
    /*************************/
    [[nodiscard]] bool getExecuteScripts() const { return executeScripts_; }
    void setExecuteScripts(bool execute) { executeScripts_ = execute; }
    [[nodiscard]] QString getExecuteCommand() const { return executeCommand_; }
    void setExecuteCommand(const QString& command) { executeCommand_ = command; }
    /*************************/
    [[nodiscard]] bool getAppendEmptyLine() const { return appendEmptyLine_; }
    void setAppendEmptyLine(bool append) { appendEmptyLine_ = append; }

    [[nodiscard]] bool getRemoveTrailingSpaces() const { return removeTrailingSpaces_; }
    void setRemoveTrailingSpaces(bool remove) { removeTrailingSpaces_ = remove; }

    [[nodiscard]] bool getOpenInWindows() const { return openInWindows_; }
    void setOpenInWindows(bool windows) { openInWindows_ = windows; }

    [[nodiscard]] bool getNativeDialog() const { return nativeDialog_; }
    void setNativeDialog(bool native) { nativeDialog_ = native; }
    /*************************/
    [[nodiscard]] bool getRecentOpened() const { return recentOpened_; }
    void setRecentOpened(bool opened) { recentOpened_ = opened; }

    [[nodiscard]] QStringList getRecentFiles() const { return recentFiles_; }
    void clearRecentFiles() { recentFiles_.clear(); }
    void addRecentFile(const QString& file);
    /*************************/
    [[nodiscard]] QHash<QString, QString> customShortcutActions() const { return actions_; }
    void setActionShortcut(const QString& action, const QString& shortcut) { actions_.insert(action, shortcut); }
    void removeShortcut(const QString& action) {
        actions_.remove(action);
        removedActions_ << action;
    }

    [[nodiscard]] bool hasReservedShortcuts() const { return (!reservedShortcuts_.isEmpty()); }
    [[nodiscard]] QStringList reservedShortcuts() const { return reservedShortcuts_; }
    void setReservedShortcuts(const QStringList& s) { reservedShortcuts_ = s; }
    /*************************/
    [[nodiscard]] bool getInertialScrolling() const { return inertialScrolling_; }
    void setInertialScrolling(bool inertial) { inertialScrolling_ = inertial; }
    /*************************/
    [[nodiscard]] QHash<QString, QVariant> savedCursorPos() {
        readCursorPos();
        return cursorPos_;
    }
    void saveCursorPos(const QString& name, int pos) {
        readCursorPos();
        if (removedCursorPos_.contains(name))
            removedCursorPos_.removeOne(name);
        else
            cursorPos_.insert(name, pos);
    }
    void removeCursorPos(const QString& name) {
        readCursorPos();
        cursorPos_.remove(name);
        removedCursorPos_ << name;
    }
    void removeAllCursorPos() {
        readCursorPos();
        removedCursorPos_.append(cursorPos_.keys());
        cursorPos_.clear();
    }
    /*************************/
    [[nodiscard]] bool getSaveLastFilesList() const { return saveLastFilesList_; }
    void setSaveLastFilesList(bool saveList) { saveLastFilesList_ = saveList; }

    // may be called only at session start and sets lasFilesCursorPos_
    [[nodiscard]] QStringList getLastFiles();
    // is called only after getLastFiles()
    [[nodiscard]] QHash<QString, QVariant> getLastFilesCursorPos() const { return lasFilesCursorPos_; }
    void setLastFileCursorPos(const QHash<QString, QVariant>& curPos) { lasFilesCursorPos_ = curPos; }
    /*************************/
    [[nodiscard]] bool getAutoSave() const { return autoSave_; }
    void setAutoSave(bool as) { autoSave_ = as; }
    [[nodiscard]] int getAutoSaveInterval() const { return autoSaveInterval_; }
    void setAutoSaveInterval(int i) { autoSaveInterval_ = i; }
    /*************************
     */
    [[nodiscard]] bool getSaveUnmodified() const { return saveUnmodified_; }
    void setSaveUnmodified(bool save) { saveUnmodified_ = save; }
    /*************************/
    [[nodiscard]] bool getSelectionHighlighting() const { return selectionHighlighting_; }
    void setSelectionHighlighting(bool enable) { selectionHighlighting_ = enable; }
    /*************************/
    [[nodiscard]] bool getPastePaths() const { return pastePaths_; }
    void setPastePaths(bool pastPaths) { pastePaths_ = pastPaths; }
    /*************************/
    [[nodiscard]] bool getCloseWithLastTab() const { return closeWithLastTab_; }
    void setCloseWithLastTab(bool close) { closeWithLastTab_ = close; }
    /*************************/
    [[nodiscard]] bool getSharedSearchHistory() const { return sharedSearchHistory_; }
    void setSharedSearchHistory(bool share) { sharedSearchHistory_ = share; }
    /*************************/
    [[nodiscard]] bool getDisableMenubarAccel() const { return disableMenubarAccel_; }
    void setDisableMenubarAccel(bool disable) { disableMenubarAccel_ = disable; }
    /*************************/
    [[nodiscard]] bool getSysIcons() const { return sysIcons_; }
    void setSysIcons(bool sysIcons) { sysIcons_ = sysIcons; }
    /*************************/
    /*************************/
    [[nodiscard]] QHash<QString, QColor> lightSyntaxColors() const { return defaultLightSyntaxColors_; }
    [[nodiscard]] QHash<QString, QColor> darkSyntaxColors() const { return defaultDarkSyntaxColors_; }

    [[nodiscard]] QHash<QString, QColor> customSyntaxColors() const { return customSyntaxColors_; }
    void setCustomSyntaxColors(const QHash<QString, QColor>& colors) { customSyntaxColors_ = colors; }

    [[nodiscard]] int getDefaultWhiteSpaceValue() const { return darkColScheme_ ? 95 : 180; }
    [[nodiscard]] int getMinWhiteSpaceValue() const { return darkColScheme_ ? 50 : 130; }
    [[nodiscard]] int getMaxWhiteSpaceValue() const { return darkColScheme_ ? 140 : 230; }
    [[nodiscard]] int getWhiteSpaceValue() const { return whiteSpaceValue_; }
    void setWhiteSpaceValue(int value);

    [[nodiscard]] int getCurLineHighlight() const { return curLineHighlight_; }
    [[nodiscard]] int getMinCurLineHighlight() const { return darkColScheme_ ? 0 : 210; }
    [[nodiscard]] int getMaxCurLineHighlight() const { return darkColScheme_ ? 70 : 255; }
    void setCurLineHighlight(int value);

    void readSyntaxColors();

   private:
    QString validatedShortcut(const QVariant v, bool* isValid);
    void readCursorPos();
    void writeCursorPos();
    void setDfaultSyntaxColors();
    void writeSyntaxColors();

    bool remSize_, remPos_, remSplitterPos_, noToolbar_, noMenubar_, menubarTitle_, hideSearchbar_, showStatusbar_,
        showCursorPos_, showLangSelector_, sidePaneMode_, remFont_, wrapByDefault_, indentByDefault_, autoReplace_,
        autoBracket_, lineByDefault_, syntaxByDefault_, showWhiteSpace_, showEndings_, textMargin_, isMaxed_, isFull_,
        darkColScheme_, thickCursor_, tabWrapAround_, hideSingleTab_, executeScripts_, appendEmptyLine_,
        removeTrailingSpaces_, openInWindows_, nativeDialog_, inertialScrolling_, autoSave_, skipNonText_,
        saveUnmodified_, selectionHighlighting_, pastePaths_, closeWithLastTab_, sharedSearchHistory_,
        disableMenubarAccel_, sysIcons_;
    int vLineDistance_, tabPosition_, maxSHSize_, lightBgColorValue_, darkBgColorValue_, recentFilesNumber_,
        curRecentFilesNumber_, autoSaveInterval_, textTabSize_;
    QString dateFormat_;
    QSize winSize_, startSize_, prefSize_;
    QPoint winPos_;
    int splitterPos_;
    QFont font_;
    QString executeCommand_;
    bool recentOpened_;
    QStringList recentFiles_;
    bool saveLastFilesList_;
    QHash<QString, QString> actions_;
    QStringList removedActions_, reservedShortcuts_;

    QHash<QString, QVariant> cursorPos_;
    QStringList removedCursorPos_;
    bool cursorPosRetrieved_;

    QHash<QString, QVariant> lasFilesCursorPos_;

    QHash<QString, QColor> defaultLightSyntaxColors_, defaultDarkSyntaxColors_, customSyntaxColors_;
    int whiteSpaceValue_, curLineHighlight_;
};

}  // namespace Texxy

#endif  // CONFIG_H
