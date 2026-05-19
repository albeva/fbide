//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CodeActionBar.hpp"
#include <wx/bmpbuttn.h>
#include <wx/image.h>
#include <wx/settings.h>
#include <wx/sizer.h>
using namespace fbide;

namespace {
// Gap around each button.
constexpr int kButtonGap = 1;
// Extra padding at the bar's left and right ends.
constexpr int kSidePadding = 4;
// Opacity of an idle (non-hovered) icon.
constexpr double kMutedAlpha = 0.45;

/// A dimmed copy of `bitmap` — its alpha scaled by `factor` so an idle icon
/// reads as muted next to the hovered one.
auto faded(const wxBitmap& bitmap, const double factor) -> wxBitmap {
    wxImage image = bitmap.ConvertToImage();
    if (!image.HasAlpha()) {
        image.InitAlpha(); // derive an alpha channel from the transparency mask
    }
    unsigned char* const alpha = image.GetAlpha();
    const int count = image.GetWidth() * image.GetHeight();
    for (int i = 0; i < count; i++) {
        alpha[i] = static_cast<unsigned char>(static_cast<double>(alpha[i]) * factor);
    }
    return wxBitmap(image);
}
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
        // Idle shows a muted icon; mouse-over / focus / pressed show it full.
        auto button = make_unowned<wxBitmapButton>(
            this, wxID_ANY, faded(icon, kMutedAlpha), wxDefaultPosition, wxDefaultSize,
            wxBU_EXACTFIT | wxBORDER_NONE
        );
        button->SetBitmapCurrent(icon);
        button->SetBitmapFocus(icon);
        button->SetBitmapPressed(icon);
        button->SetToolTip(tip);
        button->Bind(wxEVT_BUTTON, [action = std::move(action)](wxCommandEvent&) { action(); });
        sizer->Add(button, wxSizerFlags().Border(wxALL, kButtonGap));
    };

    sizer->AddSpacer(kSidePadding);
    addButton(copyIcon, "Copy code", std::move(onCopy));
    addButton(insertIcon, "Insert into editor", std::move(onInsert));
    addButton(runIcon, "Compile && run", std::move(onRun));
    sizer->AddSpacer(kSidePadding);

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
