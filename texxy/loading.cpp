/*
  texxy/loading.cpp
*/

#include "loading.h"
#include "encoding.h"

#include <QFile>
#include <QStringDecoder>
#include <QtGlobal>

namespace Texxy {

static inline void appendHugeLineNotice(QString& out) {
    // same message as original path but appended after streaming decode
    static const QLatin1String msg("    HUGE LINE TRUNCATED: NO LINE WITH MORE THAN 500000 CHARACTERS");
    out += msg;
}

static inline QChar hexDigit(quint8 value) {
    return QLatin1Char(value < 10 ? static_cast<char>('0' + value) : static_cast<char>('A' + value - 10));
}

static QString toHexView(const uchar* data, qint64 length) {
    if (!data || length <= 0)
        return QString();

    constexpr int bytesPerLine = 16;
    const qint64 lineCount = (length + bytesPerLine - 1) / bytesPerLine;
    QString out;
    out.reserve(static_cast<int>(lineCount * (bytesPerLine * 3 + bytesPerLine + 16)));

    for (qint64 offset = 0; offset < length; offset += bytesPerLine) {
        const int lineLen = static_cast<int>(qMin<qint64>(bytesPerLine, length - offset));
        out.append(QStringLiteral("%1  ").arg(offset, 8, 16, QLatin1Char('0')).toUpper());

        for (int i = 0; i < bytesPerLine; ++i) {
            if (i < lineLen) {
                const uchar byte = data[offset + i];
                out.append(hexDigit(byte >> 4));
                out.append(hexDigit(byte & 0x0F));
            }
            else {
                out.append(QStringLiteral("  "));
            }

            if (i != bytesPerLine - 1)
                out.append(QLatin1Char(' '));
            if (i == 7)
                out.append(QLatin1Char(' '));
        }

        out.append(QStringLiteral(" |"));
        for (int i = 0; i < lineLen; ++i) {
            const uchar byte = data[offset + i];
            out.append((byte >= 0x20 && byte <= 0x7E) ? QLatin1Char(byte) : QLatin1Char('.'));
        }
        for (int i = lineLen; i < bytesPerLine; ++i)
            out.append(QLatin1Char(' '));

        out.append(QStringLiteral("|\n"));
    }

    return out;
}

// scan buffer to
//  - detect presence of NULs
//  - compute first huge-line cutoff index if any
//  - keep simple UTF-16/32 heuristics from the original logic
// returns cutoff index or -1 if no cutoff is needed
struct ScanResult {
    bool hasNull = false;
    bool likelyUtf16 = false;
    bool likelyUtf32 = false;
    qint64 cutoff = -1;
};

static inline ScanResult scanBuffer(const uchar* begin, const uchar* end, bool enforced) {
    ScanResult r;
    if (!begin || begin >= end)
        return r;

    const qint64 len = end - begin;

    // peek first up to 4 bytes for UTF-16/32 heuristics
    uchar C[4] = {0, 0, 0, 0};
    const int peeks = static_cast<int>(qMin<qint64>(4, len));
    for (int i = 0; i < peeks; ++i) {
        C[i] = begin[i];
        if (C[i] == 0x00)
            r.hasNull = true;
    }

    if (!enforced) {
        if (peeks == 2 && ((C[0] != 0x00 && C[1] == 0x00) || (C[0] == 0x00 && C[1] != 0x00)))
            r.likelyUtf16 = true;  // single 2-byte char with alternating NUL suggests UTF-16

        if (peeks == 4) {
            const bool bom16le = (C[0] == 0xFF && C[1] == 0xFE);
            const bool bom16be = (C[0] == 0xFE && C[1] == 0xFF);
            const bool le16pat = (C[0] != 0x00 && C[1] == 0x00 && C[2] != 0x00 && C[3] == 0x00);
            const bool be16pat = (C[0] == 0x00 && C[1] != 0x00 && C[2] == 0x00 && C[3] != 0x00);
            const bool le32pat = (C[0] != 0x00 && C[1] != 0x00 && C[2] == 0x00 && C[3] == 0x00);
            const bool be32pat = (C[0] == 0x00 && C[1] == 0x00 && C[2] != 0x00 && C[3] != 0x00);

            if (r.hasNull && (le16pat || be16pat || bom16le || bom16be))
                r.likelyUtf16 = true;
            else if (r.hasNull && (le32pat || be32pat))
                r.likelyUtf32 = true;
        }

        // if BOM looks like UTF-16 but we only saw 2 bytes, try to confirm NUL presence
        if (peeks == 2 && !r.likelyUtf16 && ((C[0] == 0xFF && C[1] == 0xFE) || (C[0] == 0xFE && C[1] == 0xFF))) {
            const int extra = static_cast<int>(qMin<qint64>(6, len - 2));
            for (int i = 0; i < extra; ++i)
                r.hasNull |= (begin[2 + i] == 0x00);
            if (r.hasNull)
                r.likelyUtf16 = true;
        }
    }

    // compute huge-line cutoff in a single walk without building a copy
    // thresholds match original logic
    const int thresholdText = 500000;
    const int thresholdWide = 500004;  // multiple of 4
    int lineLen = 0;
    const int threshold = (enforced || r.likelyUtf16 || r.likelyUtf32) ? thresholdWide : thresholdText;

    // if we already peeked into the first bytes, seed line length
    lineLen = peeks;

    // include the peeked bytes in cutoff consideration
    for (qint64 i = 0; i < peeks; ++i) {
        const char c = static_cast<char>(C[i]);
        if (c == '\n' || c == '\r')
            lineLen = 0;
        if (lineLen > threshold && r.cutoff < 0)
            r.cutoff = i;  // cutoff occurs right before this byte
    }

    // continue scanning from after the peek
    for (const uchar* p = begin + peeks; p < end; ++p) {
        const char c = static_cast<char>(*p);
        r.hasNull |= (c == '\0');
        if (c == '\n' || c == '\r')
            lineLen = 0;
        ++lineLen;

        if (lineLen > threshold && r.cutoff < 0) {
            // cutoff index is number of bytes to keep from start
            r.cutoff = p - begin - (enforced || r.likelyUtf16 || r.likelyUtf32 ? ((lineLen - threshold) % 4) : 0);
            break;  // first cutoff position is enough
        }
    }

    return r;
}

Loading::Loading(const QString& fname,
                 const QString& charset,
                 bool reload,
                 int restoreCursor,
                 int posInLine,
                 bool forceUneditable,
                 bool multiple)
    : fname_(fname),
      charset_(charset),
      reload_(reload),
      restoreCursor_(restoreCursor),
      posInLine_(posInLine),
      forceUneditable_(forceUneditable),
      multiple_(multiple),
      skipNonText_(true) {}

Loading::~Loading() {}

void Loading::run() {
    if (!QFile::exists(fname_)) {
        emit completed(QString(), fname_, charset_.isEmpty() ? "UTF-8" : charset_, false, false, 0, 0, false,
                       multiple_);
        return;
    }

    QFile file(fname_);
    const bool hexMode = charset_.compare(QStringLiteral("Hex"), Qt::CaseInsensitive) == 0;
    const qint64 sizeLimit = hexMode ? 32LL * 1024 * 1024 : 100LL * 1024 * 1024;  // tighter guard for hex view
    if (file.size() > sizeLimit) {
        emit completed(QString(), fname_);
        return;
    }
    if (!file.open(QFile::ReadOnly)) {
        emit completed();
        return;
    }

    const qint64 fsz = file.size();
    const uchar* map = fsz ? file.map(0, fsz) : nullptr;

    QByteArray fallback;
    if (!map) {
        // fall back to a single read into a non-owning QByteArray view to avoid per-byte appends
        fallback = file.readAll();
        map = reinterpret_cast<const uchar*>(fallback.constData());
    }

    const uchar* const begin = map;
    const uchar* const end = map + (map ? (fallback.isEmpty() ? fsz : fallback.size()) : 0);

    if (hexMode) {
        const qint64 len = end - begin;
        QString hexText = toHexView(begin, len);
        file.close();
        emit completed(hexText,
                       fname_,
                       QStringLiteral("Hex"),
                       false,
                       reload_,
                       restoreCursor_,
                       posInLine_,
                       forceUneditable_,
                       multiple_);
        return;
    }

    const bool enforced = !charset_.isEmpty();

    // fast scan to determine nulls, cutoff, and wide enc guesses
    const ScanResult scan = scanBuffer(begin, end, enforced);

    // skip non-text if configured and nulls found with no charset decision
    if (!enforced && skipNonText_ && scan.hasNull && charset_.isEmpty()) {
        file.close();
        emit completed(QString(), QString(), "UTF-8");
        return;
    }

    // decide charset
    if (charset_.isEmpty()) {
        if (scan.hasNull) {
            // treat as non-text but still open as UTF-8 like original
            forceUneditable_ = true;
            charset_ = "UTF-8";
        }
        else if (scan.likelyUtf16) {
            charset_ = "UTF-16";
        }
        else if (scan.likelyUtf32) {
            charset_ = "UTF-32";
        }
        else {
            // zero-copy view into mapped data for detection to avoid copying the whole file
            const QByteArray raw =
                QByteArray::fromRawData(reinterpret_cast<const char*>(begin), static_cast<int>(end - begin));
            charset_ = detectCharset(raw);
        }
    }

    // choose decoder once
    const auto conv = charset_ == "UTF-8"    ? QStringConverter::Utf8
                      : charset_ == "UTF-16" ? QStringConverter::Utf16
                      : charset_ == "UTF-32" ? QStringConverter::Utf32
                                             : QStringConverter::Latin1;

    QStringDecoder decoder(conv);

    // stream decode directly from mapped memory to avoid building a second full-size buffer
    // if we need to truncate a huge line, decode only up to cutoff and then append the notice
    QString text;
    text.reserve(static_cast<int>(qMin<qint64>(fsz, 1'500'000)));  // rough reservation to reduce reallocs

    const qint64 keepLen = scan.cutoff >= 0 ? scan.cutoff : (end - begin);
    if (keepLen > 0) {
        // decode in large chunks to be friendly to allocator for very large files
        constexpr qint64 CHUNK = 1 << 20;  // 1 MiB chunks
        qint64 processed = 0;
        while (processed < keepLen) {
            const qint64 n = qMin(CHUNK, keepLen - processed);
            const auto view = QByteArrayView(reinterpret_cast<const char*>(begin + processed), static_cast<int>(n));
            text += decoder.decode(view);
            processed += n;
        }
    }

    // finalize the decoder state
    text += decoder.decode({});  // flush any pending partial sequence

    if (scan.cutoff >= 0) {
        appendHugeLineNotice(text);
        forceUneditable_ = true;
    }

    file.close();

    emit completed(text, fname_, charset_, enforced, reload_, restoreCursor_, posInLine_, forceUneditable_, multiple_);
}

}  // namespace Texxy
