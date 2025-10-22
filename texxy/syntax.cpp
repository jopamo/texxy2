/*
 texxy/syntax.cpp
*/

#include "singleton.h"
#include "ui_texxywindow.h"

#include <QMimeDatabase>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>
#include <QHash>
#include <QStringView>

namespace Texxy {

/*
 helper: one shared QMimeDatabase
*/
static QMimeType getMimeType(const QFileInfo& fInfo) {
    static QMimeDatabase mimeDatabase;
    return mimeDatabase.mimeTypeForFile(fInfo);
}

/*
 compile-time tables for filename and mime hints
 keys for filename maps are stored lowercase to allow cheap case-insensitive lookup
*/
static const QHash<QString, QString> specialFilenamesMap = {
    {"makefile", "makefile"},
    {"makefile.am", "makefile"},
    {"makelist", "makefile"},
    {"pkgbuild", "sh"},
    {"fstab", "sh"},
    {"changelog", "changelog"},
    {"gtkrc", "gtkrc"},
    {"control", "deb"},
    {"mirrorlist", "config"},
    {"themerc", "openbox"},
    {"bashrc", "sh"},
    {"bash_profile", "sh"},
    {"bash_functions", "sh"},
    {"bash_logout", "sh"},
    {"bash_aliases", "sh"},
    {"xprofile", "sh"},
    {"profile", "sh"},
    {"mkshrc", "sh"},
    {"zprofile", "sh"},
    {"zlogin", "sh"},
    {"zshrc", "sh"},
    {"zshenv", "sh"},
    {"cmakelists.txt", "cmake"},
};

/*
 mime to language map
*/
static const QHash<QString, QString> mimeLanguageMap = {
    {"text/x-c++", "cpp"},
    {"text/x-c++src", "cpp"},
    {"text/x-c++hdr", "cpp"},
    {"text/x-chdr", "cpp"},
    {"text/x-c", "c"},
    {"text/x-csrc", "c"},
    {"application/x-shellscript", "sh"},
    {"text/x-shellscript", "sh"},
    {"application/x-ruby", "ruby"},
    {"text/x-lua", "lua"},
    {"application/x-perl", "perl"},
    {"text/x-makefile", "makefile"},
    {"text/x-cmake", "cmake"},
    {"application/vnd.nokia.qt.qmakeprofile", "qmake"},
    {"text/troff", "troff"},
    {"application/x-troff-man", "troff"},
    {"text/x-tex", "LaTeX"},
    {"application/x-lyx", "LaTeX"},
    {"text/html", "html"},
    {"application/xhtml+xml", "html"},
    {"application/xml", "xml"},
    {"application/xml-dtd", "xml"},
    {"text/feathernotes-fnx", "xml"},
    {"audio/x-ms-asx", "xml"},
    {"text/x-nfo", "xml"},
    {"text/css", "css"},
    {"text/x-scss", "scss"},
    {"text/x-pascal", "pascal"},
    {"text/x-changelog", "changelog"},
    {"application/x-desktop", "desktop"},
    {"audio/x-scpls", "config"},
    {"application/vnd.kde.kcfgc", "config"},
    {"application/javascript", "javascript"},
    {"text/javascript", "javascript"},
    {"text/x-java", "java"},
    {"application/json", "json"},
    {"application/schema+json", "json"},
    {"text/x-qml", "qml"},
    {"text/x-log", "log"},
    {"application/x-php", "php"},
    {"text/x-php", "php"},
    {"application/x-theme", "theme"},
    {"text/x-diff", "diff"},
    {"text/x-patch", "diff"},
    {"text/markdown", "markdown"},
    {"audio/x-mpegurl", "m3u"},
    {"application/vnd.apple.mpegurl", "m3u"},
    {"text/x-go", "go"},
    {"text/rust", "rust"},
    {"text/x-tcl", "tcl"},
    {"text/tcl", "tcl"},
    {"application/toml", "toml"},
};

/*
 build-once extension maps for O(1) lookup by suffix
  - exactMap: case-sensitive extensions like .h .cpp
  - lowerMap: case-insensitive extensions like .htm .html .xml family
 store keys with leading dot to match endings precisely
*/
struct ExtMaps {
    QHash<QString, QString> exactMap;
    QHash<QString, QString> lowerMap;
};

static const ExtMaps& extMaps() {
    static const ExtMaps maps = [] {
        ExtMaps m;

        struct Entry {
            const char* ext;
            bool caseSensitive;
            const char* lang;
        };
        static const Entry entries[] = {
            {".cpp", true, "cpp"},
            {".cxx", true, "cpp"},
            {".h", true, "cpp"},
            {".c", true, "c"},
            {".sh", true, "sh"},
            {".ebuild", true, "sh"},
            {".eclass", true, "sh"},
            {".zsh", true, "sh"},
            {".rb", true, "ruby"},
            {".lua", true, "lua"},
            {".nelua", true, "lua"},
            {".py", true, "python"},
            {".pl", true, "perl"},
            {".pro", true, "qmake"},
            {".pri", true, "qmake"},
            {".tr", true, "troff"},
            {".t", true, "troff"},
            {".roff", true, "troff"},
            {".tex", true, "LaTeX"},
            {".ltx", true, "LaTeX"},
            {".latex", true, "LaTeX"},
            {".lyx", true, "LaTeX"},
            {".xml", false, "xml"},
            {".svg", false, "xml"},
            {".qrc", true, "xml"},
            {".rdf", true, "xml"},
            {".docbook", true, "xml"},
            {".fnx", true, "xml"},
            {".ts", true, "xml"},
            {".menu", true, "xml"},
            {".kml", false, "xml"},
            {".xspf", false, "xml"},
            {".asx", false, "xml"},
            {".nfo", true, "xml"},
            {".dae", true, "xml"},
            {".css", true, "css"},
            {".qss", true, "css"},
            {".scss", true, "scss"},
            {".p", true, "pascal"},
            {".pas", true, "pascal"},
            {".desktop", true, "desktop"},
            {".desktop.in", true, "desktop"},
            {".directory", true, "desktop"},
            {".kvconfig", true, "config"},
            {".service", true, "config"},
            {".mount", true, "config"},
            {".timer", true, "config"},
            {".pls", false, "config"},
            {".js", true, "javascript"},
            {".hx", true, "javascript"},
            {".java", true, "java"},
            {".json", true, "json"},
            {".qml", true, "qml"},
            {".log", false, "log"},
            {".php", true, "php"},
            {".diff", true, "diff"},
            {".patch", true, "diff"},
            {".srt", true, "srt"},
            {".theme", true, "theme"},
            {".fountain", true, "fountain"},
            {".yml", true, "yaml"},
            {".yaml", true, "yaml"},
            {".m3u", false, "m3u"},
            {".htm", false, "html"},
            {".html", false, "html"},
            {".markdown", true, "markdown"},
            {".md", true, "markdown"},
            {".mkd", true, "markdown"},
            {".rst", true, "reST"},
            {".dart", true, "dart"},
            {".go", true, "go"},
            {".rs", true, "rust"},
            {".tcl", true, "tcl"},
            {".tk", true, "tcl"},
            {".toml", true, "toml"},
        };

        for (const auto& e : entries) {
            const QString key = QString::fromLatin1(e.ext);
            if (e.caseSensitive)
                m.exactMap.insert(key, QString::fromLatin1(e.lang));
            else
                m.lowerMap.insert(key.toLower(), QString::fromLatin1(e.lang));
        }
        return m;
    }();
    return maps;
}

/*
 utility: case-insensitive special filename check
*/
static QString languageForSpecialFilename(const QString& baseName) {
    const QString key = baseName.toLower();
    if (const auto it = specialFilenamesMap.constFind(key); it != specialFilenamesMap.constEnd())
        return it.value();
    return {};
}

/*
 utility: O(dots) extension lookup using maps above
 generates suffix candidates starting at each dot in the basename
 tries exact match first, then case-insensitive map
*/
static QString languageForExtension(QStringView fullPath) {
    // slice to basename without allocating
    int slash = fullPath.lastIndexOf(QChar('/'));
#ifdef Q_OS_WIN
    slash = std::max(slash, fullPath.lastIndexOf(QChar('\\')));
#endif
    const QStringView base = (slash >= 0) ? fullPath.sliced(slash + 1) : fullPath;

    const auto& maps = extMaps();

    // walk all dot positions from left to right to handle multi-part like .desktop.in
    int pos = base.indexOf('.');
    while (pos >= 0) {
        const QStringView suff = base.sliced(pos);  // includes the leading dot
        // try exact case-sensitive
        if (const auto it = maps.exactMap.constFind(suff.toString()); it != maps.exactMap.constEnd())
            return it.value();
        // try case-insensitive by lowercasing the view once
        const QString lower = suff.toString().toLower();
        if (const auto it2 = maps.lowerMap.constFind(lower); it2 != maps.lowerMap.constEnd())
            return it2.value();
        pos = base.indexOf('.', pos + 1);
    }

    return {};
}

/*
 utility: check a QMimeType and its parents
*/
static QString languageForMime(const QMimeType& mimeType) {
    const QString mime = mimeType.name();
    if (const auto it = mimeLanguageMap.constFind(mime); it != mimeLanguageMap.constEnd())
        return it.value();

    for (const auto& parentMime : mimeType.parentMimeTypes()) {
        if (const auto it = mimeLanguageMap.constFind(parentMime); it != mimeLanguageMap.constEnd())
            return it.value();
    }
    return {};
}

/*
 utility: resolve symlinks when possible
*/
static QString resolvedFilePath(const QString& filename) {
    QFileInfo info(filename);
    if (!info.exists())
        return filename;
    if (info.isSymLink()) {
        const QString finalTarget = info.canonicalFilePath();
        return finalTarget.isEmpty() ? info.symLinkTarget() : finalTarget;
    }
    return filename;
}

/*
 decide the program language for a TextEdit by filename, extension, or mime
 falls back to "url"
*/
void TexxyWindow::setProgLang(TextEdit* textEdit) {
    if (!textEdit)
        return;

    QString fname = textEdit->getFileName();
    if (fname.isEmpty())
        return;

    fname = resolvedFilePath(fname);

    if (fname.endsWith(".sub", Qt::CaseInsensitive))
        return;

    const QFileInfo fi(fname);
    const QString baseName = fi.fileName();

    if (QString lang = languageForSpecialFilename(baseName); !lang.isEmpty()) {
        textEdit->setProg(lang);
        return;
    }

    if (QString lang = languageForExtension(QStringView{fname}); !lang.isEmpty()) {
        textEdit->setProg(lang);
        return;
    }

    QString lang;
    if (fi.exists()) {
        const QMimeType mimeType = getMimeType(fi);
        const QString mimeName = mimeType.name();
        if (mimeName.startsWith(QStringLiteral("text/x-python")))
            lang = QStringLiteral("python");
        else
            lang = languageForMime(mimeType);
    }
    else {
        lang = QStringLiteral("url");
    }

    if (lang.isEmpty())
        lang = QStringLiteral("url");

    textEdit->setProg(lang);
}

/*
 toggle syntax highlighting across tabs and keep UI responsive
*/
void TexxyWindow::toggleSyntaxHighlighting() {
    const int count = ui->tabWidget->count();
    if (count == 0)
        return;

    const bool enableSH = ui->actionSyntax->isChecked();
    if (enableSH)
        makeBusy();  // may take a while with huge texts

    for (int i = 0; i < count; ++i) {
        if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget(i))) {
            TextEdit* te = tabPage->textEdit();
            syntaxHighlighting(te, enableSH, te->getLang());
        }
    }

    if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        updateLangBtn(tabPage->textEdit());

    if (enableSH)
        QTimer::singleShot(0, this, &TexxyWindow::unbusy);
}

/*
 apply or remove syntax highlighting for one editor
*/
void TexxyWindow::syntaxHighlighting(TextEdit* textEdit, bool highlight, const QString& lang) {
    if (!textEdit || textEdit->isUneditable())
        return;

    if (highlight) {
        QString progLan = lang.isEmpty() ? textEdit->getProg() : lang;

        if (progLan.isEmpty() || progLan == "help")
            return;

        Config config = static_cast<TexxyApplication*>(qApp)->getConfig();
        const qint64 textSize = textEdit->getSize();
        const qint64 maxSize = config.getMaxSHSize() * 1024LL * 1024LL;
        if (textSize > maxSize) {
            QTimer::singleShot(100, textEdit, [=]() {
                if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
                    if (tabPage->textEdit() == textEdit) {
                        showWarningBar(tr(
                            "<center><b><big>The size limit for syntax highlighting is exceeded</big></b></center>"));
                    }
                }
            });
            return;
        }

        if (!qobject_cast<Highlighter*>(textEdit->getHighlighter())) {
            const QPoint topLeft(0, 0);
            QTextCursor start = textEdit->cursorForPosition(topLeft);

            const QPoint bottomRight(textEdit->width(), textEdit->height());
            QTextCursor end = textEdit->cursorForPosition(bottomRight);

            textEdit->setDrawIndetLines(config.getShowWhiteSpace());
            textEdit->setVLineDistance(config.getVLineDistance());

            auto* highlighter = new Highlighter(
                textEdit->document(), progLan, start, end, textEdit->hasDarkScheme(), config.getShowWhiteSpace(),
                config.getShowEndings(), config.getWhiteSpaceValue(),
                config.customSyntaxColors().isEmpty()
                    ? (textEdit->hasDarkScheme() ? config.darkSyntaxColors() : config.lightSyntaxColors())
                    : config.customSyntaxColors());
            textEdit->setHighlighter(highlighter);
        }

        QTimer::singleShot(0, textEdit, [this, textEdit]() {
            if (textEdit->isVisible()) {
                formatTextRect();
                matchBrackets();
            }
            connect(textEdit, &TextEdit::updateBracketMatching, this, &TexxyWindow::matchBrackets);
            connect(textEdit, &QPlainTextEdit::blockCountChanged, this, &TexxyWindow::formatOnBlockChange);
            connect(textEdit, &TextEdit::updateRect, this, &TexxyWindow::formatTextRect);
            connect(textEdit, &TextEdit::resized, this, &TexxyWindow::formatTextRect);
            connect(textEdit->document(), &QTextDocument::contentsChange, this, &TexxyWindow::formatOnTextChange);
        });
    }
    else {
        if (auto* highlighter = qobject_cast<Highlighter*>(textEdit->getHighlighter())) {
            disconnect(textEdit->document(), &QTextDocument::contentsChange, this, &TexxyWindow::formatOnTextChange);
            disconnect(textEdit, &TextEdit::resized, this, &TexxyWindow::formatTextRect);
            disconnect(textEdit, &TextEdit::updateRect, this, &TexxyWindow::formatTextRect);
            disconnect(textEdit, &QPlainTextEdit::blockCountChanged, this, &TexxyWindow::formatOnBlockChange);
            disconnect(textEdit, &TextEdit::updateBracketMatching, this, &TexxyWindow::matchBrackets);

            QList<QTextEdit::ExtraSelection> es = textEdit->extraSelections();
            int n = textEdit->getRedSel().count();
            while (n > 0 && !es.isEmpty()) {
                es.removeLast();
                --n;
            }
            textEdit->setRedSel(QList<QTextEdit::ExtraSelection>());
            textEdit->setExtraSelections(es);

            textEdit->setDrawIndetLines(false);
            textEdit->setVLineDistance(0);

            delete highlighter;
        }
    }
}

/*
 defer rectangle update slightly on text changes to keep UI smooth
*/
void TexxyWindow::formatOnTextChange(int /*position*/, int charsRemoved, int charsAdded) const {
    if (charsRemoved > 0 || charsAdded > 0)
        QTimer::singleShot(0, this, &TexxyWindow::formatTextRect);
}

/*
 block count changes require an immediate limit update
*/
void TexxyWindow::formatOnBlockChange(int /* newBlockCount */) const {
    formatTextRect();
}

/*
 limit highlighter work to the visible area and rehighlight pending blocks
*/
void TexxyWindow::formatTextRect() const {
    if (auto* tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget())) {
        TextEdit* textEdit = tabPage->textEdit();
        if (auto* highlighter = qobject_cast<Highlighter*>(textEdit->getHighlighter())) {
            const QPoint topLeft(0, 0);
            QTextCursor start = textEdit->cursorForPosition(topLeft);

            const QPoint bottomRight(textEdit->width(), textEdit->height());
            QTextCursor end = textEdit->cursorForPosition(bottomRight);

            highlighter->setLimit(start, end);

            QTextBlock block = start.block();
            while (block.isValid() && block.blockNumber() <= end.blockNumber()) {
                if (auto* data = static_cast<TextBlockData*>(block.userData())) {
                    if (!data->isHighlighted())
                        highlighter->rehighlightBlock(block);
                }
                block = block.next();
            }
        }
    }
}

}  // namespace Texxy
