/*
 * texxy/config.cpp
 */

#include "config.h"
#include <QKeySequence>
#include <cmath>

namespace FeatherPad {

// small helpers to keep QSettings lookups tight and explicit
static inline bool readBool(Settings& s, const char* key, bool def) {
    const QVariant v = s.value(key)
    return v.isValid() ? v.toBool() : def
}

static inline int readClampedInt(Settings& s, const char* key, int def, int lo, int hi) {
    const int v = s.value(key, def).toInt()
    return std::clamp(v, lo, hi)
}

static inline QSize readSizeOr(Settings& s, const char* key, const QSize& def) {
    const QSize z = s.value(key, def).toSize()
    return (!z.isValid() || z.isNull()) ? def : z
}

static inline void pruneToLimit(QStringList& xs, int limit) {
    while (xs.count() > limit)
        xs.removeLast()
}

Config::Config()
    : remSize_(true),
      remPos_(false),
      remSplitterPos_(true),
      noToolbar_(false),
      noMenubar_(false),
      menubarTitle_(false),
      hideSearchbar_(false),
      showStatusbar_(true),
      showCursorPos_(false),
      showLangSelector_(false),
      sidePaneMode_(false),
      remFont_(true),
      wrapByDefault_(true),
      indentByDefault_(true),
      autoReplace_(false),
      autoBracket_(false),
      lineByDefault_(false),
      syntaxByDefault_(true),
      showWhiteSpace_(false),
      showEndings_(false),
      textMargin_(false),
      isMaxed_(false),
      isFull_(false),
      darkColScheme_(false),
      thickCursor_(false),
      tabWrapAround_(false),
      hideSingleTab_(false),
      executeScripts_(false),
      appendEmptyLine_(true),
      removeTrailingSpaces_(false),
      openInWindows_(false),
      nativeDialog_(true),
      inertialScrolling_(false),
      autoSave_(false),
      skipNonText_(true),
      saveUnmodified_(false),
      selectionHighlighting_(false),
      pastePaths_(false),
      closeWithLastTab_(false),
      sharedSearchHistory_(false),
      disableMenubarAccel_(false),
      sysIcons_(false),
      vLineDistance_(-80),
      tabPosition_(0),
      maxSHSize_(2),
      lightBgColorValue_(255),
      darkBgColorValue_(15),
      recentFilesNumber_(10),
      curRecentFilesNumber_(10),
      autoSaveInterval_(1),
      textTabSize_(4),
      winSize_(QSize(700, 500)),
      startSize_(QSize(700, 500)),
      winPos_(QPoint(0, 0)),
      splitterPos_(150),
      font_(QFont("Monospace")),
      recentOpened_(false),
      saveLastFilesList_(false),
      cursorPosRetrieved_(false),
      spellCheckFromStart_(false),
      whiteSpaceValue_(180),
      curLineHighlight_(-1) {}

Config::~Config() {}

void Config::readConfig() {
    Settings settings("featherpad", "fp")

    // window
    settings.beginGroup("window")

    if (settings.value("size") == "none") {
        remSize_ = false
    } else {
        winSize_ = readSizeOr(settings, "size", QSize(700, 500))
        isMaxed_ = settings.value("max", false).toBool()
        isFull_  = settings.value("fullscreen", false).toBool()
    }
    startSize_ = readSizeOr(settings, "startSize", QSize(700, 500))

    const QVariant posv = settings.value("position")
    if (posv.isValid() && posv != "none") {
        remPos_ = true
        winPos_ = settings.value("position", QPoint(0, 0)).toPoint()
    }

    if (settings.value("splitterPos") == "none") {
        remSplitterPos_ = false
    } else {
        splitterPos_ = std::max(settings.value("splitterPos", 150).toInt(), 0)
    }

    prefSize_ = settings.value("prefSize").toSize()

    if (settings.value("noToolbar").toBool())
        noToolbar_ = true
    if (settings.value("noMenubar").toBool())
        noMenubar_ = true

    // never hide both toolbar and menubar
    if (noToolbar_ && noMenubar_) {
        noToolbar_ = false
        noMenubar_ = true
    }

    if (settings.value("menubarTitle").toBool())
        menubarTitle_ = true
    if (settings.value("hideSearchbar").toBool())
        hideSearchbar_ = true

    {
        const QVariant v = settings.value("showStatusbar")
        if (v.isValid())
            showStatusbar_ = v.toBool()
    }

    if (settings.value("showCursorPos").toBool())
        showCursorPos_ = true
    if (settings.value("showLangSelector").toBool())
        showLangSelector_ = true
    if (settings.value("sidePaneMode").toBool())
        sidePaneMode_ = true

    {
        const int pos = settings.value("tabPosition").toInt()
        if (pos > 0 && pos <= 3)
            tabPosition_ = pos
    }

    if (settings.value("tabWrapAround").toBool())
        tabWrapAround_ = true
    if (settings.value("hideSingleTab").toBool())
        hideSingleTab_ = true
    if (settings.value("openInWindows").toBool())
        openInWindows_ = true

    {
        const QVariant v = settings.value("nativeDialog")
        if (v.isValid())
            nativeDialog_ = v.toBool()
    }

    if (settings.value("closeWithLastTab").toBool())
        closeWithLastTab_ = true
    if (settings.value("sharedSearchHistory").toBool())
        sharedSearchHistory_ = true
    if (settings.value("disableMenubarAccel").toBool())
        disableMenubarAccel_ = true
    if (settings.value("sysIcons").toBool())
        sysIcons_ = true

    settings.endGroup()

    // text
    settings.beginGroup("text")

    if (settings.value("font") == "none") {
        remFont_ = false
        font_.setPointSize(std::max(QFont().pointSize(), 9))
    } else {
        const QString fontStr = settings.value("font").toString()
        if (!fontStr.isEmpty())
            font_.fromString(fontStr)
        else
            font_.setPointSize(std::max(QFont().pointSize(), 9))
    }

    if (settings.value("noWrap").toBool())
        wrapByDefault_ = false
    if (settings.value("noIndent").toBool())
        indentByDefault_ = false
    if (settings.value("autoReplace").toBool())
        autoReplace_ = true
    if (settings.value("autoBracket").toBool())
        autoBracket_ = true
    if (settings.value("lineNumbers").toBool())
        lineByDefault_ = true
    if (settings.value("noSyntaxHighlighting").toBool())
        syntaxByDefault_ = false
    if (settings.value("showWhiteSpace").toBool())
        showWhiteSpace_ = true
    if (settings.value("showEndings").toBool())
        showEndings_ = true
    if (settings.value("textMargin").toBool())
        textMargin_ = true
    if (settings.value("darkColorScheme").toBool())
        darkColScheme_ = true
    if (settings.value("thickCursor").toBool())
        thickCursor_ = true
    if (settings.value("inertialScrolling").toBool())
        inertialScrolling_ = true
    if (settings.value("autoSave").toBool())
        autoSave_ = true

    {
        const int distance = settings.value("vLineDistance").toInt()
        if (std::abs(distance) >= 10 && std::abs(distance) < 1000)
            vLineDistance_ = distance
    }

    {
        const QVariant v = settings.value("skipNonText")
        if (v.isValid())
            skipNonText_ = v.toBool()
    }

    if (settings.value("saveUnmodified").toBool())
        saveUnmodified_ = true
    if (settings.value("selectionHighlighting").toBool())
        selectionHighlighting_ = true
    if (settings.value("pastePaths").toBool())
        pastePaths_ = true

    maxSHSize_ = readClampedInt(settings, "maxSHSize", 2, 1, 10)

    // do not let the light bg be darker than #e6e6e6
    lightBgColorValue_ = readClampedInt(settings, "lightBgColorValue", 255, 230, 255)
    // do not let the dark bg be lighter than #323232
    darkBgColorValue_  = readClampedInt(settings, "darkBgColorValue", 15, 0, 50)

    dateFormat_ = settings.value("dateFormat").toString()

    if (settings.value("executeScripts").toBool())
        executeScripts_ = true
    executeCommand_ = settings.value("executeCommand").toString()

    {
        const QVariant v = settings.value("appendEmptyLine")
        if (v.isValid())
            appendEmptyLine_ = v.toBool()
    }

    if (settings.value("removeTrailingSpaces").toBool())
        removeTrailingSpaces_ = true

    recentFilesNumber_   = readClampedInt(settings, "recentFilesNumber", 10, 0, 50)
    curRecentFilesNumber_ = recentFilesNumber_

    recentFiles_ = settings.value("recentFiles").toStringList()
    recentFiles_.removeAll("")
    recentFiles_.removeDuplicates()
    pruneToLimit(recentFiles_, recentFilesNumber_)

    if (settings.value("recentOpened").toBool())
        recentOpened_ = true

    if (settings.value("saveLastFilesList").toBool())
        saveLastFilesList_ = true

    autoSaveInterval_ = readClampedInt(settings, "autoSaveInterval", 1, 1, 60)
    textTabSize_      = readClampedInt(settings, "textTabSize", 4, 2, 10)

    dictPath_ = settings.value("dictionaryPath").toString()
    spellCheckFromStart_ = settings.value("spellCheckFromStart").toBool()

    settings.endGroup()

    readSyntaxColors()
}

void Config::resetFont() {
    font_ = QFont("Monospace")
    font_.setPointSize(std::max(QFont().pointSize(), 9))
}

void Config::readShortcuts() {
    // do not read global custom shortcuts so user can restore defaults
    Settings tmp("featherpad", "fp")
    Settings settings(tmp.fileName(), QSettings::NativeFormat)

    settings.beginGroup("shortcuts")
    const QStringList actions = settings.childKeys()
    for (int i = 0; i < actions.size(); ++i) {
        const QVariant v = settings.value(actions.at(i))
        bool isValid
        const QString vs = validatedShortcut(v, &isValid)
        if (isValid)
            setActionShortcut(actions.at(i), vs)
        else
            removedActions_ << actions.at(i)
    }
    settings.endGroup()
}

QStringList Config::getLastFiles() {
    if (!saveLastFilesList_)
        return QStringList()

    Settings settingsLastCur("featherpad", "fp_last_cursor_pos")
    lasFilesCursorPos_ = settingsLastCur.value("cursorPositions").toHash()

    QStringList lastFiles = lasFilesCursorPos_.keys()
    lastFiles.removeAll("")
    lastFiles.removeDuplicates()
    pruneToLimit(lastFiles, 50) // never more than 50 files
    return lastFiles
}

void Config::readSyntaxColors() {
    setDefaultSyntaxColors()
    customSyntaxColors_.clear()

    // do not read global custom colors so user can restore defaults
    Settings tmp("featherpad", darkColScheme_ ? "fp_dark_syntax_colors" : "fp_light_syntax_colors")
    Settings settingsColors(tmp.fileName(), QSettings::NativeFormat)

    settingsColors.beginGroup("curLineHighlight")
    curLineHighlight_ = std::clamp(settingsColors.value("value", -1).toInt(), -1, 255)
    settingsColors.endGroup()
    if (curLineHighlight_ >= 0 && (darkColScheme_ ? curLineHighlight_ > 70 : curLineHighlight_ < 210))
        curLineHighlight_ = -1

    settingsColors.beginGroup("whiteSpace")
    const int ws = settingsColors.value("value").toInt()
    settingsColors.endGroup()
    if (ws < getMinWhiteSpaceValue() || ws > getMaxWhiteSpaceValue())
        whiteSpaceValue_ = getDefaultWhiteSpaceValue()
    else
        whiteSpaceValue_ = ws

    const auto syntaxes = defaultLightSyntaxColors_.keys()
    QList<QColor> used
    used << (darkColScheme_ ? QColor(Qt::white) : QColor(Qt::black))
    used << QColor(whiteSpaceValue_, whiteSpaceValue_, whiteSpaceValue_)

    for (const auto& syntax : syntaxes) {
        QColor col
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
        col = QColor::fromString(settingsColors.value(syntax).toString())
#else
        col.setNamedColor(settingsColors.value(syntax).toString())
#endif
        if (col.isValid())
            col.setAlpha(255) // only opaque custom colors
        if (!col.isValid() || used.contains(col)) {
            customSyntaxColors_.clear() // invalid or repeated color means corrupted configuration
            break
        }
        used << col
        customSyntaxColors_.insert(syntax, col)
    }
}

void Config::writeConfig() {
    Settings settings("featherpad", "fp")
    if (!settings.isWritable())
        return

    // window
    settings.beginGroup("window")

    if (remSize_) {
        settings.setValue("size", winSize_)
        settings.setValue("max", isMaxed_)
        settings.setValue("fullscreen", isFull_)
    } else {
        settings.setValue("size", "none")
        settings.remove("max")
        settings.remove("fullscreen")
    }

    if (remPos_)
        settings.setValue("position", winPos_)
    else
        settings.setValue("position", "none")

    if (remSplitterPos_)
        settings.setValue("splitterPos", splitterPos_)
    else
        settings.setValue("splitterPos", "none")

    settings.setValue("prefSize",  prefSize_)
    settings.setValue("startSize", startSize_)
    settings.setValue("noToolbar", noToolbar_)
    settings.setValue("noMenubar", noMenubar_)
    settings.setValue("menubarTitle", menubarTitle_)
    settings.setValue("hideSearchbar", hideSearchbar_)
    settings.setValue("showStatusbar", showStatusbar_)
    settings.setValue("showCursorPos", showCursorPos_)
    settings.setValue("showLangSelector", showLangSelector_)
    settings.setValue("sidePaneMode", sidePaneMode_)
    settings.setValue("tabPosition", tabPosition_)
    settings.setValue("tabWrapAround", tabWrapAround_)
    settings.setValue("hideSingleTab", hideSingleTab_)
    settings.setValue("openInWindows", openInWindows_)
    settings.setValue("nativeDialog", nativeDialog_)
    settings.setValue("closeWithLastTab", closeWithLastTab_)
    settings.setValue("sharedSearchHistory", sharedSearchHistory_)
    settings.setValue("disableMenubarAccel", disableMenubarAccel_)
    settings.setValue("sysIcons", sysIcons_)

    settings.endGroup()

    // text
    settings.beginGroup("text")

    settings.setValue("font", remFont_ ? font_.toString() : "none")
    settings.setValue("noWrap", !wrapByDefault_)
    settings.setValue("noIndent", !indentByDefault_)
    settings.setValue("autoReplace", autoReplace_)
    settings.setValue("autoBracket", autoBracket_)
    settings.setValue("lineNumbers", lineByDefault_)
    settings.setValue("noSyntaxHighlighting", !syntaxByDefault_)
    settings.setValue("showWhiteSpace", showWhiteSpace_)
    settings.setValue("showEndings", showEndings_)
    settings.setValue("textMargin", textMargin_)
    settings.setValue("darkColorScheme", darkColScheme_)
    settings.setValue("thickCursor", thickCursor_)
    settings.setValue("inertialScrolling", inertialScrolling_)
    settings.setValue("autoSave", autoSave_)
    settings.setValue("skipNonText", skipNonText_)
    settings.setValue("saveUnmodified", saveUnmodified_)
    settings.setValue("selectionHighlighting", selectionHighlighting_)
    settings.setValue("pastePaths", pastePaths_)
    settings.setValue("maxSHSize", maxSHSize_)
    settings.setValue("lightBgColorValue", lightBgColorValue_)
    settings.setValue("dateFormat", dateFormat_)
    settings.setValue("darkBgColorValue", darkBgColorValue_)
    settings.setValue("executeScripts", executeScripts_)
    settings.setValue("appendEmptyLine", appendEmptyLine_)
    settings.setValue("removeTrailingSpaces", removeTrailingSpaces_)
    settings.setValue("vLineDistance", vLineDistance_)
    settings.setValue("recentFilesNumber", recentFilesNumber_)
    settings.setValue("executeCommand", executeCommand_)

    pruneToLimit(recentFiles_, recentFilesNumber_)
    settings.setValue("recentFiles", recentFiles_.isEmpty() ? QString("") : recentFiles_)
    settings.setValue("recentOpened", recentOpened_)
    settings.setValue("saveLastFilesList", saveLastFilesList_)
    settings.setValue("autoSaveInterval", autoSaveInterval_)
    settings.setValue("textTabSize", textTabSize_)
    settings.setValue("dictionaryPath", dictPath_)
    settings.setValue("spellCheckFromStart", spellCheckFromStart_)

    settings.endGroup()

    // shortcuts
    settings.beginGroup("shortcuts")
    for (int i = 0; i < removedActions_.size(); ++i)
        settings.remove(removedActions_.at(i))

    for (auto it = actions_.cbegin(); it != actions_.cend(); ++it)
        settings.setValue(it.key(), it.value())
    settings.endGroup()

    writeCursorPos()
    writeSyntaxColors()
}

void Config::readCursorPos() {
    if (!cursorPosRetrieved_) {
        Settings settings("featherpad", "fp_cursor_pos")
        cursorPos_ = settings.value("cursorPositions").toHash()
        cursorPosRetrieved_ = true
    }
}

void Config::writeCursorPos() {
    Settings settings("featherpad", "fp_cursor_pos")
    if (settings.isWritable()) {
        if (!cursorPos_.isEmpty())
            settings.setValue("cursorPositions", cursorPos_)
    }

    Settings settingsLastCur("featherpad", "fp_last_cursor_pos")
    if (settingsLastCur.isWritable()) {
        if (saveLastFilesList_ && !lasFilesCursorPos_.isEmpty())
            settingsLastCur.setValue("cursorPositions", lasFilesCursorPos_)
        else
            settingsLastCur.remove("cursorPositions")
    }
}

void Config::writeSyntaxColors() {
    Settings settingsColors("featherpad", darkColScheme_ ? "fp_dark_syntax_colors" : "fp_light_syntax_colors")

    if (customSyntaxColors_.isEmpty()) {
        if (whiteSpaceValue_ != getDefaultWhiteSpaceValue() || curLineHighlight_ != -1) {
            if (settingsColors.allKeys().size() > 2)
                settingsColors.clear()
        } else {
            settingsColors.clear()
            return
        }
    } else {
        for (auto it = customSyntaxColors_.cbegin(); it != customSyntaxColors_.cend(); ++it)
            settingsColors.setValue(it.key(), it.value().name())
    }

    // QSettings can drop files without subkeys in some cases, so always add a small subkey
    settingsColors.beginGroup("whiteSpace")
    settingsColors.setValue("value", whiteSpaceValue_)
    settingsColors.endGroup()

    settingsColors.beginGroup("curLineHighlight")
    settingsColors.setValue("value", curLineHighlight_)
    settingsColors.endGroup()
}

void Config::setWhiteSpaceValue(int value) {
    value = std::clamp(value, getMinWhiteSpaceValue(), getMaxWhiteSpaceValue())
    QList<QColor> colors
    colors << (darkColScheme_ ? QColor(Qt::white) : QColor(Qt::black))

    if (!customSyntaxColors_.isEmpty())
        colors = customSyntaxColors_.values()
    else if (darkColScheme_)
        colors = defaultDarkSyntaxColors_.values()
    else
        colors = defaultLightSyntaxColors_.values()

    const int average = (getMinWhiteSpaceValue() + getMaxWhiteSpaceValue()) / 2
    QColor ws(value, value, value)
    while (colors.contains(ws)) {
        int r = ws.red()
        if (value >= average)
            --r
        else
            ++r
        ws = QColor(r, r, r)
    }
    whiteSpaceValue_ = ws.red()
}

void Config::setCurLineHighlight(int value) {
    if (value < getMinCurLineHighlight() || value > getMaxCurLineHighlight())
        curLineHighlight_ = -1
    else
        curLineHighlight_ = value
}

void Config::addRecentFile(const QString& file) {
    if (curRecentFilesNumber_ > 0) {
        recentFiles_.removeAll(file)
        recentFiles_.prepend(file)
        pruneToLimit(recentFiles_, curRecentFilesNumber_)
    }
}

QString Config::validatedShortcut(const QVariant v, bool* isValid) {
    static QStringList added
    if (v.isValid()) {
        QString str = v.toString()
        if (str.isEmpty()) {
            *isValid = true
            return QString()
        }

        // convert older NativeText shortcuts to PortableText for backward compatibility
        QKeySequence keySeq(str)
        if (str == keySeq.toString(QKeySequence::NativeText))
            str = keySeq.toString()

        if (!QKeySequence(str, QKeySequence::PortableText).toString().isEmpty()) {
            if (!reservedShortcuts_.contains(str) && !added.contains(str)) {
                *isValid = true
                added << str
                return str
            }
        }
    }

    *isValid = false
    return QString()
}

void Config::setDefaultSyntaxColors() {
    if (defaultLightSyntaxColors_.isEmpty()) {
        defaultLightSyntaxColors_.insert("function", QColor(Qt::blue))
        defaultLightSyntaxColors_.insert("BuiltinFunction", QColor(Qt::magenta))
        defaultLightSyntaxColors_.insert("comment", QColor(Qt::red))
        defaultLightSyntaxColors_.insert("quote", QColor(Qt::darkGreen))
        defaultLightSyntaxColors_.insert("type", QColor(Qt::darkMagenta))
        defaultLightSyntaxColors_.insert("keyWord", QColor(Qt::darkBlue))
        defaultLightSyntaxColors_.insert("number", QColor(150, 85, 0))
        defaultLightSyntaxColors_.insert("regex", QColor(150, 0, 0))
        defaultLightSyntaxColors_.insert("xmlElement", QColor(126, 0, 230))
        defaultLightSyntaxColors_.insert("cssValue", QColor(0, 110, 110))
        defaultLightSyntaxColors_.insert("other", QColor(100, 100, 0))

        defaultDarkSyntaxColors_.insert("function", QColor(85, 227, 255))
        defaultDarkSyntaxColors_.insert("BuiltinFunction", QColor(Qt::magenta))
        defaultDarkSyntaxColors_.insert("comment", QColor(255, 120, 120))
        defaultDarkSyntaxColors_.insert("quote", QColor(Qt::green))
        defaultDarkSyntaxColors_.insert("type", QColor(255, 153, 255))
        defaultDarkSyntaxColors_.insert("keyWord", QColor(65, 154, 255))
        defaultDarkSyntaxColors_.insert("number", QColor(255, 205, 0))
        defaultDarkSyntaxColors_.insert("regex", QColor(255, 160, 0))
        defaultDarkSyntaxColors_.insert("xmlElement", QColor(255, 255, 1))
        defaultDarkSyntaxColors_.insert("cssValue", QColor(150, 255, 0))
        defaultDarkSyntaxColors_.insert("other", QColor(Qt::yellow))
    }
}

} // namespace FeatherPad
