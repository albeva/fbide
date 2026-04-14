//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "BBCodeText.hpp"
using namespace fbide;

BBCodeText::BBCodeText(
    wxWindow *parent,
    const wxWindowID id,
    const wxString& value,
    const wxPoint& pos,
    const wxSize& size,
    const long style
)
: wxTextCtrl(
      parent, id, wxEmptyString,
      pos, size,
      wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxTE_RICH2 | style
  ) {
    const auto fg = GetForegroundColour();
    const auto bg = GetBackgroundColour();

    const auto font = GetFont();
    m_normal = wxTextAttr(fg, bg, font);
    m_bold = wxTextAttr(fg, bg, font.Bold());
    wxTextCtrl::SetDefaultStyle(m_normal);
    parseBBCode(value);
    wxTextCtrl::SetInsertionPoint(0);
}

auto BBCodeText::SetFont(const wxFont& font) -> bool {
    const auto res = wxTextCtrl::SetFont(font);
    const auto fg = GetForegroundColour();
    const auto bg = GetBackgroundColour();
    m_normal = wxTextAttr(fg, bg, font);
    m_bold = wxTextAttr(fg, bg, wxFont(font).Bold());
    SetDefaultStyle(m_normal);
    return res;
}

void BBCodeText::WriteText(const wxString& text) {
    parseBBCode(text);
}

void BBCodeText::AppendText(const wxString& text) {
    parseBBCode(text);
}

void BBCodeText::parseBBCode(const wxString& text) {
    bool inTag = false;
    wxString tag;
    for (const auto ch : text) {
        if (ch == '[' && !inTag) {
            inTag = true;
        } else if (ch == ']' && inTag) {
            inTag = false;
            if (tag.Lower() == "bold") {
                SetDefaultStyle(m_bold);
            } else if (tag.Lower() == "/bold") {
                SetDefaultStyle(m_normal);
            }
            tag.clear();
        } else if (inTag) {
            tag += ch;
        } else {
            wxTextCtrl::WriteText(wxString(ch));
        }
    }
}
