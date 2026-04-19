//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ThemeOld.hpp"
using namespace fbide;

namespace {
auto colorFromBgr(unsigned int bgr) -> wxColour {
    return {
        static_cast<unsigned char>((bgr >> 16) & 0xFF),
        static_cast<unsigned char>((bgr >> 8) & 0xFF),
        static_cast<unsigned char>(bgr & 0xFF)
    };
}

auto colorToBgr(const wxColour& colour) -> unsigned int {
    return (static_cast<unsigned int>(colour.Red()) << 16)
         | (static_cast<unsigned int>(colour.Green()) << 8)
         | static_cast<unsigned int>(colour.Blue());
}

constexpr std::array sectionNames {
    "default", "comment", "number", "keyword", "string",
    "preprocessor", "operator", "identifier", "date",
    "stringeol", "keyword2", "keyword3", "keyword4",
    "constant", "asm"
};

auto readColor(const wxFileConfig& ini, const wxString& key, const wxColour& fallback) -> wxColour {
    return colorFromBgr(static_cast<unsigned int>(ini.ReadLong(key, static_cast<long>(colorToBgr(fallback)))));
}

void writeColor(wxFileConfig& ini, const wxString& key, const wxColour& colour) {
    ini.Write(key, static_cast<long>(colorToBgr(colour)));
}

auto readFontStyle(const wxFileConfig& ini, ThemeOld::FontStyle fallback) -> ThemeOld::FontStyle {
    long val = ini.ReadLong("fontstyle", -1);
    if (val == -1) {
        val = ini.ReadLong("fonstyle", static_cast<long>(fallback.flags()));
    }
    return ThemeOld::FontStyle(static_cast<int>(val));
}

template<typename T>
void readColors(const wxFileConfig& ini, T& pair, const wxColour& defBg, const wxColour& defFg) {
    pair.background = readColor(ini, "background", defBg);
    pair.foreground = readColor(ini, "foreground", defFg);
}

template<typename T>
void writeColors(wxFileConfig& ini, const T& pair) {
    writeColor(ini, "background", pair.background);
    writeColor(ini, "foreground", pair.foreground);
}
} // namespace

void ThemeOld::load(const wxString& path) {
    m_themePath = path;
    wxFFileInputStream stream(path);
    if (!stream.IsOk()) {
        return;
    }

    wxFileConfig ini(stream);

    // [default]
    ini.SetPath("/default");
    m_editor.background = readColor(ini, "background", *wxWHITE);
    m_editor.foreground = readColor(ini, "foreground", *wxBLACK);
    m_editor.caretColour = readColor(ini, "caret", *wxBLACK);
    m_editor.caretLine = readColor(ini, "caretline", wxColour(0xDD, 0xDD, 0xDD));
    m_editor.fontSize = static_cast<int>(ini.ReadLong("fontsize", 12));
    m_editor.fontStyle = FontStyle(static_cast<int>(ini.ReadLong("fontstyle", 0)));
    m_editor.fontName = ini.Read("font", "");

    // [linenumber]
    ini.SetPath("/linenumber");
    readColors(ini, m_lineNumber, wxColour(0xC0, 0xC0, 0xC0), *wxWHITE);

    // [select]
    ini.SetPath("/select");
    readColors(ini, m_selection, wxColour(0xC0, 0xC0, 0xC0), *wxWHITE);

    // [brace]
    ini.SetPath("/brace");
    readColors(ini, m_brace, m_editor.foreground, m_editor.background);
    m_brace.fontStyle = readFontStyle(ini, FontStyle {});

    // [badbrace]
    ini.SetPath("/badbrace");
    readColors(ini, m_badBrace, m_editor.foreground, *wxBLACK);
    m_badBrace.fontStyle = readFontStyle(ini, FontStyle {});

    // Per-style entries (1-14)
    for (size_t idx = 1; idx < sectionNames.size(); idx++) {
        ini.SetPath(wxString("/") + sectionNames[idx]);
        auto& [foreground, background, fontName, fontSize, fontStyle, letterCase] = m_styles[idx];
        background = readColor(ini, "background", m_editor.background);
        foreground = readColor(ini, "foreground", m_editor.foreground);
        fontSize = static_cast<int>(ini.ReadLong("fontsize", m_editor.fontSize));
        letterCase = static_cast<int>(ini.ReadLong("capital", 0));
        fontStyle = readFontStyle(ini, m_editor.fontStyle);
        fontName = ini.Read("font", m_editor.fontName);
    }
}

void ThemeOld::save() const {
    wxAppConsole console;

    if (m_themePath.empty()) {
        return;
    }

    wxFileConfig ini;

    // [default]
    ini.SetPath("/default");
    writeColors(ini, m_editor);
    writeColor(ini, "caret", m_editor.caretColour);
    writeColor(ini, "caretline", m_editor.caretLine);
    ini.Write("fontsize", static_cast<long>(m_editor.fontSize));
    ini.Write("fontstyle", static_cast<long>(m_editor.fontStyle.flags()));
    ini.Write("font", m_editor.fontName);

    // [linenumber]
    ini.SetPath("/linenumber");
    writeColors(ini, m_lineNumber);

    // [select]
    ini.SetPath("/select");
    writeColors(ini, m_selection);

    // [brace]
    ini.SetPath("/brace");
    writeColors(ini, m_brace);
    ini.Write("fontstyle", static_cast<long>(m_brace.fontStyle.flags()));

    // [badbrace]
    ini.SetPath("/badbrace");
    writeColors(ini, m_badBrace);
    ini.Write("fontstyle", static_cast<long>(m_badBrace.fontStyle.flags()));

    // Per-style entries (1-14)
    for (size_t idx = 1; idx < sectionNames.size(); idx++) {
        ini.SetPath(wxString("/") + sectionNames[idx]);
        const auto& [foreground, background, fontName, fontSize, fontStyle, letterCase] = m_styles[idx];
        writeColor(ini, "background", background);
        writeColor(ini, "foreground", foreground);
        ini.Write("font", fontName);
        ini.Write("fontsize", static_cast<long>(fontSize));
        ini.Write("capital", static_cast<long>(letterCase));
        ini.Write("fontstyle", static_cast<long>(fontStyle.flags()));
    }

    wxFileOutputStream outStream(m_themePath);
    ini.Save(outStream);
}
