//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerLog.hpp"
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

    const auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    const auto fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);

    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
    m_output = make_unowned<wxTextCtrl>(
        this, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_BESTWRAP | wxTE_RICH2
    );
    m_output->SetFont(font);
    m_output->SetBackgroundColour(bg);
    m_output->SetForegroundColour(fg);

    sizer->Add(m_output, 1, wxEXPAND);
    SetSizer(sizer);

    m_normal = wxTextAttr(fg, bg, font);
    m_bold = wxTextAttr(fg, bg, font.Bold());
    m_output->SetDefaultStyle(m_normal);
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
