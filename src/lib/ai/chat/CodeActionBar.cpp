//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CodeActionBar.hpp"
#include <wx/bmpbuttn.h>
#include <wx/settings.h>
#include <wx/sizer.h>
using namespace fbide;

namespace {
// Gap between a button and the bar edge / its neighbours.
constexpr int kButtonGap = 1;
} // namespace

CodeActionBar::CodeActionBar(
    wxWindow* parent,
    const wxBitmap& copyIcon,
    const wxBitmap& insertIcon,
    const wxBitmap& runIcon,
    Action onCopy,
    Action onInsert,
    Action onRun
)
: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE) {
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    SetDoubleBuffered(true); // flicker-free button hover repaints

    auto sizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    const auto addButton = [&](const wxBitmap& icon, const wxString& tip, Action action) {
        auto button = make_unowned<wxBitmapButton>(
            this, wxID_ANY, icon, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT | wxBORDER_NONE
        );
        button->SetToolTip(tip);
        button->Bind(wxEVT_BUTTON, [action = std::move(action)](wxCommandEvent&) { action(); });
        sizer->Add(button, wxSizerFlags().Border(wxALL, kButtonGap));
    };
    addButton(copyIcon, "Copy code", std::move(onCopy));
    addButton(insertIcon, "Insert into editor", std::move(onInsert));
    addButton(runIcon, "Compile && run", std::move(onRun));

    SetSizer(sizer);
    sizer->SetSizeHints(this); // shrink the bar to fit its buttons

    Bind(wxEVT_LEAVE_WINDOW, &CodeActionBar::onLeave, this);
}

void CodeActionBar::onLeave(wxMouseEvent& event) {
    // wxEVT_LEAVE_WINDOW also fires when the pointer moves onto a child
    // button — only report a leave when it is truly outside the bar.
    const wxRect screenRect(GetScreenPosition(), GetSize());
    if (!screenRect.Contains(wxGetMousePosition()) && m_onLeave) {
        m_onLeave();
    }
    event.Skip();
}
