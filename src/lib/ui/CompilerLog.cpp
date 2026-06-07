//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerLog.hpp"

#include "UIManager.hpp"
#include "app/Context.hpp"
using namespace fbide;

namespace {
/// Localized title key per section, indexed by `CompilerLogSection`.
/// CompileResult and RunResult intentionally share the "Results:" key.
constexpr auto getTitle(const CompilerLogSection section) -> std::string_view {
    switch (section) {
    case CompilerLogSection::CompilerCommand:
        return "dialogs.log.sectionCommand";
    case CompilerLogSection::CompilerOutput:
        return "dialogs.log.sectionOutput";
    case CompilerLogSection::CompileResult:
        return "dialogs.log.sectionResults";
    case CompilerLogSection::SystemInfo:
        return "dialogs.log.sectionSystem";
    case CompilerLogSection::RunCommand:
        return "dialogs.log.sectionRunCommand";
    case CompilerLogSection::RunResult:
        return "dialogs.log.sectionResults";
    default:
        std::unreachable();
    }
};
} // namespace

wxBEGIN_EVENT_TABLE(CompilerLog, wxDialog)
    EVT_SHOW(CompilerLog::onShow)
wxEND_EVENT_TABLE()

CompilerLog::CompilerLog(wxWindow* parent, const wxString& title)
: wxDialog(
      parent, wxID_ANY, title,
      wxDefaultPosition, wxSize(700, 300),
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX
  ) {}

void CompilerLog::create(Context& ctx) {
    m_ctx = &ctx;

    m_output = make_unowned<wxTextCtrl>(
        this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxTE_RICH2
    );
    const auto font = GetFont();

    const auto fg = m_output->GetForegroundColour();
    const auto bg = m_output->GetBackgroundColour();
    m_normal = wxTextAttr(fg, bg, font);
    m_bold = wxTextAttr(fg, bg, wxFont(font).Bold());
    m_output->SetDefaultStyle(m_normal);

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    sizer->Add(m_output, 1, wxEXPAND);
    SetSizer(sizer);
}

void CompilerLog::onShow(wxShowEvent& event) {
    if (event.IsShown() && not m_sections.empty()) {
        render();
    }
}

void CompilerLog::add(const CompilerLogSection section, const wxString& text) {
    if (IsShown()) {
        const FreezeLock freeze { this };
        render(section, text);
        m_output->SetSelection(0, 0);
    } else {
        m_sections.emplace_back(section, text);
    }
}

void CompilerLog::clear() {
    m_sections.clear();
    m_output->Clear();
}

void CompilerLog::render() {
    const FreezeLock freeze { this };

    for (const auto& [section, text] : m_sections) {
        render(section, text);
    }

    m_sections.clear();
    m_output->SetSelection(0, 0);
}

void CompilerLog::render(const CompilerLogSection section, const wxString& text) const {
    if (not m_output->IsEmpty()) {
        m_output->AppendText('\n');
    }

    m_output->SetDefaultStyle(m_bold);
    m_output->AppendText(m_ctx->tr(wxString(getTitle(section))) + "\n");
    m_output->SetDefaultStyle(m_normal);
    m_output->AppendText(text + "\n");
}
