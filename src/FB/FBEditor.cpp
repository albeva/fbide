/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */
#include "FBEditor.hpp"
#include "App/Manager.hpp"
#include "UI/UiManager.hpp"
#include "Config/ConfigManager.hpp"
#include "Defaults.hpp"
using namespace fbide;

struct FBEditor::StyleEntry {
    wxFont font;
    #define STYLE_ENTRY(name, value) decltype(value) name;
    DEFAULT_EDITOR_STYLE(STYLE_ENTRY)
    #undef STYLE_ENTRY

    explicit StyleEntry(const Config& style, const StyleEntry* parent = nullptr) {
        if (parent != nullptr) {
            font = parent->font;
        } else {
            font = wxFont(
                style.Get(Defaults::Key::FontSize, Defaults::FontSize),
                wxFONTFAMILY_MODERN,
                wxFONTSTYLE_NORMAL,
                wxFONTWEIGHT_NORMAL,
                false,
                style.Get(Defaults::Key::FontName, wxEmptyString));
        }

        #define INIT_FIELD(NAME, DEF) NAME = style.Get(#NAME, parent != nullptr ? parent->NAME : DEF);
        DEFAULT_EDITOR_STYLE(INIT_FIELD)
        #undef INIT_FIELD
    }
};

const wxString FBEditor::TypeId = "text/freebasic"; // NOLINT

wxBEGIN_EVENT_TABLE(FBEditor, wxStyledTextCtrl) // NOLINT
    EVT_STC_CHARADDED(wxID_ANY, FBEditor::OnCharAdded)
wxEND_EVENT_TABLE()

FBEditor::FBEditor(const TypeManager::Type &type) : TextDocument(type) {}
FBEditor::~FBEditor() = default;

void FBEditor::CreateDocument() {
    TextDocument::CreateDocument();
    LoadFBLexer();
    SetLexerLanguage(TypeId);
    ILexerSdk *ilexer = this;
    PrivateLexerCall(SET_LEXER_IFACE, static_cast<void *>(ilexer));

    LoadConfiguration(GetConfig("Editor"));
    LoadTheme(GetCfgMgr().GetTheme());
}


/**
 * Load editor configuration
 */
void FBEditor::LoadConfiguration(const Config& config) {
    #define SET_CONFIG(name, ...) Set##name(config.Get(Defaults::Key::name, Defaults::name));
    DEFAULT_EDITOR_CONFIG(SET_CONFIG)
    #undef SET_CONFIG

    if (config.Get(Defaults::Key::ShowLineNumbers, Defaults::ShowLineNumbers)) {
        int LineNrMargin = TextWidth(wxSTC_STYLE_LINENUMBER, "00001");
        SetMarginWidth(0, LineNrMargin);
    } else {
        SetMarginWidth(0, 0);
    }
    SetMarginWidth(1, 0);
}

/**
 * Load editor theme
 */
void FBEditor::LoadTheme(const Config& theme) {
    const auto defStyle = StyleEntry(theme.GetOrEmpty("Default"), nullptr);
    LoadStyle(wxSTC_STYLE_DEFAULT, defStyle);
    StyleSetFont(wxSTC_STYLE_LINENUMBER, defStyle.font);

    #define SET_FB_STYLE(NAME) LoadStyle(static_cast<int>(FBStyle::NAME), StyleEntry(theme.GetOrEmpty(#NAME), &defStyle));
    FB_STYLE(SET_FB_STYLE)
    #undef SET_FB_STYLE
}

void FBEditor::LoadStyle(int nr, const StyleEntry& def) {
    StyleSetFont(nr, def.font);
    #define SET_STYLE(NAME, ...) StyleSet##NAME(nr, def.NAME);
    DEFAULT_EDITOR_STYLE(SET_STYLE)
    #undef SET_STYLE
}

void FBEditor::OnCharAdded(wxStyledTextEvent &event) {
    event.Skip();
}

// Load fblexer

void FBEditor::Log(const std::string &message) {
    LOG_VERBOSE(wxString(message));
}

bool FBEditor::s_fbLexerLoaded = false; // NOLINT

void FBEditor::LoadFBLexer() {
    if (s_fbLexerLoaded) {
        return;
    }

    #ifdef NDEBUG
        #define SUFFIX ""
    #else
        #define SUFFIX "d"
    #endif

    #if defined(__DARWIN__)
    const auto *dll = "libfblexer" SUFFIX ".dylib";
    #elif defined(__WXMSW__)
    const auto* dll = "fblexer" SUFFIX ".dll";
    #else
    const auto *dll = "libfblexer" SUFFIX ".so";
    #endif

    auto path = GetCfgMgr().ResolveResourcePath(dll);
    if (!wxFileExists(path)) {
        wxLogFatalError("Resource " + path + " not found"); // NOLINT
    }
    LoadLexerLibrary(path);
    s_fbLexerLoaded = true;

    LOG_VERBOSE("Loaded fblexer from " + path);
    LOG_VERBOSE(GetLibraryVersionInfo().ToString());
}
