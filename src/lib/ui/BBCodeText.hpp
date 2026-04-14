//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Read-only text control with [bold]...[/bold] markup support.
/// Overrides WriteText and AppendText to parse BBCode tags.
class BBCodeText final : public wxTextCtrl {
public:
    NO_COPY_AND_MOVE(BBCodeText)

    BBCodeText(
        wxWindow *parent, wxWindowID id,
        const wxString& value = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0
    );

    auto SetFont(const wxFont& font) -> bool override;
    void WriteText(const wxString& text) override;
    void AppendText(const wxString& text) override;

private:
    void parseBBCode(const wxString& text);

    wxTextAttr m_normal;
    wxTextAttr m_bold;
};

} // namespace fbide
