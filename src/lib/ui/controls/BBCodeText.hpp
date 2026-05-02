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

    /// Construct a read-only multi-line `wxTextCtrl` with BBCode parsing.
    BBCodeText(
        wxWindow* parent, wxWindowID id,
        const wxString& value = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0
    );

    /// Set the base font and rebuild bold/normal text attributes.
    auto SetFont(const wxFont& font) -> bool override;
    /// Write `text`, parsing BBCode tags into bold/normal runs.
    void WriteText(const wxString& text) override;
    /// Append `text`, parsing BBCode tags into bold/normal runs.
    void AppendText(const wxString& text) override;

private:
    /// Walk `text`, switching between bold and normal attributes on tag boundaries.
    void parseBBCode(const wxString& text);

    wxTextAttr m_normal; ///< Default text attribute.
    wxTextAttr m_bold;   ///< Bold text attribute.
};

} // namespace fbide
