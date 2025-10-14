/*
  texxy/encoding.cpp
  helpers for UTF-8 validation and charset detection
*/

#include "encoding.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string_view>

// keep heavy logic out of Qt namespace pollution
namespace {

// returns pointer advanced past consecutive ASCII bytes using wide chunks
// uses memcpy to avoid UB on unaligned loads
static inline const unsigned char* skip_ascii(const unsigned char* p, const unsigned char* end) noexcept {
    // process 8 bytes at a time while possible
    while (static_cast<size_t>(end - p) >= sizeof(uint64_t)) {
        uint64_t chunk;
        std::memcpy(&chunk, p, sizeof(chunk));
        if (chunk & 0x8080808080808080ULL)
            break;  // some byte has high bit set
        p += sizeof(uint64_t);
    }
    // process 4 bytes
    if (static_cast<size_t>(end - p) >= sizeof(uint32_t)) {
        uint32_t chunk32;
        std::memcpy(&chunk32, p, sizeof(chunk32));
        if ((chunk32 & 0x80808080U) == 0) {
            p += sizeof(uint32_t);
        }
    }
    // process residual bytes
    while (p < end && (*p < 0x80))
        ++p;
    return p;
}

// strict UTF-8 validator rejecting overlongs and surrogates
static inline bool validate_utf8(std::string_view bytes) noexcept {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(bytes.data());
    const unsigned char* const end = p + bytes.size();

    while (p < end) {
        p = skip_ascii(p, end);
        if (p >= end)
            break;

        const unsigned char c = *p++;

        // two-byte form: [C2..DF] [80..BF]
        if (c >= 0xC2 && c <= 0xDF) {
            if (p == end)
                return false;  // need 1 more byte
            if ((p[0] & 0xC0) != 0x80)
                return false;  // invalid continuation
            ++p;
            continue;
        }

        // three-byte form: [E0..EF] [80..BF] [80..BF]
        if (c >= 0xE0 && c <= 0xEF) {
            if (end - p < 2)
                return false;  // need 2 more bytes
            const unsigned char c1 = p[0], c2 = p[1];
            if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80)
                return false;  // invalid continuation

            // minimal checks
            if (c == 0xE0 && c1 < 0xA0)
                return false;  // overlong
            if (c == 0xED && c1 >= 0xA0)
                return false;  // surrogate half

            // extra surrogate guard
            const unsigned int code = ((c & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
            if (code >= 0xD800 && code <= 0xDFFF)
                return false;  // surrogates invalid
            p += 2;
            continue;
        }

        // four-byte form: [F0..F4] [80..BF] [80..BF] [80..BF]
        if (c >= 0xF0 && c <= 0xF4) {
            if (end - p < 3)
                return false;  // need 3 more bytes
            const unsigned char c1 = p[0], c2 = p[1], c3 = p[2];
            if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
                return false;  // invalid continuation

            // minimal and upper bound checks
            if (c == 0xF0 && c1 < 0x90)
                return false;  // overlong
            if (c == 0xF4 && c1 > 0x8F)
                return false;  // beyond U+10FFFF

            const unsigned int code = ((c & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            if (code > 0x10FFFF)
                return false;  // outside Unicode range
            p += 3;
            continue;
        }

        // anything in [80..C1] or [F5..FF] is invalid
        return false;
    }

    return true;
}

// minimal BOM probe to prefer explicit encodings when possible
static inline const char* probe_bom(std::string_view bytes) noexcept {
    const size_t n = bytes.size();
    const unsigned char* d = reinterpret_cast<const unsigned char*>(bytes.data());

    if (n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF)
        return "UTF-8";  // UTF-8 BOM
    if (n >= 2 && d[0] == 0xFE && d[1] == 0xFF)
        return "UTF-16BE";  // UTF-16 BE BOM
    if (n >= 4 && d[0] == 0x00 && d[1] == 0x00 && d[2] == 0xFE && d[3] == 0xFF)
        return "UTF-32BE";  // UTF-32 BE BOM
    if (n >= 4 && d[0] == 0xFF && d[1] == 0xFE && d[2] == 0x00 && d[3] == 0x00)
        return "UTF-32LE";  // UTF-32 LE BOM
    if (n >= 2 && d[0] == 0xFF && d[1] == 0xFE)
        return "UTF-16LE";  // UTF-16 LE BOM
    return nullptr;
}

}  // anonymous namespace

namespace Texxy {

QString detectCharset(const QByteArray& byteArray) {
    const auto* data = byteArray.constData();
    const auto size = static_cast<size_t>(byteArray.size());
    const std::string_view sv{data, size};

    if (const char* bom = probe_bom(sv))
        return QString::fromLatin1(bom);

    if (validate_utf8(sv))
        return QStringLiteral("UTF-8");

    // fallback since legacy codecs are typically unavailable on Qt6 by default
    return QStringLiteral("ISO-8859-1");
}

}  // namespace Texxy
