//
// Created by Albert on 7/29/2020.
//
#include "DocumentManager.hpp"
#include "App/Manager.hpp"
#include "UI/UiManager.hpp"
#include "UI/MainWindow.hpp"
#include "TypeManager.hpp"
#include "Document.hpp"
using namespace fbide;

wxBEGIN_EVENT_TABLE(DocumentManager, wxEvtHandler)
    EVT_MENU(wxID_NEW,  DocumentManager::OnNew)
    EVT_MENU(wxID_OPEN, DocumentManager::OnOpen)
    EVT_MENU(wxID_SAVE, DocumentManager::OnSave)
wxEND_EVENT_TABLE()

DocumentManager::DocumentManager() {
    auto& uiMgr = GetUiMgr();
    auto window = uiMgr.GetWindow();
    window->PushEventHandler(this);
}

DocumentManager::~DocumentManager() {
    auto& uiMgr = GetUiMgr();
    auto window = uiMgr.GetWindow();
    window->RemoveEventHandler(this);
}


void DocumentManager::OnNew(wxCommandEvent& event) {
    auto& uiMgr = GetUiMgr();
    auto window = uiMgr.GetWindow();
    auto& type = GetTypeMgr();

    auto doc = type.CreateFromType("default");
    doc->CreateDocument();
}

void DocumentManager::OnOpen(wxCommandEvent& event) {
}

void DocumentManager::OnSave(wxCommandEvent& event) {
}
