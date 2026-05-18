//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiChatPanel.hpp"
#include <wx/html/htmlwin.h>
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
        wxTE_MULTILINE | wxTE_PROCESS_ENTER
    );
    sizer->Add(m_input, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, 4));

    m_send = make_unowned<wxButton>(this, wxID_ANY, m_ctx.tr("panels.aichat.send"));
    sizer->Add(m_send, wxSizerFlags().Right().Border(wxALL, 4));

    SetSizer(sizer);

    m_send->Bind(wxEVT_BUTTON, &AiChatPanel::onSend, this);
}

void AiChatPanel::onSend(wxCommandEvent& /*event*/) {
    const auto text = m_input->GetValue();
    if (text.empty()) {
        return;
    }
    // Phase 1 stub: echo the input. The real model round-trip lands in Phase 3.
    m_output->SetPage("<html><body><p>" + text + "</p></body></html>");
    m_input->Clear();
}
