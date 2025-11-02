#include "textedit/textedit_prelude.h"

namespace Texxy {

namespace {
// set both CLIPBOARD and X11 PRIMARY when available
inline void setClipboardTextBoth(const QString& text) {
    if (text.isEmpty())
        return;
    if (QClipboard* cb = QApplication::clipboard()) {
        cb->setText(text, QClipboard::Clipboard);
        if (cb->supportsSelection())
            cb->setText(text, QClipboard::Selection);
    }
}

// cheap check for usable plain text in a mime data object
inline bool hasUsableText(const QMimeData* md) {
    return md && md->hasText() && !md->text().isEmpty();
}
}  // namespace

void TextEdit::copy() {
    const QTextCursor cur = textCursor();
    if (cur.hasSelection())
        setClipboardTextBoth(cur.selection().toPlainText());
    else
        copyColumn();
}

void TextEdit::cut() {
    QTextCursor cur = textCursor();
    if (cur.hasSelection()) {
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        setClipboardTextBoth(cur.selection().toPlainText());
        cur.removeSelectedText();
    }
    else {
        cutColumn();
    }
}

void TextEdit::deleteText() {
    if (textCursor().hasSelection()) {
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        QPlainTextEdit::insertPlainText(QString());
    }
    else {
        deleteColumn();
    }
}

void TextEdit::undo() {
    removeColumnHighlight();
    setGreenSel(QList<QTextEdit::ExtraSelection>());

    if (getSearchedText().isEmpty()) {
        QList<QTextEdit::ExtraSelection> es;
        if (!currentLine_.cursor.isNull())
            es.prepend(currentLine_);
        es.append(blueSel_);
        es.append(colSel_);
        es.append(redSel_);
        setExtraSelections(es);
    }

    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    QPlainTextEdit::undo();

    removeSelectionHighlights_ = true;
    selectionHlight();
}

void TextEdit::redo() {
    removeColumnHighlight();
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    QPlainTextEdit::redo();

    removeSelectionHighlights_ = true;
    selectionHlight();
}

void TextEdit::paste() {
    keepTxtCurHPos_ = false;
    if (!colSel_.isEmpty())
        pasteOnColumn();
    else
        QPlainTextEdit::paste();
}

void TextEdit::selectAll() {
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    removeColumnHighlight();
    QPlainTextEdit::selectAll();
}

void TextEdit::insertPlainText(const QString& text) {
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    QPlainTextEdit::insertPlainText(text);
}

QMimeData* TextEdit::createMimeDataFromSelection() const {
    const QTextCursor cur = textCursor();
    if (!cur.hasSelection())
        return nullptr;

    auto* md = new QMimeData;
    md->setText(cur.selection().toPlainText());
    return md;
}

bool TextEdit::pastingIsPossible() const {
    if (!(textInteractionFlags() & Qt::TextEditable))
        return false;

    const QClipboard* cb = QGuiApplication::clipboard();
    if (!cb)
        return false;

    const QMimeData* md = cb->mimeData();
    return md && (md->hasUrls() || hasUsableText(md));
}

bool TextEdit::canInsertFromMimeData(const QMimeData* source) const {
    if (!source)
        return false;
    if (source->hasUrls())
        return true;
    return hasUsableText(source);
}

void TextEdit::insertFromMimeData(const QMimeData* source) {
    if (!source)
        return;

    if (source->hasUrls()) {
        const QList<QUrl> urlList = source->urls();
        const bool multiple = urlList.count() > 1;

        if (!pastePaths_ || multiple) {
            QTextCursor cur = textCursor();
            cur.beginEditBlock();
            for (const QUrl& url : urlList) {
                if (!url.isValid())
                    continue;
                const QString s = url.isLocalFile() ? url.toLocalFile() : url.toString(QUrl::EncodeSpaces);
                cur.insertText(s);
                if (multiple)
                    cur.insertText("\n");
            }
            cur.endEditBlock();
            ensureCursorVisible();
        }
        else {
            txtCurHPos_ = -1;
            for (const QUrl& url : urlList) {
                QString file;
                const QString scheme = url.scheme();
                if (scheme == QLatin1String("admin"))
                    file = url.adjusted(QUrl::NormalizePathSegments).path();
                else if (scheme == QLatin1String("file") || scheme.isEmpty())
                    file = url.adjusted(QUrl::NormalizePathSegments).toLocalFile();
                else
                    continue;
                emit filePasted(file, 0, 0, multiple);
            }
        }
        return;
    }

    if (hasUsableText(source))
        QPlainTextEdit::insertFromMimeData(source);
}

void TextEdit::copyColumn() {
    if (colSel_.isEmpty())
        return;

    QStringList lines;
    lines.reserve(colSel_.size());
    for (const auto& extra : std::as_const(colSel_))
        lines.append(extra.cursor.selection().toPlainText());

    const QString joined = lines.join('\n');
    if (!joined.isEmpty())
        setClipboardTextBoth(joined);
}

void TextEdit::cutColumn() {
    if (colSel_.isEmpty())
        return;

    if (colSel_.count() > 1000) {
        emit hugeColumn();
        return;
    }

    QStringList lines;
    lines.reserve(colSel_.size());

    QTextCursor cur = textCursor();
    cur.beginEditBlock();
    for (const auto& extra : std::as_const(colSel_)) {
        lines.append(extra.cursor.selectedText());
        if (!extra.cursor.hasSelection())
            continue;
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        QTextCursor c = extra.cursor;
        c.removeSelectedText();
    }
    cur.endEditBlock();

    const QString joined = lines.join('\n');
    if (!joined.isEmpty())
        setClipboardTextBoth(joined);

    removeColumnHighlight();
}

void TextEdit::deleteColumn() {
    if (colSel_.isEmpty())
        return;

    if (colSel_.count() > 1000) {
        emit hugeColumn();
        return;
    }

    QTextCursor cur = textCursor();
    cur.beginEditBlock();
    for (const auto& extra : std::as_const(colSel_)) {
        if (!extra.cursor.hasSelection())
            continue;
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        QTextCursor c = extra.cursor;
        c.removeSelectedText();
    }
    cur.endEditBlock();

    removeColumnHighlight();
}

void TextEdit::pasteOnColumn() {
    if (colSel_.isEmpty())
        return;

    if (colSel_.count() > 1000) {
        emit hugeColumn();
        return;
    }

    const QClipboard* cb = QApplication::clipboard();
    const QString raw = cb ? cb->text(QClipboard::Clipboard) : QString();
    if (raw.isEmpty())
        return;

    const bool savedPressed = mousePressed_;
    mousePressed_ = true;

    const QStringList parts = raw.split('\n');  // preserve empties to keep row alignment
    QTextCursor cur = textCursor();
    cur.beginEditBlock();

    int i = 0;
    for (const auto& extra : std::as_const(colSel_)) {
        if (i >= parts.size())
            break;
        QTextCursor c = extra.cursor;
        c.insertText(parts.at(i));
        ++i;
    }

    cur.endEditBlock();
    mousePressed_ = savedPressed;

    if (i > 0)
        keepTxtCurHPos_ = false;

    for (const auto& extra : std::as_const(colSel_)) {
        if (extra.cursor.hasSelection()) {
            if (selectionTimerId_) {
                killTimer(selectionTimerId_);
                selectionTimerId_ = 0;
            }
            selectionTimerId_ = startTimer(kUpdateIntervalMs);
            return;
        }
    }

    removeColumnHighlight();
}

}  // namespace Texxy
