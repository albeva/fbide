//
//  Document.cpp
//  fbide
//
//  Created by Albert on 07/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "app_pch.hpp"
#include "Document.hpp"

using namespace fbide;


namespace {
    
    int uniqueId{0};
    
}


Document::Document() : m_id(++uniqueId)
{
    SetTitle("");
}


Document::~Document()
{
}


void Document::SetFilename(const wxString &filename)
{
    m_filename = filename;
}


void Document::SetTitle(const wxString &title)
{
    if (title.empty()) {
        m_title = GetLang("document.unnamed", {{"id", ""_wx << m_id}});
    }
}