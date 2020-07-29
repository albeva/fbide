//
// Created by Albert on 7/29/2020.
//
#include "FBEditor.hpp"
using namespace fbide;

const wxString FBEditor::TypeId = "text/freebasic";

wxBEGIN_EVENT_TABLE(FBEditor, wxStyledTextCtrl)
    EVT_STC_CHARADDED(wxID_ANY, FBEditor::OnCharAdded)
wxEND_EVENT_TABLE()

FBEditor::FBEditor(const TypeManager::Type &type) : TextDocument(type) {}
FBEditor::~FBEditor() = default;

void FBEditor::CreateDocument() {
    TextDocument::CreateDocument();
    ILexerSdk* ilexer = this;
    PrivateLexerCall(SET_LEXER_IFACE, static_cast<void*>(ilexer));
}

void FBEditor::Log(const std::string &message) {
    wxLogMessage(wxString(message));
}

void FBEditor::OnCharAdded(wxStyledTextEvent &event) {
    event.Skip();
}
