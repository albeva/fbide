//
// Created by Albert on 7/29/2020.
//
#pragma once
#include "app_pch.hpp"
#include "Editor/TextDocument.hpp"
#include "LexerSdk.hpp"

namespace fbide {

class FBEditor final: public TextDocument, public ILexerSdk {
    NON_COPYABLE(FBEditor)
public:
    // Editor mime type
    static const wxString TypeId;

    FBEditor(const TypeManager::Type& type);
    virtual ~FBEditor();
    void CreateDocument() final;

    // fblexer communication
    void Log(const std::string& message) final;

private:
    void OnCharAdded(wxStyledTextEvent &event);

    static bool s_FBLExerLoaded;
    static void LoadFBLexer();

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
