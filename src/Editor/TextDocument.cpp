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
#include "TextDocument.hpp"
#include "UI/UiManager.hpp"
#include "Config/Config.hpp"
#include "App/Manager.hpp"
using namespace fbide;

const wxString TextDocument::TypeId = "text/plain"; // NOLINT

TextDocument::TextDocument(const TypeManager::Type& type)
: Document(type) {}

TextDocument::~TextDocument() = default;

/**
 * Instantiate the document
 */
void TextDocument::CreateDocument() {
    auto& ui = GetUiMgr();
    auto* da = ui.GetDocArea();
    #ifdef __WXMSW__
        auto* wnd = ui.GetWindow();
        wxWindowUpdateLocker locker(wnd);
    #endif
    wxStyledTextCtrl::Create(da);
    da->AddPage(this, GetDocumentTitle(), true);
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
