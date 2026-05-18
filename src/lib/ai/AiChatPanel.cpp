//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiChatPanel.hpp"
#include <maddy/parser.h>
#include <sstream>
#include <wx/filedlg.h>
#include <wx/html/htmlwin.h>
#include "AiContext.hpp"
#include "AiManager.hpp"
#include "app/Context.hpp"
using namespace fbide;

namespace {
// Re-render at most this often while a reply streams in, in milliseconds.
constexpr int kRenderThrottleMs = 150;

/// Escape the HTML metacharacters in plain text.
auto escapeHtml(const wxString& text) -> wxString {
    wxString out;
    out.reserve(text.size());
    for (const auto ch : text) {
        if (ch == '&') {
            out += "&amp;";
        } else if (ch == '<') {
            out += "&lt;";
        } else if (ch == '>') {
            out += "&gt;";
        } else {
            out += ch;
        }
    }
    return out;
}

/// True when `lang` (a fence tag) denotes FreeBASIC. An empty tag counts —
/// in a FreeBASIC IDE an untagged block is assumed to be FreeBASIC.
auto isFreeBasicTag(const wxString& lang) -> bool {
    return lang.empty() || lang == "freebasic" || lang == "fb" || lang == "basic" || lang == "bas";
}
} // namespace

AiChatPanel::AiChatPanel(wxWindow* parent, Context& ctx)
: wxPanel(parent, wxID_ANY)
, m_ctx(ctx) {
    auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);

    m_output = make_unowned<wxHtmlWindow>(this, wxID_ANY);
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
    wxString html = "<html><body>";
    for (const auto& message : m_ctx.getAiManager().history()) {
        const bool isAssistant = message.role == AiRole::Assistant;
        html += "<p><b>";
        html += isAssistant ? "Assistant" : "You";
        html += "</b></p>";
        // Reformat model code; leave the user's own code untouched.
        html += renderMessageBody(message.content, isAssistant);
        html += "<hr>";
    }
    if (m_busy) {
        // The streaming reply is not in the history yet — render the
        // partial text as it arrives.
        html += "<p><b>Assistant</b></p>";
        html += m_streaming.empty() ? wxString("<p><i>Thinking&hellip;</i></p>") : renderMessageBody(m_streaming, true);
    }
    if (!m_lastError.empty()) {
        html += "<p><font color=\"#cc0000\"><b>Error:</b> " + m_lastError + "</font></p>";
    }
    html += "</body></html>";

    m_output->SetPage(html);
    scrollToBottom();
}

auto AiChatPanel::renderMessageBody(const wxString& markdown, const bool reformatCode) -> wxString {
    const auto config = std::make_shared<maddy::ParserConfig>();
    maddy::Parser parser(config);

    wxString html;
    wxString prose;    // accumulated prose lines awaiting maddy
    wxString code;     // accumulated code lines inside the current fence
    wxString codeLang; // language tag of the current fence
    bool inCode = false;

    const auto flushProse = [&] {
        if (!prose.empty()) {
            std::stringstream stream(prose.utf8_string());
            html += wxString::FromUTF8(parser.Parse(stream));
            prose.clear();
        }
    };

    // Split on fenced code blocks (```), keeping empty lines.
    wxStringTokenizer lines(markdown, "\n", wxTOKEN_RET_EMPTY_ALL);
    while (lines.HasMoreTokens()) {
        const wxString line = lines.GetNextToken();
        if (line.Strip(wxString::both).StartsWith("```")) {
            if (inCode) {
                html += renderCodeBlock(code, codeLang, reformatCode);
                code.clear();
                inCode = false;
            } else {
                flushProse();
                codeLang = line.Strip(wxString::both).Mid(3).Strip(wxString::both).Lower();
                inCode = true;
            }
            continue;
        }
        (inCode ? code : prose) += line + "\n";
    }
    // An unterminated fence (mid-stream) still renders what arrived so far.
    if (inCode) {
        html += renderCodeBlock(code, codeLang, reformatCode);
    }
    flushProse();
    return html;
}

auto AiChatPanel::renderCodeBlock(const wxString& code, const wxString& lang, const bool reformat) -> wxString {
    if (isFreeBasicTag(lang)) {
        return m_ctx.getAiManager().highlightFreeBasic(code, reformat);
    }
    return "<pre>" + escapeHtml(code) + "</pre>";
}

void AiChatPanel::scrollToBottom() {
    m_output->Scroll(0, m_output->GetScrollRange(wxVERTICAL));
}
