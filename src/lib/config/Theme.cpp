//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Theme.hpp"
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

void Theme::save(const wxString& newThemePath) {
    if (not newThemePath.IsEmpty()) {
        m_themePath = newThemePath;
    }
    if (m_themePath.IsEmpty()) {
        wxLogError("Missing filename when saving theme");
        return;
    }
    wxFileConfig ini { "", "", "", "", wxCONFIG_USE_LOCAL_FILE };

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
