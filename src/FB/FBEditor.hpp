//
// Created by Albert on 7/29/2020.
//
#pragma once
#include "app_pch.hpp"
#include "Editor/EditorDocument.hpp"
#include "LexerSdk.hpp"

namespace fbide {

class FBEditor final: public EditorDocument, public ILexerSdk {
    NON_COPYABLE(FBEditor)
public:
    // Editor mime type
    static const wxString TypeId;

    FBEditor(const TypeManager::Type& type);
    virtual ~FBEditor();
    void Create() final;

    // fblexer communication
    void Log(const std::string& message) final;
};

} // namespace fbide
