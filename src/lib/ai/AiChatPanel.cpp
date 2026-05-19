//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiChatPanel.hpp"
#include <wx/filedlg.h>
#include "AiContext.hpp"
#include "AiManager.hpp"
#include "app/Context.hpp"
#include "chat/AiChatView.hpp"
using namespace fbide;

namespace {
// Re-render at most this often while a reply streams in, in milliseconds.
constexpr int kRenderThrottleMs = 150;
} // namespace

AiChatPanel::AiChatPanel(wxWindow* parent, Context& ctx)
: wxPanel(parent, wxID_ANY)
, m_ctx(ctx) {
    auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);

    m_output = make_unowned<AiChatView>(this, m_ctx);
    sizer->Add(m_output, wxSizerFlags(1).Expand().Border(wxALL, 4));

    // Context bar: list of attached files plus add/remove buttons.
    m_contextList = make_unowned<wxListBox>(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 56));
    m_addFile = make_unowned<wxButton>(this, wxID_ANY, m_ctx.tr("panels.aichat.addFile"));
    m_removeFile = make_unowned<wxButton>(this, wxID_ANY, m_ctx.tr("panels.aichat.removeFile"));

    auto contextButtons = make_unowned<wxBoxSizer>(wxVERTICAL);
    contextButtons->Add(m_addFile, wxSizerFlags().Expand());
    contextButtons->Add(m_removeFile, wxSizerFlags().Expand().Border(wxTOP, 2));

    auto contextRow = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    contextRow->Add(m_contextList, wxSizerFlags(1).Expand());
    contextRow->Add(contextButtons, wxSizerFlags().Border(wxLEFT, 4));
    sizer->Add(contextRow, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, 4));

    m_input = make_unowned<wxTextCtrl>(
        this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxSize(-1, 60),
        wxTE_MULTILINE
    );
    sizer->Add(m_input, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 4));

    m_send = make_unowned<wxButton>(this, wxID_ANY, m_ctx.tr("panels.aichat.send"));
    sizer->Add(m_send, wxSizerFlags().Right().Border(wxALL, 4));

    SetSizer(sizer);

    m_send->Bind(wxEVT_BUTTON, &AiChatPanel::onSend, this);
    m_addFile->Bind(wxEVT_BUTTON, &AiChatPanel::onAddFile, this);
    m_removeFile->Bind(wxEVT_BUTTON, &AiChatPanel::onRemoveFile, this);
    m_renderTimer.SetOwner(this);
    Bind(wxEVT_TIMER, &AiChatPanel::onRenderTimer, this);

    renderConversation();
}

void AiChatPanel::onSend(wxCommandEvent& /*event*/) {
    const auto text = m_input->GetValue();
    if (text.empty()) {
        return;
    }
    m_input->Clear();
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

void AiChatPanel::onAddFile(wxCommandEvent& /*event*/) {
    wxFileDialog dialog(
        this, m_ctx.tr("panels.aichat.addFile"), {}, {},
        "FreeBASIC sources (*.bas;*.bi)|*.bas;*.bi|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST
    );
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    wxArrayString paths;
    dialog.GetPaths(paths);
    auto& context = m_ctx.getAiManager().context();
    for (const auto& path : paths) {
        context.add(std::make_unique<FileContextItem>(path));
    }
    refreshContextList();
}

void AiChatPanel::onRemoveFile(wxCommandEvent& /*event*/) {
    const int selection = m_contextList->GetSelection();
    if (selection == wxNOT_FOUND) {
        return;
    }
    m_ctx.getAiManager().context().removeAt(static_cast<std::size_t>(selection));
    refreshContextList();
}

void AiChatPanel::refreshContextList() {
    m_contextList->Clear();
    for (const auto& item : m_ctx.getAiManager().context().items()) {
        m_contextList->Append(item->label());
    }
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
