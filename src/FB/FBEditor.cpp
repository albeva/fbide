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
using namespace fbide;

const wxString FBEditor::TypeId = "text/freebasic"; // NOLINT
constexpr int DefaultFontSize = 12;

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
    // load generic configuration
}

/**
 * Load editor theme
 */
void FBEditor::LoadTheme(const Config& theme) {
    wxFont font(
        theme.Get("FontSize", DefaultFontSize),
        wxFONTFAMILY_MODERN,
        wxFONTSTYLE_NORMAL,
        wxFONTWEIGHT_NORMAL,
        false,
        theme.Get("FontName", "Courier New"));

    StyleSetFont(wxSTC_STYLE_DEFAULT, font);
    StyleSetFont(wxSTC_STYLE_LINENUMBER, font);
}

void FBEditor::Log(const std::string &message) {
    wxLogMessage(wxString(message)); // NOLINT
}

void FBEditor::OnCharAdded(wxStyledTextEvent &event) {
    event.Skip();
}

// Load fblexer
bool FBEditor::s_fbLexerLoaded = false; // NOLINT

void FBEditor::LoadFBLexer() {
    if (s_fbLexerLoaded) {
        return;
    }

    #if defined(__DARWIN__)
    auto path = GetConfig(Key::IdePath).AsString() / "libfblexer.dylib";
    #elif defined(__WXMSW__)
    auto path = GetConfig(Key::IdePath).AsString() / "fblexer.dll";
    #else
    auto path = GetConfig(Key::IdePath).AsString() / "libfblexer.so";
    #endif

    LoadLexerLibrary(path);
    s_fbLexerLoaded = true;
}
