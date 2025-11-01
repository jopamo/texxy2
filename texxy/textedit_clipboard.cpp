#include "textedit.h"

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QList>
#include <QStringList>
#include <QTextDocumentFragment>
#include <QUrl>

#include <utility>

namespace Texxy {

void TextEdit::copy() {
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
        QApplication::clipboard()->setText(cursor.selection().toPlainText());
    else
        copyColumn();
}

void TextEdit::cut() {
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        QApplication::clipboard()->setText(cursor.selection().toPlainText());
        cursor.removeSelectedText();
    }
    else
        cutColumn();
}

void TextEdit::deleteText() {
    if (textCursor().hasSelection()) {
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        insertPlainText(QString());
    }
    else
        deleteColumn();
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
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        QMimeData* mimeData = new QMimeData;
        mimeData->setText(cursor.selection().toPlainText());
        return mimeData;
    }
    return nullptr;
}

namespace {
bool containsPlainText(const QStringList& list) {
    for (const auto& str : list) {
        if (str.compare("text/plain", Qt::CaseInsensitive) == 0 ||
            str.startsWith("text/plain;charset=", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}
}  // namespace

bool TextEdit::pastingIsPossible() const {
    if (textInteractionFlags() & Qt::TextEditable) {
        const QMimeData* md = QGuiApplication::clipboard()->mimeData();
        return md != nullptr && (md->hasUrls() || (containsPlainText(md->formats()) && !md->text().isEmpty()));
    }
    return false;
}

bool TextEdit::canInsertFromMimeData(const QMimeData* source) const {
    if (!source)
        return false;
    if (source->hasUrls())
        return true;
    return containsPlainText(source->formats()) && !source->text().isEmpty();
}

void TextEdit::insertFromMimeData(const QMimeData* source) {
    if (source == nullptr)
        return;
    if (source->hasUrls()) {
        const QList<QUrl> urlList = source->urls();
        bool multiple(urlList.count() > 1);
        if (!pastePaths_ || multiple) {
            QTextCursor cur = textCursor();
            cur.beginEditBlock();
            for (const QUrl& url : urlList) {
                if (!url.isValid())
                    continue;
                cur.insertText(url.isLocalFile() ? url.toLocalFile() : url.toString(QUrl::EncodeSpaces));
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
                QString scheme = url.scheme();
                if (scheme == "admin")
                    file = url.adjusted(QUrl::NormalizePathSegments).path();
                else if (scheme == "file" || scheme.isEmpty())
                    file = url.adjusted(QUrl::NormalizePathSegments).toLocalFile();
                else
                    continue;
                emit filePasted(file, 0, 0, multiple);
            }
        }
    }
    else if (containsPlainText(source->formats()) && !source->text().isEmpty()) {
        QPlainTextEdit::insertFromMimeData(source);
    }
}

void TextEdit::copyColumn() {
    QString res;
    for (auto const& extra : std::as_const(colSel_)) {
        res.append(extra.cursor.selection().toPlainText());
        res.append('\n');
    }
    if (!res.isEmpty()) {
        res.chop(1);
        if (!res.isEmpty())
            QApplication::clipboard()->setText(res);
    }
}

void TextEdit::cutColumn() {
    if (colSel_.isEmpty())
        return;
    if (colSel_.count() > 1000) {
        emit hugeColumn();
        return;
    }
    QString res;
    QTextCursor cur = textCursor();
    cur.beginEditBlock();
    for (auto const& extra : std::as_const(colSel_)) {
        res.append(extra.cursor.selectedText());
        res.append('\n');
        if (!extra.cursor.hasSelection())
            continue;
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        cur = extra.cursor;
        cur.removeSelectedText();
    }
    cur.endEditBlock();
    if (!res.isEmpty()) {
        res.chop(1);
        if (!res.isEmpty())
            QApplication::clipboard()->setText(res);
    }
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
    for (auto const& extra : std::as_const(colSel_)) {
        if (!extra.cursor.hasSelection())
            continue;
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        cur = extra.cursor;
        cur.removeSelectedText();
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
    bool origMousePressed = mousePressed_;
    mousePressed_ = true;
    QStringList parts = QApplication::clipboard()->text().split('\n');
    QTextCursor cur = textCursor();
    cur.beginEditBlock();
    int i = 0;
    for (auto const& extra : std::as_const(colSel_)) {
        if (i >= parts.size())
            break;
        cur = extra.cursor;
        cur.insertText(parts.at(i));
        ++i;
    }
    cur.endEditBlock();
    mousePressed_ = origMousePressed;
    if (i > 0)
        keepTxtCurHPos_ = false;

    for (auto const& extra : std::as_const(colSel_)) {
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
