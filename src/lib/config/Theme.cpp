//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Theme.hpp"
#include "Version.hpp"
using namespace fbide;

namespace {

// ---------------------------------------------------------------------------
// Read/write basic value type
// ---------------------------------------------------------------------------
template<typename T>
[[nodiscard]] auto read(wxFileConfig& ini, const wxString& key) -> T {
    T value;
    if (ini.Read(key, &value)) {
        return value;
    }
    return T();
}

template<typename T>
void write(wxFileConfig& ini, const wxString& key, const T& value) {
    ini.Write(key, value);
}

// ---------------------------------------------------------------------------
// Read/write Version
// ---------------------------------------------------------------------------
template<>
[[nodiscard]] auto read<Version>(wxFileConfig& ini, const wxString& key) -> Version {
    wxString value;
    if (ini.Read(key, &value)) {
        return Version(value);
    }
    return Version();
}

template<>
void write<Version>(wxFileConfig& ini, const wxString& key, const Version& value) {
    ini.Write(key, value.asString());
}

// ---------------------------------------------------------------------------
// Read/write wxColour
// ---------------------------------------------------------------------------
template<>
[[nodiscard]] auto read<wxColour>(wxFileConfig& ini, const wxString& key) -> wxColour {
    wxString value;
    if (ini.Read(key, &value)) {
        wxColour col(value);
        if (col.IsOk() || col == wxNullColour) {
            return col;
        }
        wxLogWarning("Invalid colour '%s' for key '%s' in '%s'", value, key, ini.GetPath());
    }
    return wxNullColour;
}

template<>
void write<wxColour>(wxFileConfig& ini, const wxString& key, const wxColour& value) {
    if (not value.IsOk()) {
        if (value != wxNullColour) {
            wxLogError("Invalid color '%s' for key '%s' in '%s'", value.GetAsString(), key, ini.GetPath());
        }
        return;
    }
    ini.Write(key, value.GetAsString(wxC2S_HTML_SYNTAX));
}

// ---------------------------------------------------------------------------
// Read/write Colors
// ---------------------------------------------------------------------------

template<>
[[nodiscard]] auto read<Theme::Colors>(wxFileConfig& ini, const wxString& path) -> Theme::Colors {
    const wxConfigPathChanger restore { &ini, path };
    return {
        .foreground = read<wxColour>(ini, "foreground"),
        .background = read<wxColour>(ini, "background")
    };
}

template<>
void write<Theme::Colors>(wxFileConfig& ini, const wxString& path, const Theme::Colors& value) {
    const wxConfigPathChanger restore { &ini, path };
    write(ini, "foreground", value.foreground);
    write(ini, "background", value.background);
}

// ---------------------------------------------------------------------------
// Read/write Entry
// ---------------------------------------------------------------------------

template<>
[[nodiscard]] auto read<Theme::Entry>(wxFileConfig& ini, const wxString& path) -> Theme::Entry {
    const wxConfigPathChanger restore { &ini, path };
    return {
        .colors = read<Theme::Colors>(ini, wxEmptyString),
        .bold = read<bool>(ini, "bold"),
        .italic = read<bool>(ini, "italic"),
        .underlined = read<bool>(ini, "underlined"),
    };
}

template<>
void write<Theme::Entry>(wxFileConfig& ini, const wxString& path, const Theme::Entry& value) {
    const wxConfigPathChanger restore { &ini, path };
    write(ini, wxEmptyString, value.colors);
    write(ini, "bold", value.bold);
    write(ini, "italic", value.italic);
    write(ini, "underlined", value.underlined);
}

// ---------------------------------------------------------------------------
// Legacy v4 helpers — BGR ints, flag-based fontstyle
// ---------------------------------------------------------------------------
auto colorFromBgr(const long bgr) -> wxColour {
    return {
        static_cast<unsigned char>((bgr >> 16) & 0xFF),
        static_cast<unsigned char>((bgr >> 8) & 0xFF),
        static_cast<unsigned char>(bgr & 0xFF)
    };
}

auto colorToBgrLong(const wxColour& c) -> long {
    return (static_cast<long>(c.Red()) << 16)
         | (static_cast<long>(c.Green()) << 8)
         | static_cast<long>(c.Blue());
}

auto readBgrColor(const wxFileConfig& ini, const wxString& key, const wxColour& fallback) -> wxColour {
    return colorFromBgr(ini.ReadLong(key, colorToBgrLong(fallback)));
}

auto readLegacyFontFlags(const wxFileConfig& ini) -> int {
    long val = ini.ReadLong("fontstyle", -1);
    if (val == -1) {
        val = ini.ReadLong("fonstyle", 0); // classic.fbt typo
    }
    return static_cast<int>(val);
}

auto readLegacyEntry(wxFileConfig& ini, const wxString& section,
                     const wxColour& defBg, const wxColour& defFg) -> Theme::Entry {
    // Trailing slash is required — wxConfigPathChanger otherwise strips the
    // last component, treating it as an entry name instead of a group.
    const wxConfigPathChanger restore { &ini, section + "/" };
    const int flags = readLegacyFontFlags(ini);
    return {
        .colors = {
            .foreground = readBgrColor(ini, "foreground", defFg),
            .background = readBgrColor(ini, "background", defBg),
        },
        .bold = (flags & 1) != 0,
        .italic = (flags & 2) != 0,
        .underlined = (flags & 4) != 0,
    };
}

auto readLegacyColors(wxFileConfig& ini, const wxString& section,
                      const wxColour& defBg, const wxColour& defFg) -> Theme::Colors {
    const wxConfigPathChanger restore { &ini, section + "/" };
    return {
        .foreground = readBgrColor(ini, "foreground", defFg),
        .background = readBgrColor(ini, "background", defBg),
    };
}

} // namespace

/// --------------------------------------------------------------------------
/// Init
/// --------------------------------------------------------------------------

Theme::Theme(const wxString& themePath)
: m_themePath(themePath) {
    load(wxEmptyString, false);
}

void Theme::load(const wxString& themePath, const bool reset) {
    if (reset) {
        *this = {};
    }

    if (not themePath.IsEmpty()) {
        m_themePath = themePath;
    }

    // If the requested file doesn't exist, try swapping the extension
    // between .ini and .fbt — keeps callers unaware of format versions.
    if (not wxFileExists(m_themePath)) {
        wxFileName fn(m_themePath);
        const auto ext = fn.GetExt().Lower();
        if (ext == "ini") {
            fn.SetExt("fbt");
        } else if (ext == "fbt") {
            fn.SetExt("ini");
        }
        if (wxFileExists(fn.GetFullPath())) {
            m_themePath = fn.GetFullPath();
        }
    }

    // Legacy v4 format — .fbt extension.
    if (m_themePath.Lower().EndsWith(".fbt")) {
        const wxString legacyPath = m_themePath;
        loadV4(legacyPath);
        m_themePath = legacyPath;
        return;
    }

    wxFFileInputStream stream(m_themePath);
    if (!stream.IsOk()) {
        wxLogError("Failed to load theme from '%s'", m_themePath);
        return;
    }
    wxFileConfig ini(stream);

    // Load properties

    // clang-format off
    #define LOAD(NAME, GETTER, TYPE) m_## NAME = read<TYPE>(ini, #NAME);
        DEFINE_THEME_PROPERTY(LOAD)
    #undef LOAD
    // clang-format on

    // Load categories
    for (const auto& cat : kThemeCategories) {
        ini.SetPath("/" + wxString(getThemeCategoryName(cat)));
        m_categories[static_cast<std::size_t>(cat)] = read<Entry>(ini, wxEmptyString);
    }
}

void Theme::loadV4(const wxString& themePath) {
    *this = {};

    // set explicitly old version
    m_version = Version::oldFbide();

    wxFFileInputStream stream(themePath);
    if (!stream.IsOk()) {
        wxLogError("Failed to load legacy theme from '%s'", themePath);
        return;
    }
    wxFileConfig ini(stream);

    // [default] — global font + default colours
    wxColour defaultBg;
    wxColour defaultFg;
    {
        const wxConfigPathChanger restore { &ini, "/default/" };
        defaultBg = readBgrColor(ini, "background", *wxWHITE);
        defaultFg = readBgrColor(ini, "foreground", *wxBLACK);
        m_font = ini.Read("font", wxEmptyString);
        m_fontSize = static_cast<int>(ini.ReadLong("fontsize", 12));
    }

    // Old themes often leave [default].font empty; fall back to any syntax font
    if (m_font.IsEmpty()) {
        for (const auto* section : { "/comment/", "/keyword/", "/string/", "/identifier/" }) {
            const wxConfigPathChanger restore { &ini, section };
            m_font = ini.Read("font", wxEmptyString);
            if (not m_font.IsEmpty()) {
                break;
            }
        }
    }
    if (m_font.IsEmpty()) {
        m_font = "Courier New";
    }

    // Editor-wide entries
    m_lineNumber = readLegacyColors(ini, "/linenumber", *wxWHITE, wxColour(0xC0, 0xC0, 0xC0));
    m_selection = readLegacyColors(ini, "/select", *wxWHITE, wxColour(0xC0, 0xC0, 0xC0));
    m_brace = readLegacyEntry(ini, "/brace", defaultBg, defaultFg);
    m_badBrace = readLegacyEntry(ini, "/badbrace", *wxBLACK, defaultFg);

    // Category mapping — mirrors Editor::applyFreebasicTheme fallbacks
    const auto mapCategory = [&](const ThemeCategory cat, const wxString& section) {
        m_categories[static_cast<std::size_t>(cat)] = readLegacyEntry(ini, section, defaultBg, defaultFg);
    };

    // Default syntax style = editor defaults (no flags)
    m_categories[static_cast<std::size_t>(ThemeCategory::Default)] = {
        .colors = { .foreground = defaultFg, .background = defaultBg },
    };

    mapCategory(ThemeCategory::Comment,          "/comment");
    mapCategory(ThemeCategory::MultilineComment, "/comment");
    mapCategory(ThemeCategory::Number,           "/number");
    mapCategory(ThemeCategory::String,           "/string");
    mapCategory(ThemeCategory::StringOpen,       "/stringeol");
    mapCategory(ThemeCategory::Identifier,       "/identifier");
    mapCategory(ThemeCategory::Keyword1,         "/keyword");
    mapCategory(ThemeCategory::Keyword2,         "/keyword2");
    mapCategory(ThemeCategory::Keyword3,         "/keyword3");
    mapCategory(ThemeCategory::Keyword4,         "/keyword4");
    mapCategory(ThemeCategory::Keyword5,         "/keyword");
    mapCategory(ThemeCategory::Operator,         "/operator");
    mapCategory(ThemeCategory::Label,            "/identifier");
    mapCategory(ThemeCategory::Constant,         "/constant");
    mapCategory(ThemeCategory::Preprocessor,     "/preprocessor");
    mapCategory(ThemeCategory::Error,            "/identifier");
}

void Theme::save(const wxString& newThemePath) {
    if (not newThemePath.IsEmpty()) {
        m_themePath = newThemePath;
    }
    if (m_themePath.IsEmpty()) {
        wxLogError("Missing filename when saving theme");
        return;
    }

    // Migrate legacy (.fbt) paths to .ini on first save.
    if (m_version < Version::fbide()) {
        wxFileName fn(m_themePath);
        if (fn.GetExt().Lower() == "fbt") {
            fn.SetExt("ini");
            wxLogMessage("Migrating theme '%s' to new format at '%s'", m_themePath, fn.GetFullPath());
            m_themePath = fn.GetFullPath();
        }
    }
    m_version = Version::fbide();

    wxFileConfig ini { "", "", "", "", wxCONFIG_USE_LOCAL_FILE };

    // patch version
    if (not m_version.isValid() || m_version == Version::oldFbide()) {
        m_version = Version::fbide();
    }

    // clang-format off
    #define STORE(NAME, GETTER, TYPE) write(ini, #NAME, m_## NAME);
        DEFINE_THEME_PROPERTY(STORE)
    #undef STORE
    // clang-format on

    for (const auto& cat : kThemeCategories) {
        ini.SetPath("/" + wxString(getThemeCategoryName(cat)));
        write(ini, wxEmptyString, m_categories[static_cast<std::size_t>(cat)]);
    }

    // save
    wxFileOutputStream outStream(m_themePath);
    ini.Save(outStream);
}
