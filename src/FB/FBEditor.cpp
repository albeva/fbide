//
// Created by Albert on 7/29/2020.
//
#include "FBEditor.hpp"
#include "App/Manager.hpp"
#include "UI/UiManager.hpp"
#include "UI/MainWindow.hpp"
#include "Config/ConfigManager.hpp"
using namespace fbide;

const wxString FBEditor::TypeId = "text/freebasic";

wxBEGIN_EVENT_TABLE(FBEditor, wxStyledTextCtrl)
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
}

void FBEditor::Log(const std::string &message) {
    wxLogMessage(wxString(message));
}

void FBEditor::OnCharAdded(wxStyledTextEvent &event) {
    event.Skip();
}

// Load fblexer
bool FBEditor::s_FBLExerLoaded = false;

void FBEditor::LoadFBLexer() {
    if (s_FBLExerLoaded) {
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
    s_FBLExerLoaded = true;
}
