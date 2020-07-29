//
// Created by Albert on 7/29/2020.
//
#include "FBEditor.hpp"
using namespace fbide;

const wxString FBEditor::TypeId = "text/freebasic";

FBEditor::FBEditor(const TypeManager::Type &type) : EditorDocument(type) {}

FBEditor::~FBEditor() = default;

void FBEditor::Create() {
    EditorDocument::Create();
    ILexerSdk* ilexer = this;
    GetEditor().PrivateLexerCall(SET_LEXER_IFACE, static_cast<void*>(ilexer));
}

void FBEditor::Log(const std::string &message) {
    wxLogMessage(wxString(message));
}
