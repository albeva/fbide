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
#include "DocumentManager.hpp"
#include "App/Manager.hpp"
#include "UI/UiManager.hpp"
#include "TypeManager.hpp"
#include "Document.hpp"
using namespace fbide;

wxBEGIN_EVENT_TABLE(DocumentManager, wxEvtHandler) // NOLINT
    EVT_MENU(wxID_NEW,  DocumentManager::OnNew)
    EVT_MENU(wxID_OPEN, DocumentManager::OnOpen)
    EVT_MENU(wxID_SAVE, DocumentManager::OnSave)
wxEND_EVENT_TABLE()

DocumentManager::DocumentManager() {
    auto& uiMgr = GetUiMgr();
    auto *window = uiMgr.GetWindow();
    window->PushEventHandler(this);
}

DocumentManager::~DocumentManager() {
    auto& uiMgr = GetUiMgr();
    auto *window = uiMgr.GetWindow();
    window->RemoveEventHandler(this);
}


void DocumentManager::OnNew(wxCommandEvent&  /*event*/) {
    auto& type = GetTypeMgr();
    auto *doc = type.CreateFromType("default");
    doc->CreateDocument();
}

void DocumentManager::OnOpen(wxCommandEvent& /* event */) {
}

void DocumentManager::OnSave(wxCommandEvent& /* event */) {
}
