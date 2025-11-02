// src/platform/window_utils.cpp
/*
 * window_utils.cpp
 */

#include "texxywindow.h"

#include "ui/tabpage.h"
#include "ui_texxywindow.h"

namespace Texxy {

[[nodiscard]] TabPage* TexxyWindow::currentTabPage() const {
    return qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
}

[[nodiscard]] TextEdit* TexxyWindow::currentTextEdit() const {
    if (const auto* tabPage = currentTabPage())
        return tabPage->textEdit();
    return nullptr;
}

[[nodiscard]] bool TexxyWindow::resolveActiveTextEdit(bool requireWritable, TabPage** outPage, TextEdit** outEdit) {
    if (!isReady())
        return false;

    TabPage* const tabPage = currentTabPage();
    if (!tabPage)
        return false;

    TextEdit* const textEdit = tabPage->textEdit();
    if (!textEdit || (requireWritable && textEdit->isReadOnly()))
        return false;

    if (outPage)
        *outPage = tabPage;
    if (outEdit)
        *outEdit = textEdit;

    return true;
}

[[nodiscard]] bool TexxyWindow::lineContextVisible() const {
    return ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible();
}

QList<QTextEdit::ExtraSelection> TexxyWindow::composeSelections(TextEdit* textEdit,
                                                                const QList<QTextEdit::ExtraSelection>& primary) const {
    // bind as const refs to avoid copies if getters return references
    const auto& blueSel = textEdit->getBlueSel();
    const auto& colSel = textEdit->getColSel();
    const auto& redSel = textEdit->getRedSel();
    const bool includeLineContext = lineContextVisible();

    QList<QTextEdit::ExtraSelection> composed;
    const int extra = static_cast<int>(primary.size() + blueSel.size() + colSel.size() + redSel.size());
    composed.reserve(extra + (includeLineContext ? 1 : 0));

    if (includeLineContext)
        composed.append(textEdit->currentLineSelection());

    composed += primary;
    composed += blueSel;
    composed += colSel;
    composed += redSel;

    return composed;
}

}  // namespace Texxy
