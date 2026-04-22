//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "TextEncoding.hpp"
using namespace fbide;

namespace {

struct TextEncodingInfo {
    TextEncoding::Value value;
    std::string_view configKey;
    wxBOM bom;
    wxFontEncoding fontEncoding;
};

// Stable config keys; wxBOM map; wxFontEncoding map.
// clang-format off
constexpr std::array kTextEncodingInfo {
    TextEncodingInfo { TextEncoding::UTF8,         "UTF-8",        wxBOM_None,     wxFONTENCODING_UTF8      },
    TextEncodingInfo { TextEncoding::UTF8_BOM,     "UTF-8-BOM",    wxBOM_UTF8,     wxFONTENCODING_UTF8      },
    TextEncodingInfo { TextEncoding::UTF16_LE,     "UTF-16-LE",    wxBOM_UTF16LE,  wxFONTENCODING_UTF16LE   },
    TextEncodingInfo { TextEncoding::UTF16_BE,     "UTF-16-BE",    wxBOM_UTF16BE,  wxFONTENCODING_UTF16BE   },
    TextEncodingInfo { TextEncoding::Windows_1252, "Windows-1252", wxBOM_None,     wxFONTENCODING_CP1252    },
    TextEncodingInfo { TextEncoding::Windows_1250, "Windows-1250", wxBOM_None,     wxFONTENCODING_CP1250    },
    TextEncodingInfo { TextEncoding::Windows_1251, "Windows-1251", wxBOM_None,     wxFONTENCODING_CP1251    },
    TextEncodingInfo { TextEncoding::CP437,        "CP437",        wxBOM_None,     wxFONTENCODING_CP437     },
    TextEncodingInfo { TextEncoding::CP850,        "CP850",        wxBOM_None,     wxFONTENCODING_CP850     },
    TextEncodingInfo { TextEncoding::ISO_8859_1,   "ISO-8859-1",   wxBOM_None,     wxFONTENCODING_ISO8859_1 },
    TextEncodingInfo { TextEncoding::System,       "System",       wxBOM_None,     wxFONTENCODING_SYSTEM    },
};
// clang-format on

static_assert(kTextEncodingInfo.size() == TextEncoding::all.size(),
    "kTextEncodingInfo must cover every TextEncoding value");

constexpr auto infoFor(const TextEncoding::Value enc) -> const TextEncodingInfo& {
    for (const auto& info : kTextEncodingInfo) {
        if (info.value == enc) {
            return info;
        }
    }
    std::unreachable();
}

struct EolModeInfo {
    EolMode::Value value;
    std::string_view configKey;
    int stcMode;
};

constexpr std::array kEolModeInfo {
    EolModeInfo { EolMode::LF, "LF", wxSTC_EOL_LF },
    EolModeInfo { EolMode::CRLF, "CRLF", wxSTC_EOL_CRLF },
    EolModeInfo { EolMode::CR, "CR", wxSTC_EOL_CR },
};

static_assert(kEolModeInfo.size() == EolMode::all.size(),
    "kEolModeInfo must cover every EolMode value");

constexpr auto infoFor(const EolMode::Value mode) -> const EolModeInfo& {
    for (const auto& info : kEolModeInfo) {
        if (info.value == mode) {
            return info;
        }
    }
    std::unreachable();
}

/// Run `func(conv)` with a wxMBConv suitable for the given encoding.
/// Stack-allocated converters — cheap and avoids heap.
template<typename F>
auto withConverter(const TextEncoding::Value enc, F&& func) {
    switch (enc) {
    case TextEncoding::UTF8:
    case TextEncoding::UTF8_BOM: {
        wxMBConvUTF8 conv;
        return func(conv);
    }
    case TextEncoding::UTF16_LE: {
        wxMBConvUTF16LE conv;
        return func(conv);
    }
    case TextEncoding::UTF16_BE: {
        wxMBConvUTF16BE conv;
        return func(conv);
    }
    case TextEncoding::Windows_1252:
    case TextEncoding::Windows_1250:
    case TextEncoding::Windows_1251:
    case TextEncoding::CP437:
    case TextEncoding::CP850:
    case TextEncoding::ISO_8859_1:
    case TextEncoding::System: {
        wxCSConv conv(infoFor(enc).fontEncoding);
        return func(conv);
    }
    }
    std::unreachable();
}

} // namespace

// ---------------------------------------------------------------------------
// TextEncoding
// ---------------------------------------------------------------------------

auto TextEncoding::toString() const -> std::string_view {
    return infoFor(m_encoding).configKey;
}

auto TextEncoding::parse(const std::string_view key) -> std::optional<TextEncoding> {
    for (const auto& info : kTextEncodingInfo) {
        if (info.configKey == key) {
            return TextEncoding { info.value };
        }
    }
    return std::nullopt;
}

auto TextEncoding::toWxBom() const -> wxBOM {
    return infoFor(m_encoding).bom;
}

auto TextEncoding::fromWxBom(const wxBOM bom) -> std::optional<TextEncoding> {
    if (bom == wxBOM_None || bom == wxBOM_Unknown) {
        return std::nullopt;
    }
    for (const auto& info : kTextEncodingInfo) {
        if (info.bom == bom) {
            return TextEncoding { info.value };
        }
    }
    return std::nullopt;
}

auto TextEncoding::bomLength() const -> std::size_t {
    const auto bom = toWxBom();
    if (bom == wxBOM_None) {
        return 0;
    }
    size_t count = 0;
    wxConvAuto::GetBOMChars(bom, &count);
    return count;
}

auto TextEncoding::bomBytes() const -> std::span<const std::byte> {
    const auto bom = toWxBom();
    if (bom == wxBOM_None) {
        return {};
    }
    size_t count = 0;
    const char* ptr = wxConvAuto::GetBOMChars(bom, &count);
    return { reinterpret_cast<const std::byte*>(ptr), count };
}

auto TextEncoding::encode(const wxString& text) const -> std::optional<wxCharBuffer> {
    return withConverter(m_encoding, [&](wxMBConv& conv) -> std::optional<wxCharBuffer> {
        if (text.empty()) {
            return wxCharBuffer { static_cast<size_t>(0) };
        }
        const auto wide = text.wc_str();
        const size_t wideLen = wxWcslen(wide);
        const size_t outSize = conv.FromWChar(nullptr, 0, wide, wideLen);
        if (outSize == wxCONV_FAILED) {
            return std::nullopt;
        }
        wxCharBuffer buf(outSize);
        conv.FromWChar(buf.data(), outSize, wide, wideLen);
        return buf;
    });
}

auto TextEncoding::decode(const void* bytes, const std::size_t len) const -> std::optional<wxString> {
    return withConverter(m_encoding, [&](wxMBConv& conv) -> std::optional<wxString> {
        if (len == 0) {
            return wxString {};
        }
        const auto* src = static_cast<const char*>(bytes);
        const size_t wideLen = conv.ToWChar(nullptr, 0, src, len);
        if (wideLen == wxCONV_FAILED) {
            return std::nullopt;
        }
        wxWCharBuffer buf(wideLen);
        conv.ToWChar(buf.data(), wideLen, src, len);
        return wxString(buf.data(), wideLen);
    });
}

// ---------------------------------------------------------------------------
// EolMode
// ---------------------------------------------------------------------------

auto EolMode::toString() const -> std::string_view {
    return infoFor(m_mode).configKey;
}

auto EolMode::parse(const std::string_view key) -> std::optional<EolMode> {
    for (const auto& info : kEolModeInfo) {
        if (info.configKey == key) {
            return EolMode { info.value };
        }
    }
    return std::nullopt;
}

auto EolMode::toStc() const -> int {
    return infoFor(m_mode).stcMode;
}

auto EolMode::fromStc(const int stcEolMode) -> EolMode {
    for (const auto& info : kEolModeInfo) {
        if (info.stcMode == stcEolMode) {
            return EolMode { info.value };
        }
    }
    return EolMode { LF };
}
