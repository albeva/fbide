//
//  Document.cpp
//  fbide
//
//  Created by Albert on 07/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "Document.hpp"
#include "Config/Config.hpp"
#include "App/Manager.hpp"

using namespace fbide;

static int uniqueId = 0; // NOLINT

Document::Document(const TypeManager::Type& type) : m_id(++uniqueId), m_type(type) {
    Document::SetDocumentTitle("");
}

Document::~Document() = default;

void Document::SetDocumentFileName(const wxString& filename) {
    m_filename = filename;
}

void Document::SetDocumentTitle(const wxString& title) {
    if (title.empty()) {
        m_title = GetLang("document.unnamed", { { "id", ""_wx << m_id } });
    }
}