//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiChatPanel.hpp"
#include <maddy/parser.h>
#include <sstream>
#include <wx/html/htmlwin.h>
#include "AiManager.hpp"
#include "app/Context.hpp"
using namespace fbide;

namespace {
// Re-render at most this often while a reply streams in, in milliseconds.
constexpr int kRenderThrottleMs = 150;
} // namespace

AiChatPanel::AiChatPanel(wxWindow* parent, Context& ctx)
: wxPanel(parent, wxID_ANY)
, m_ctx(ctx) {
    auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);

    m_output = make_unowned<wxHtmlWindow>(this, wxID_ANY);
    sizer->Add(m_output, wxSizerFlags(1).Expand().Border(wxALL, 4));

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
    m_renderTimer.SetOwner(this);
    Bind(wxEVT_TIMER, &AiChatPanel::onRenderTimer, this);

    renderConversation();
}

void AiChatPanel::onSend(wxCommandEvent& /*event*/) {
    if (m_busy) {
        return;
    }
    const auto text = m_input->GetValue();
    if (text.empty()) {
        return;
    }

    m_input->Clear();
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

void AiChatPanel::renderConversation() {
    const auto config = std::make_shared<maddy::ParserConfig>();
    maddy::Parser parser(config);

    const auto renderMarkdown = [&parser](const wxString& text) -> wxString {
        std::stringstream markdown(text.utf8_string());
        return wxString::FromUTF8(parser.Parse(markdown));
    };

    wxString html = "<html><body>";
    for (const auto& message : m_ctx.getAiManager().history()) {
        html += "<p><b>";
        html += message.role == AiRole::User ? "You" : "Assistant";
        html += "</b></p>";
        html += renderMarkdown(message.content);
        html += "<hr>";
    }
    if (m_busy) {
        // The streaming reply is not in the history yet — render the
        // partial text as it arrives.
        html += "<p><b>Assistant</b></p>";
        html += m_streaming.empty() ? wxString("<p><i>Thinking&hellip;</i></p>") : renderMarkdown(m_streaming);
    }
    if (!m_lastError.empty()) {
        html += "<p><font color=\"#cc0000\"><b>Error:</b> " + m_lastError + "</font></p>";
    }
    html += "</body></html>";

    m_output->SetPage(html);
    scrollToBottom();
}

void AiChatPanel::scrollToBottom() {
    m_output->Scroll(0, m_output->GetScrollRange(wxVERTICAL));
}
