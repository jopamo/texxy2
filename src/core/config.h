// src/core/config.h
/*
 * texxy/config.h
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <QColor>
#include <QFont>
#include <QHash>
#include <QPoint>
#include <QSettings>
#include <QSize>
#include <QStringList>
#include <QVariant>

#include <algorithm>

namespace Texxy {

// Prevent redundant writings
// QSettings writes even when unchanged on some backends
class Settings : public QSettings {
    Q_OBJECT
   public:
    Settings(const QString& organization, const QString& application = QString(), QObject* parent = nullptr)
        : QSettings(organization, application, parent) {}
    Settings(const QString& fileName, QSettings::Format format, QObject* parent = nullptr)
        : QSettings(fileName, format, parent) {}

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

    [[nodiscard]] bool getRemSize() const noexcept { return remSize_; }
    void setRemSize(bool rem) noexcept { remSize_ = rem; }

    [[nodiscard]] bool getRemPos() const noexcept { return remPos_; }
    void setRemPos(bool rem) noexcept { remPos_ = rem; }

    [[nodiscard]] bool getRemSplitterPos() const noexcept { return remSplitterPos_; }
    void setRemSplitterPos(bool rem) noexcept { remSplitterPos_ = rem; }

    [[nodiscard]] bool getIsMaxed() const noexcept { return isMaxed_; }
    void setIsMaxed(bool isMaxed) noexcept { isMaxed_ = isMaxed; }

    [[nodiscard]] bool getIsFull() const noexcept { return isFull_; }
    void setIsFull(bool isFull) noexcept { isFull_ = isFull; }

    [[nodiscard]] bool getDarkColScheme() const noexcept { return darkColScheme_; }
    void setDarkColScheme(bool dark) noexcept { darkColScheme_ = dark; }

    [[nodiscard]] bool getThickCursor() const noexcept { return thickCursor_; }
    void setThickCursor(bool thick) noexcept { thickCursor_ = thick; }

    [[nodiscard]] int getLightBgColorValue() const noexcept { return lightBgColorValue_; }
    void setLightBgColorValue(int lightness) noexcept { lightBgColorValue_ = lightness; }

    [[nodiscard]] int getDarkBgColorValue() const noexcept { return darkBgColorValue_; }
    void setDarkBgColorValue(int darkness) noexcept { darkBgColorValue_ = darkness; }

    [[nodiscard]] QString getDateFormat() const { return dateFormat_; }
    void setDateFormat(const QString& format) { dateFormat_ = format; }

    [[nodiscard]] int getTextTabSize() const noexcept { return textTabSize_; }
    void setTextTabSize(int textTab) noexcept { textTabSize_ = textTab; }

    [[nodiscard]] int getDefaultRecentFilesNumber() const noexcept { return 10; }
    [[nodiscard]] int getRecentFilesNumber() const noexcept { return recentFilesNumber_; }
    void setRecentFilesNumber(int number) noexcept { recentFilesNumber_ = std::clamp(number, 0, kRecentFilesMax); }
    [[nodiscard]] int getCurRecentFilesNumber() const noexcept { return curRecentFilesNumber_; }

    [[nodiscard]] bool getTabWrapAround() const noexcept { return tabWrapAround_; }
    void setTabWrapAround(bool wrap) noexcept { tabWrapAround_ = wrap; }

    [[nodiscard]] bool getHideSingleTab() const noexcept { return hideSingleTab_; }
    void setHideSingleTab(bool hide) noexcept { hideSingleTab_ = hide; }

    [[nodiscard]] QSize getWinSize() const noexcept { return winSize_; }
    void setWinSize(const QSize& s) noexcept { winSize_ = s; }

    [[nodiscard]] QSize getPrefSize() const noexcept { return prefSize_; }
    void setPrefSize(const QSize& s) noexcept { prefSize_ = s; }

    [[nodiscard]] QSize getDefaultStartSize() const noexcept { return QSize(700, 500); }
    [[nodiscard]] QSize getStartSize() const noexcept { return startSize_; }
    void setStartSize(const QSize& s) noexcept { startSize_ = s; }

    [[nodiscard]] QPoint getWinPos() const noexcept { return winPos_; }
    void setWinPos(const QPoint& p) noexcept { winPos_ = p; }

    [[nodiscard]] int getSplitterPos() const noexcept { return splitterPos_; }
    void setSplitterPos(int pos) noexcept { splitterPos_ = std::max(0, pos); }

    [[nodiscard]] bool getNoToolbar() const noexcept { return noToolbar_; }
    void setNoToolbar(bool noTB) noexcept { noToolbar_ = noTB; }

    [[nodiscard]] bool getNoMenubar() const noexcept { return noMenubar_; }
    void setNoMenubar(bool noMB) noexcept { noMenubar_ = noMB; }

    [[nodiscard]] bool getMenubarTitle() const noexcept { return menubarTitle_; }
    void setMenubarTitle(bool mt) noexcept { menubarTitle_ = mt; }

    [[nodiscard]] bool getHideSearchbar() const noexcept { return hideSearchbar_; }
    void setHideSearchbar(bool hide) noexcept { hideSearchbar_ = hide; }

    [[nodiscard]] bool getShowStatusbar() const noexcept { return showStatusbar_; }
    void setShowStatusbar(bool show) noexcept { showStatusbar_ = show; }

    [[nodiscard]] bool getShowCursorPos() const noexcept { return showCursorPos_; }
    void setShowCursorPos(bool show) noexcept { showCursorPos_ = show; }

    [[nodiscard]] bool getShowLangSelector() const noexcept { return showLangSelector_; }
    void setShowLangSelector(bool show) noexcept { showLangSelector_ = show; }

    [[nodiscard]] bool getSidePaneMode() const noexcept { return sidePaneMode_; }
    void setSidePaneMode(bool sidePane) noexcept { sidePaneMode_ = sidePane; }

    [[nodiscard]] int getTabPosition() const noexcept { return tabPosition_; }
    void setTabPosition(int pos) noexcept { tabPosition_ = std::clamp(pos, kMinTabPos, kMaxTabPos); }

    [[nodiscard]] QFont getFont() const { return font_; }
    void setFont(const QFont& font) { font_ = font; }
    void resetFont();

    [[nodiscard]] bool getRemFont() const noexcept { return remFont_; }
    void setRemFont(bool rem) noexcept { remFont_ = rem; }

    [[nodiscard]] bool getWrapByDefault() const noexcept { return wrapByDefault_; }
    void setWrapByDefault(bool wrap) noexcept { wrapByDefault_ = wrap; }

    [[nodiscard]] bool getIndentByDefault() const noexcept { return indentByDefault_; }
    void setIndentByDefault(bool indent) noexcept { indentByDefault_ = indent; }

    [[nodiscard]] bool getAutoReplace() const noexcept { return autoReplace_; }
    void setAutoReplace(bool autoR) noexcept { autoReplace_ = autoR; }

    [[nodiscard]] bool getAutoBracket() const noexcept { return autoBracket_; }
    void setAutoBracket(bool autoB) noexcept { autoBracket_ = autoB; }

    [[nodiscard]] bool getLineByDefault() const noexcept { return lineByDefault_; }
    void setLineByDefault(bool line) noexcept { lineByDefault_ = line; }

    [[nodiscard]] bool getSyntaxByDefault() const noexcept { return syntaxByDefault_; }
    void setSyntaxByDefault(bool syntax) noexcept { syntaxByDefault_ = syntax; }

    [[nodiscard]] bool getShowWhiteSpace() const noexcept { return showWhiteSpace_; }
    void setShowWhiteSpace(bool show) noexcept { showWhiteSpace_ = show; }

    [[nodiscard]] bool getShowEndings() const noexcept { return showEndings_; }
    void setShowEndings(bool show) noexcept { showEndings_ = show; }

    [[nodiscard]] bool getTextMargin() const noexcept { return textMargin_; }
    void setTextMargin(bool margin) noexcept { textMargin_ = margin; }

    [[nodiscard]] int getDefaultVLineDistance() const noexcept { return 80; }
    [[nodiscard]] int getVLineDistance() const noexcept { return vLineDistance_; }
    void setVLineDistance(int distance) noexcept { vLineDistance_ = distance; }

    [[nodiscard]] int getDefaultMaxSHSize() const noexcept { return 2; }
    [[nodiscard]] int getMaxSHSize() const noexcept { return maxSHSize_; }
    void setMaxSHSize(int max) noexcept { maxSHSize_ = max; }

    [[nodiscard]] bool getSkipNonText() const noexcept { return skipNonText_; }
    void setSkipNonText(bool skip) noexcept { skipNonText_ = skip; }

    [[nodiscard]] bool getExecuteScripts() const noexcept { return executeScripts_; }
    void setExecuteScripts(bool execute) noexcept { executeScripts_ = execute; }
    [[nodiscard]] QString getExecuteCommand() const { return executeCommand_; }
    void setExecuteCommand(const QString& command) { executeCommand_ = command; }

    [[nodiscard]] bool getAppendEmptyLine() const noexcept { return appendEmptyLine_; }
    void setAppendEmptyLine(bool append) noexcept { appendEmptyLine_ = append; }

    [[nodiscard]] bool getRemoveTrailingSpaces() const noexcept { return removeTrailingSpaces_; }
    void setRemoveTrailingSpaces(bool remove) noexcept { removeTrailingSpaces_ = remove; }

    [[nodiscard]] bool getOpenInWindows() const noexcept { return openInWindows_; }
    void setOpenInWindows(bool windows) noexcept { openInWindows_ = windows; }

    [[nodiscard]] bool getNativeDialog() const noexcept { return nativeDialog_; }
    void setNativeDialog(bool native) noexcept { nativeDialog_ = native; }

    [[nodiscard]] bool getRecentOpened() const noexcept { return recentOpened_; }
    void setRecentOpened(bool opened) noexcept { recentOpened_ = opened; }

    [[nodiscard]] QStringList getRecentFiles() const { return recentFiles_; }
    void clearRecentFiles() { recentFiles_.clear(); }
    void addRecentFile(const QString& file);

    [[nodiscard]] QHash<QString, QString> customShortcutActions() const { return actions_; }
    void setActionShortcut(const QString& action, const QString& shortcut) { actions_.insert(action, shortcut); }
    void removeShortcut(const QString& action) {
        actions_.remove(action);
        removedActions_ << action;
    }

    [[nodiscard]] bool hasReservedShortcuts() const noexcept { return (!reservedShortcuts_.isEmpty()); }
    [[nodiscard]] QStringList reservedShortcuts() const { return reservedShortcuts_; }
    void setReservedShortcuts(const QStringList& s) { reservedShortcuts_ = s; }

    [[nodiscard]] bool getInertialScrolling() const noexcept { return inertialScrolling_; }
    void setInertialScrolling(bool inertial) noexcept { inertialScrolling_ = inertial; }

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

    [[nodiscard]] bool getSaveLastFilesList() const noexcept { return saveLastFilesList_; }
    void setSaveLastFilesList(bool saveList) noexcept { saveLastFilesList_ = saveList; }

    // may be called only at session start and sets lasFilesCursorPos_
    [[nodiscard]] QStringList getLastFiles();
    // is called only after getLastFiles()
    [[nodiscard]] QHash<QString, QVariant> getLastFilesCursorPos() const { return lasFilesCursorPos_; }
    void setLastFileCursorPos(const QHash<QString, QVariant>& curPos) { lasFilesCursorPos_ = curPos; }

    [[nodiscard]] bool getAutoSave() const noexcept { return autoSave_; }
    void setAutoSave(bool as) noexcept { autoSave_ = as; }
    [[nodiscard]] int getAutoSaveInterval() const noexcept { return autoSaveInterval_; }
    void setAutoSaveInterval(int i) noexcept { autoSaveInterval_ = i; }

    [[nodiscard]] bool getSaveUnmodified() const noexcept { return saveUnmodified_; }
    void setSaveUnmodified(bool save) noexcept { saveUnmodified_ = save; }

    [[nodiscard]] bool getSelectionHighlighting() const noexcept { return selectionHighlighting_; }
    void setSelectionHighlighting(bool enable) noexcept { selectionHighlighting_ = enable; }

    [[nodiscard]] bool getPastePaths() const noexcept { return pastePaths_; }
    void setPastePaths(bool pastPaths) noexcept { pastePaths_ = pastPaths; }

    [[nodiscard]] bool getCloseWithLastTab() const noexcept { return closeWithLastTab_; }
    void setCloseWithLastTab(bool close) noexcept { closeWithLastTab_ = close; }

    [[nodiscard]] bool getSharedSearchHistory() const noexcept { return sharedSearchHistory_; }
    void setSharedSearchHistory(bool share) noexcept { sharedSearchHistory_ = share; }

    [[nodiscard]] bool getDisableMenubarAccel() const noexcept { return disableMenubarAccel_; }
    void setDisableMenubarAccel(bool disable) noexcept { disableMenubarAccel_ = disable; }

    [[nodiscard]] bool getSysIcons() const noexcept { return sysIcons_; }
    void setSysIcons(bool sysIcons) noexcept { sysIcons_ = sysIcons; }

    [[nodiscard]] QHash<QString, QColor> lightSyntaxColors() const { return defaultLightSyntaxColors_; }
    [[nodiscard]] QHash<QString, QColor> darkSyntaxColors() const { return defaultDarkSyntaxColors_; }

    [[nodiscard]] QHash<QString, QColor> customSyntaxColors() const { return customSyntaxColors_; }
    void setCustomSyntaxColors(const QHash<QString, QColor>& colors) { customSyntaxColors_ = colors; }

    [[nodiscard]] int getDefaultWhiteSpaceValue() const noexcept { return darkColScheme_ ? 95 : 180; }
    [[nodiscard]] int getMinWhiteSpaceValue() const noexcept { return darkColScheme_ ? 50 : 130; }
    [[nodiscard]] int getMaxWhiteSpaceValue() const noexcept { return darkColScheme_ ? 140 : 230; }
    [[nodiscard]] int getWhiteSpaceValue() const noexcept { return whiteSpaceValue_; }
    void setWhiteSpaceValue(int value);

    [[nodiscard]] int getCurLineHighlight() const noexcept { return curLineHighlight_; }
    [[nodiscard]] int getMinCurLineHighlight() const noexcept { return darkColScheme_ ? 0 : 210; }
    [[nodiscard]] int getMaxCurLineHighlight() const noexcept { return darkColScheme_ ? 70 : 255; }
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
