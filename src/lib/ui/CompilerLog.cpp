//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerLog.hpp"
#include "BBCodeText.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Theme.hpp"
using namespace fbide;

CompilerLog::CompilerLog(wxWindow* parent, const wxString& title)
: wxDialog(
      parent, wxID_ANY, title,
      wxDefaultPosition, wxSize(400, 200),
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX
  ) {}

void CompilerLog::create(const Context& ctx) {
    const auto& style = ctx.getTheme().getDefault();

    auto font = wxFont(style.fontSize, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    if (!style.fontName.empty()) {
        font.SetFaceName(style.fontName);
    }

    m_output = make_unowned<BBCodeText>(this, wxID_ANY);
    m_output->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    m_output->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    m_output->SetFont(font);

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    sizer->Add(m_output, 1, wxEXPAND);
    SetSizer(sizer);
}

void CompilerLog::clear() {
    m_output->Clear();
}

void CompilerLog::log(const wxString& line) {
    m_output->AppendText(line + "\n");
}

void CompilerLog::log(const wxArrayString& lines) {
    for (const auto& line : lines) {
        log(line);
    }
}
