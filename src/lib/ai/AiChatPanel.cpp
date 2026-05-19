//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiChatPanel.hpp"
#include <wx/filedlg.h>
#include <wx/menu.h>
#include "AiContext.hpp"
#include "AiManager.hpp"
#include "ContextTagBar.hpp"
#include "analyses/symbols/SymbolTable.hpp"
#include "app/Context.hpp"
#include "chat/AiChatView.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
using namespace fbide;

namespace {
// Re-render at most this often while a reply streams in, in milliseconds.
constexpr int kRenderThrottleMs = 150;
// Input box auto-grow bounds, in text lines, plus its vertical padding.
constexpr int kMinInputLines = 2;
constexpr int kMaxInputLines = 8;
constexpr int kInputPadding = 10;

// Attach-menu command IDs. The tab / include entries are numbered from a
// base so the chosen index can be recovered from the selection.
enum AttachMenuId : int {
    ID_AttachActive = wxID_HIGHEST + 1,
    ID_AttachBrowse,
    ID_AttachTabBase = wxID_HIGHEST + 100,
    ID_AttachIncludeBase = wxID_HIGHEST + 1000,
};

/// The active document's `#include`s, resolved to existing files. Paths are
/// resolved relative to the document's own folder; compiler / system-dir
/// includes that do not resolve there are dropped.
auto activeIncludes(Document* doc) -> std::vector<wxString> {
    std::vector<wxString> result;
    if (doc == nullptr) {
        return result;
    }
    const auto table = doc->getSymbolTable();
    if (!table) {
        return result;
    }
    const wxString baseDir = wxFileName(doc->getFilePath()).GetPath();
    for (const auto& include : table->getIncludes()) {
        wxFileName file(include.path);
        if (!file.IsAbsolute()) {
            if (baseDir.empty()) {
                continue; // untitled host file — nothing to resolve against
            }
            file.MakeAbsolute(baseDir);
        }
        if (file.FileExists()) {
            result.push_back(file.GetFullPath());
        }
    }
    return result;
}
} // namespace

AiChatPanel::AiChatPanel(wxWindow* parent, Context& ctx)
: wxPanel(parent, wxID_ANY)
, m_ctx(ctx) {
    auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);

    m_output = make_unowned<AiChatView>(this, m_ctx);
    sizer->Add(m_output, wxSizerFlags(1).Expand().Border(wxALL, 4));

    // Tag strip — sits between the conversation and the input, hidden when
    // nothing is attached.
    m_tagBar = make_unowned<ContextTagBar>(this, m_ctx);
    sizer->Add(m_tagBar, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 4));

    m_input = make_unowned<wxTextCtrl>(
        this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE
    );
    sizer->Add(m_input, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, 4));

    // Bottom row: the attach button on the left, send on the right.
    m_addContext = make_unowned<wxButton>(
        this, wxID_ANY, "+", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT
    );
    m_addContext->SetToolTip("Attach context");
    m_send = make_unowned<wxButton>(this, wxID_ANY, m_ctx.tr("panels.aichat.send"));

    auto row = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    row->Add(m_addContext, wxSizerFlags().Centre());
    row->AddStretchSpacer(1);
    row->Add(m_send, wxSizerFlags().Centre());
    sizer->Add(row, wxSizerFlags().Expand().Border(wxALL, 4));

    SetSizer(sizer);

    m_send->Bind(wxEVT_BUTTON, &AiChatPanel::onSend, this);
    m_addContext->Bind(wxEVT_BUTTON, &AiChatPanel::onAddContext, this);
    m_input->Bind(wxEVT_TEXT, &AiChatPanel::onInputText, this);
    Bind(EVT_CONTEXT_TAGS_CHANGED, &AiChatPanel::onTagsChanged, this);
    m_renderTimer.SetOwner(this);
    Bind(wxEVT_TIMER, &AiChatPanel::onRenderTimer, this);

    autoSizeInput();
    renderConversation();
}

void AiChatPanel::onSend(wxCommandEvent& /*event*/) {
    const auto text = m_input->GetValue();
    if (text.empty()) {
        return;
    }
    m_input->Clear();
    autoSizeInput();
    submitPrompt(text);
}

void AiChatPanel::submitPrompt(const wxString& text) {
    if (m_busy || text.empty()) {
        return;
    }

    m_lastError.clear();
    m_streaming.clear();
    m_busy = true;
    m_dirty = false;
    m_send->Disable();
    m_renderTimer.Start(kRenderThrottleMs);

    m_ctx.getAiManager().sendMessage(
        text,
        [this](const wxString& delta) {
            // Accumulate; the throttle timer drives the actual re-render.
            m_streaming += delta;
            m_dirty = true;
        },
        [this](AiResponse response) {
            m_renderTimer.Stop();
            m_busy = false;
            m_dirty = false;
            m_send->Enable();
            if (!response.ok) {
                m_lastError = response.error;
            }
            // The reply now lives in the history — drop the partial copy.
            m_streaming.clear();
            renderConversation();
        }
    );

    // Show the user message (already in the history) and the busy state.
    renderConversation();
}

void AiChatPanel::onRenderTimer(wxTimerEvent& /*event*/) {
    if (m_dirty) {
        m_dirty = false;
        renderConversation();
    }
}

void AiChatPanel::onInputText(wxCommandEvent& event) {
    autoSizeInput();
    event.Skip();
}

void AiChatPanel::autoSizeInput() {
    const int lines = std::clamp(m_input->GetNumberOfLines(), kMinInputLines, kMaxInputLines);
    const int height = (lines * m_input->GetCharHeight()) + kInputPadding;
    if (m_input->GetMinHeight() != height) {
        m_input->SetMinSize(wxSize(-1, height));
        Layout();
    }
}

void AiChatPanel::onAddContext(wxCommandEvent& /*event*/) {
    auto& documents = m_ctx.getDocumentManager();
    Document* const active = documents.getActive();

    // Collect the choices up front — the menu item IDs index into these.
    std::vector<Document*> otherTabs;
    for (const auto& document : documents.getDocuments()) {
        if (document.get() != active) {
            otherTabs.push_back(document.get());
        }
    }
    const std::vector<wxString> includes = activeIncludes(active);

    wxMenu menu;
    menu.Append(ID_AttachActive, "Active tab")->Enable(active != nullptr);

    if (!otherTabs.empty()) {
        auto subMenu = make_unowned<wxMenu>();
        for (std::size_t i = 0; i < otherTabs.size(); i++) {
            subMenu->Append(ID_AttachTabBase + static_cast<int>(i), otherTabs[i]->getTitle());
        }
        menu.AppendSubMenu(subMenu, "Other tabs");
    }
    if (!includes.empty()) {
        auto subMenu = make_unowned<wxMenu>();
        for (std::size_t i = 0; i < includes.size(); i++) {
            subMenu->Append(
                ID_AttachIncludeBase + static_cast<int>(i), wxFileName(includes[i]).GetFullName()
            );
        }
        menu.AppendSubMenu(subMenu, "Includes");
    }
    menu.AppendSeparator();
    menu.Append(ID_AttachBrowse, "Browse files…");

    const int selection = GetPopupMenuSelectionFromUser(menu);
    if (selection == wxID_NONE) {
        return;
    }

    auto& context = m_ctx.getAiManager().context();
    if (selection == ID_AttachActive) {
        attachDocument(active);
    } else if (selection == ID_AttachBrowse) {
        wxFileDialog dialog(
            this, "Attach files", {}, {},
            "FreeBASIC sources (*.bas;*.bi)|*.bas;*.bi|All files (*.*)|*.*",
            wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST
        );
        if (dialog.ShowModal() == wxID_OK) {
            wxArrayString paths;
            dialog.GetPaths(paths);
            for (const auto& path : paths) {
                context.add(std::make_unique<FileContextItem>(path));
            }
        }
    } else if (selection >= ID_AttachTabBase && selection < ID_AttachIncludeBase) {
        attachDocument(otherTabs.at(static_cast<std::size_t>(selection - ID_AttachTabBase)));
    } else if (selection >= ID_AttachIncludeBase) {
        const auto index = static_cast<std::size_t>(selection - ID_AttachIncludeBase);
        context.add(std::make_unique<FileContextItem>(includes.at(index)));
    }

    m_tagBar->refresh();
    Layout();
}

void AiChatPanel::attachDocument(Document* doc) {
    if (doc == nullptr) {
        return;
    }
    // Snapshot the editor's current text — including any unsaved edits.
    m_ctx.getAiManager().context().add(
        std::make_unique<BufferContextItem>(doc->getTitle(), doc->getEditor()->GetText())
    );
}

void AiChatPanel::onTagsChanged(wxCommandEvent& /*event*/) {
    Layout(); // the tag bar shrank or hid — re-flow the panel
}

void AiChatPanel::renderConversation() {
    std::vector<ChatViewMessage> messages;
    for (const auto& message : m_ctx.getAiManager().history()) {
        messages.push_back({
            .fromUser = message.role == AiRole::User,
            .markdown = message.content,
        });
    }
    if (m_busy) {
        // The streaming reply is not in the history yet — show the partial
        // text as it arrives, or a placeholder until the first chunk.
        messages.push_back({
            .fromUser = false,
            .markdown = m_streaming.empty() ? wxString("_Thinking…_") : m_streaming,
        });
    }
    if (!m_lastError.empty()) {
        messages.push_back({
            .fromUser = false,
            .markdown = "**Error:** " + m_lastError,
        });
    }
    m_output->setMessages(std::move(messages));
}
