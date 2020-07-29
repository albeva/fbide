//
//  Editor.cpp
//  fbide
//
//  Created by Albert on 09/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "TextDocument.hpp"
#include "UI/MainWindow.hpp"
#include "UI/UiManager.hpp"
#include "Config/Config.hpp"
#include "App/Manager.hpp"
using namespace fbide;

const wxString TextDocument::TypeId = "text/plain";

TextDocument::TextDocument(const TypeManager::Type& type)
: Document(type) {}

TextDocument::~TextDocument() = default;

/**
 * Instantiate the document
 */
void TextDocument::CreateDocument() {
    auto& ui = GetUiMgr();
    auto da = ui.GetDocArea();

    wxWindowUpdateLocker lock(ui.GetWindow());

    wxStyledTextCtrl::Create(da);
    da->AddPage(this, GetDocumentTitle(), true);

    // editor configuration
    auto & config = GetDocumentType().config;
    auto type = config.Get("type", "null");
    SetLexerLanguage(type);
}

/**
 * LoadDocument specified file. Will CreateDocument the instance
 */
void TextDocument::LoadDocument(const wxString& filename) {
    LoadFile(filename);
}

/**
 * SaveDocument the document
 */
void TextDocument::SaveDocument(const wxString& filename) {
    SaveFile(filename);
}
