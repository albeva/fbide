//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerLog.hpp"

#include "UIManager.hpp"
#include "app/Context.hpp"
#include "config/Theme.hpp"
#include "controls/BBCodeText.hpp"
using namespace fbide;

CompilerLog::CompilerLog(wxWindow* parent, const wxString& title)
: wxDialog(
      parent, wxID_ANY, title,
      wxDefaultPosition, wxSize(700, 300),
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX
  ) {}

void CompilerLog::create(const Context& /*ctx*/) {
    m_output = make_unowned<BBCodeText>(this, wxID_ANY);
    m_output->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    sizer->Add(m_output, 1, wxEXPAND);
    SetSizer(sizer);
}

void CompilerLog::log(const wxArrayString& lines) {
    const FreezeLock freeze { this };
    m_output->Clear();
    for (const auto& line : lines) {
        m_output->AppendText(line + "\n");
    }
    m_output->SetScrollPos(wxVERTICAL, 0);
    m_output->SetScrollPos(wxHORIZONTAL, 0);
    m_output->SetSelection(0, 0);
}
