#include "textedit/textedit_prelude.h"

#include "ui/ui/vscrollbar.h"

namespace Texxy {

namespace {

// clamp and return an 8-bit integer
inline int clamp8(int v) {
    return std::clamp(v, 0, 255);
}

// compute separator color alpha for light/dark backgrounds
inline int separatorAlphaFor(int bg, bool darkMode) {
    // original intent preserved, but clamped to 0..255
    const int a = darkMode ? (95 - std::lround(3.0 * bg / 5.0)) : (2 * std::lround(bg / 5.0) - 32);
    return clamp8(a);
}

// ensure selection contrast by either tweaking inactive palette or falling back to a minimal stylesheet
inline void ensureSelectionContrast(QWidget* w, QPalette& pal, int bgGray) {
    QColor hi = pal.highlight().color();
    const int delta = std::abs(bgGray - qGray(hi.rgb()));
    const bool lowSat = hi.hslSaturation() < 100;
    if (delta < 30 && lowSat) {
        // try aligning inactive highlight to active before using a stylesheet
        pal.setColor(QPalette::Inactive, QPalette::Highlight, pal.highlight().color());
        pal.setColor(QPalette::Inactive, QPalette::HighlightedText, pal.highlightedText().color());
        w->setPalette(pal);
        // if still low contrast, fall back to explicit selection colors
        QColor hi2 = w->palette().highlight().color();
        if (std::abs(bgGray - qGray(hi2.rgb())) < 30 && hi2.hslSaturation() < 100) {
            w->setStyleSheet(
                "QPlainTextEdit {"
                "selection-background-color: rgb(180, 180, 180);"
                "selection-color: black}");
        }
    }
}

// set viewport foreground/background quickly with a tiny stylesheet
inline void applyViewportTheme(QWidget* viewport, int gray, bool darkMode) {
    // rely on palette for text base colors, only enforce bg/fg on the viewport for fast repaint
    viewport->setStyleSheet(QString::fromLatin1(".QWidget { color: %1; background-color: rgb(%2,%2,%2) }")
                                .arg(darkMode ? QStringLiteral("white") : QStringLiteral("black"))
                                .arg(gray));
}

}  // namespace

TextEdit::TextEdit(QWidget* parent, int bgColorValue) : QPlainTextEdit(parent) {
    prevAnchor_ = prevPos_ = -1;
    widestDigit_ = 0;
    autoIndentation_ = true;
    autoReplace_ = true;
    autoBracket_ = false;
    drawIndetLines_ = false;
    saveCursor_ = false;
    pastePaths_ = false;
    vLineDistance_ = 0;
    matchedBrackets_ = false;

    inertialScrolling_ = false;
    scrollTimer_ = nullptr;

    mousePressed_ = false;

    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;

    prog_ = "url";  // default language

    textTab_ = "    ";  // default text tab is four spaces

    resizeTimerId_ = 0;
    selectionTimerId_ = 0;
    selectionHighlighting_ = false;
    highlightThisSelection_ = true;
    removeSelectionHighlights_ = false;
    size_ = 0;
    wordNumber_ = -1;  // not calculated yet
    encoding_ = "UTF-8";
    uneditable_ = false;

    setMouseTracking(true);
    // document()->setUseDesignMetrics(true)

    // background color and selection contrast setup
    QPalette pal = palette();
    bgColorValue = clamp8(bgColorValue);
    if (bgColorValue < 230 && bgColorValue > 50)
        bgColorValue = 230;  // avoid mid gray that reduces readability

    if (bgColorValue < 230) {
        darkValue_ = bgColorValue;
        applyViewportTheme(viewport(), bgColorValue, true);
        ensureSelectionContrast(this, pal, bgColorValue);
        separatorColor_ = Qt::white;
        separatorColor_.setAlpha(separatorAlphaFor(darkValue_, true));
    }
    else {
        darkValue_ = -1;
        applyViewportTheme(viewport(), bgColorValue, false);
        // for light backgrounds, prefer palette alignment first
        ensureSelectionContrast(this, pal, bgColorValue);
        separatorColor_ = Qt::black;
        separatorColor_.setAlpha(separatorAlphaFor(bgColorValue, false));
    }

    setCurLineHighlight(-1);

    setFrameShape(QFrame::NoFrame);

    // replace vertical scrollbar with a faster wheel-handling one
    auto* vScrollBar = new VScrollBar;
    setVerticalScrollBar(vScrollBar);

    lineNumberArea_ = new LineNumberArea(this);
    lineNumberArea_->setToolTip(tr("Double click to center current line"));
    lineNumberArea_->hide();
    lineNumberArea_->installEventFilter(this);

    connect(this, &QPlainTextEdit::updateRequest, this, &TextEdit::onUpdateRequesting);
    connect(this, &QPlainTextEdit::cursorPositionChanged, [this] {
        if (!keepTxtCurHPos_)
            txtCurHPos_ = -1;  // forget last cursor x if it shouldn't be remembered
        emit updateBracketMatching();
        // remove column highlight if no mouse button is pressed
        if (!colSel_.isEmpty() && !mousePressed_)
            removeColumnHighlight();
    });
    connect(this, &QPlainTextEdit::selectionChanged, this, &TextEdit::onSelectionChanged);
    connect(this, &QPlainTextEdit::copyAvailable, [this](bool yes) {
        if (yes)
            emit canCopy(true);
        else if (colSel_.isEmpty())
            emit canCopy(false);
    });

    setContextMenuPolicy(Qt::CustomContextMenu);
}

/*************************/
void TextEdit::setEditorFont(const QFont& f, bool setDefault) {
    if (setDefault)
        font_ = f;

    setFont(f);
    viewport()->setFont(f);  // needed when whitespaces are shown
    document()->setDefaultFont(f);

    // consistent tabs
    const QFontMetricsF metrics(f);
    QTextOption opt = document()->defaultTextOption();
    opt.setTabStopDistance(metrics.horizontalAdvance(textTab_));
    document()->setDefaultTextOption(opt);

    // line number font is bold only for the current line
    QFont LF(f);
    if (f.bold()) {
        LF.setBold(false);
        lineNumberArea_->setFont(LF);
    }
    else {
        lineNumberArea_->setFont(f);
    }

    // find widest digit for line number area width
    LF.setBold(true);  // bold for current line numbers
    widestDigit_ = 0;
    int maxW = 0;
    for (int i = 0; i < 10; ++i) {
        const int w = QFontMetrics(LF).horizontalAdvance(QString::number(i));
        if (w > maxW) {
            maxW = w;
            widestDigit_ = i;
        }
    }
}

/*************************/
TextEdit::~TextEdit() {
    if (scrollTimer_) {
        disconnect(scrollTimer_, &QTimer::timeout, this, &TextEdit::scrollWithInertia);
        scrollTimer_->stop();
        delete scrollTimer_;
    }
    delete lineNumberArea_;
}

/*************************/
void TextEdit::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);

    const QRect cr = contentsRect();
    const int lw = lineNumberAreaWidth();
    const bool rtl = QApplication::layoutDirection() == Qt::RightToLeft;
    lineNumberArea_->setGeometry(QRect(rtl ? cr.width() - lw : cr.left(), cr.top(), lw, cr.height()));

    if (lineWrapMode() != QPlainTextEdit::NoWrap)
        removeColumnHighlight();

    if (resizeTimerId_) {
        killTimer(resizeTimerId_);
        resizeTimerId_ = 0;
    }
    resizeTimerId_ = startTimer(kUpdateIntervalMs);
}

/*************************/
void TextEdit::timerEvent(QTimerEvent* event) {
    QPlainTextEdit::timerEvent(event);

    if (event->timerId() == resizeTimerId_) {
        killTimer(event->timerId());
        resizeTimerId_ = 0;
        emit resized();
    }
    else if (event->timerId() == selectionTimerId_) {
        killTimer(event->timerId());
        selectionTimerId_ = 0;
        selectionHlight();
        emit selChanged();
    }
}

void TextEdit::onUpdateRequesting(const QRect& /*rect*/, int dy) {
    // only care about vertical text scrolling, not blinking cursor updates
    if (dy == 0)
        return;

    // QPlainTextEdit::updateRequest gives the whole rect on scroll, so we ignore it
    emit updateRect();

    // previously invisible brackets may appear after scroll
    if (!matchedBrackets_ && isVisible())
        emit updateBracketMatching();
}

/*************************/
// if a page shows very briefly, updateRect might fire while the page is hidden
// guard by emitting updateRect on show and request bracket matching if needed
void TextEdit::showEvent(QShowEvent* event) {
    QPlainTextEdit::showEvent(event);
    emit updateRect();
    if (!matchedBrackets_)
        emit updateBracketMatching();
}

/*************************/
bool TextEdit::event(QEvent* event) {
    if (highlighter_ &&
        ((event->type() == QEvent::WindowDeactivate && hasFocus())  // another window is activated
         || event->type() == QEvent::FocusOut)                      // another widget has been focused
        && viewport()->cursor().shape() != Qt::IBeamCursor) {
        viewport()->setCursor(Qt::IBeamCursor);
    }
    return QPlainTextEdit::event(event);
}

}  // namespace Texxy
