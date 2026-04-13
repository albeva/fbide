//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerLog.hpp"
using namespace fbide;

CompilerLog::CompilerLog(wxWindow* parent, const wxString& title)
: wxDialog(
      parent, wxID_ANY, title,
      wxDefaultPosition, wxSize(400, 200),
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX
  ) {}

void CompilerLog::create() {
    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    m_output = make_unowned<wxTextCtrl>(
        this, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_BESTWRAP | wxTE_RICH2
    );
    m_output->SetFont(wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    sizer->Add(m_output, 1, wxEXPAND);
    SetSizer(sizer);

    m_normal = m_output->GetDefaultStyle();
    m_bold = m_normal;
    m_bold.SetFont(m_bold.GetFont().Bold());
}

void CompilerLog::clear() {
    m_output->Clear();
}

void CompilerLog::log(const wxString& line) {
    bool inTag = false;
    wxString tag;
    for (const auto ch : line) {
        if (ch == '[' && !inTag) {
            inTag = true;
        } else if (ch == ']' && inTag) {
            inTag = false;
            if (tag.Lower() == "bold") {
                m_output->SetDefaultStyle(m_bold);
            } else if (tag.Lower() == "/bold") {
                m_output->SetDefaultStyle(m_normal);
            }
            tag.clear();
        } else if (inTag) {
            tag += ch;
        } else {
            m_output->WriteText(wxString(ch));
        }
    }
    m_output->WriteText("\n");
}

void CompilerLog::log(const wxArrayString& lines) {
    for (const auto& line : lines) {
        log(line);
    }
}
