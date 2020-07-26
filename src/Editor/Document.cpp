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

namespace {
int uniqueId = 0;
}

Document::Document(const TypeManager::Type& type) : m_id(++uniqueId), m_type(type) {
    SetTitle("");
}

Document::~Document() = default;

void Document::SetFilename(const wxString& filename) {
    m_filename = filename;
}

void Document::SetTitle(const wxString& title) {
    if (title.empty()) {
        m_title = GetLang("document.unnamed", { { "id", ""_wx << m_id } });
    }
}
