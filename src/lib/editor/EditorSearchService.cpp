//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "EditorSearchService.hpp"
#include "Editor.hpp"
#include "app/Context.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

// clang-format off
wxBEGIN_EVENT_TABLE(EditorSearchService, wxEvtHandler)
    EVT_FIND(wxID_ANY,             EditorSearchService::onFindDialog)
    EVT_FIND_NEXT(wxID_ANY,        EditorSearchService::onFindDialogNext)
    EVT_FIND_REPLACE(wxID_ANY,     EditorSearchService::onReplaceDialog)
    EVT_FIND_REPLACE_ALL(wxID_ANY, EditorSearchService::onReplaceAllDialog)
    EVT_FIND_CLOSE(wxID_ANY,       EditorSearchService::onFindDialogClose)
wxEND_EVENT_TABLE()
// clang-format on

EditorSearchService::EditorSearchService(Context& ctx)
: m_ctx(ctx) {}

void EditorSearchService::showFind() {
    showFindDialog(false);
}

void EditorSearchService::showReplace() {
    showFindDialog(true);
}

void EditorSearchService::findNext() {
    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }
    const auto text = m_findData.GetFindString();
    if (text.empty()) {
        showFindDialog(false);
        return;
    }
    const bool forward = (m_findData.GetFlags() & wxFR_DOWN) != 0;
    doc->getEditor()->findNext(text, m_findData.GetFlags(), forward);
}

void EditorSearchService::gotoLine() {
    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }
    const auto input = wxGetTextFromUser(
        m_ctx.tr("dialogs.goto.prompt"),
        m_ctx.tr("dialogs.goto.title"),
        wxEmptyString,
        m_ctx.getUIManager().getMainFrame()
    );
    if (!input.empty()) {
        doc->getEditor()->gotoLine(input);
    }
}

void EditorSearchService::showFindDialog(const bool replace) {
    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }

    // Single instance: raise the open dialog instead of stacking another.
    // Find and Replace share the one slot — close it to switch modes.
    if (m_findDialog != nullptr) {
        m_findDialog->Raise();
        m_findDialog->SetFocus();
        return;
    }

    // Pre-fill with selection or word under cursor.
    if (const auto word = doc->getEditor()->getWordAtCursor(); !word.empty()) {
        m_findData.SetFindString(word);
    }

    auto* frame = m_ctx.getUIManager().getMainFrame();
    const int style = replace ? wxFR_REPLACEDIALOG : 0;
    const auto title = replace ? m_ctx.tr("dialogs.replace.title")
                               : m_ctx.tr("dialogs.find.title");

    m_findDialog = make_unowned<wxFindReplaceDialog>(frame, &m_findData, title, style);
    m_findDialog->PushEventHandler(this);
    m_findDialog->Show();
}

void EditorSearchService::onFindDialog(wxFindDialogEvent& event) {
    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }
    const bool forward = (event.GetFlags() & wxFR_DOWN) != 0;
    doc->getEditor()->findNext(event.GetFindString(), event.GetFlags(), forward);
}

void EditorSearchService::onFindDialogNext(wxFindDialogEvent& event) {
    onFindDialog(event);
}

void EditorSearchService::onReplaceDialog(wxFindDialogEvent& event) {
    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }
    doc->getEditor()->replaceNext(event.GetFindString(), event.GetReplaceString(), event.GetFlags());
}

void EditorSearchService::onReplaceAllDialog(wxFindDialogEvent& event) {
    auto* doc = m_ctx.getDocumentManager().getActive();
    if (doc == nullptr) {
        return;
    }
    doc->getEditor()->replaceAll(event.GetFindString(), event.GetReplaceString(), event.GetFlags());
}

void EditorSearchService::onFindDialogClose(wxFindDialogEvent& event) {
    if (auto* dlg = event.GetDialog()) {
        dlg->PopEventHandler();
        dlg->Destroy();
    }
    m_findDialog = nullptr;
}
