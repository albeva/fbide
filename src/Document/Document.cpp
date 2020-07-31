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
