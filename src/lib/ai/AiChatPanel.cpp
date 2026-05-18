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
    m_busy = true;
    m_send->Disable();

    m_ctx.getAiManager().sendMessage(text, [this](const AiResponse& response) {
        m_busy = false;
        m_send->Enable();
        if (!response.ok) {
            m_lastError = response.error;
        }
        renderConversation();
    });

    // Show the user message (already appended to the history) and the
    // busy indicator while the reply is in flight.
    renderConversation();
}

void AiChatPanel::renderConversation() {
    const auto config = std::make_shared<maddy::ParserConfig>();
    maddy::Parser parser(config);

    wxString html = "<html><body>";
    for (const auto& message : m_ctx.getAiManager().history()) {
        std::stringstream markdown(message.content.utf8_string());
        html += "<p><b>";
        html += message.role == AiRole::User ? "You" : "Assistant";
        html += "</b></p>";
        html += wxString::FromUTF8(parser.Parse(markdown));
        html += "<hr>";
    }
    if (m_busy) {
        html += "<p><i>Thinking…</i></p>";
    }
    if (!m_lastError.empty()) {
        html += "<p><font color=\"#cc0000\"><b>Error:</b> " + m_lastError + "</font></p>";
    }
    html += "</body></html>";

    m_output->SetPage(html);
}
