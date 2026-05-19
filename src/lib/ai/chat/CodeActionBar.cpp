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
#include "app/Context.hpp"
#include "command/CommandId.hpp"
#include "ui/ArtiProvider.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace fbide {
wxDEFINE_EVENT(EVT_CODE_ACTION, wxCommandEvent);
wxDEFINE_EVENT(EVT_CODE_BAR_LEAVE, wxCommandEvent);
} // namespace fbide

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

CodeActionBar::CodeActionBar(wxWindow* parent, Context& ctx)
: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE) {
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    SetDoubleBuffered(true); // flicker-free button hover repaints

    const auto& art = ctx.getUIManager().getArtProvider();

    auto sizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    sizer->AddSpacer(kSidePadding);
    addButton(sizer, art.getBitmap(CommandId::Copy), CodeAction::Copy, "Copy code");
    addButton(sizer, art.getBitmap(CommandId::Paste), CodeAction::Insert, "Insert into editor");
    addButton(sizer, art.getBitmap(CommandId::QuickRun), CodeAction::Run, "Compile && run");
    sizer->AddSpacer(kSidePadding);

    SetSizer(sizer);
    sizer->SetSizeHints(this); // shrink the bar to fit its buttons

    Bind(wxEVT_LEAVE_WINDOW, &CodeActionBar::onLeave, this);
}

void CodeActionBar::addButton(
    wxSizer* sizer,
    const wxBitmap& icon,
    const CodeAction action,
    const wxString& tip
) {
    // Idle shows a muted icon; mouse-over / focus / pressed show it full.
    auto button = make_unowned<wxBitmapButton>(
        this, wxID_ANY, faded(icon, kMutedAlpha), wxDefaultPosition, wxDefaultSize,
        wxBU_EXACTFIT | wxBORDER_NONE
    );
    button->SetBitmapCurrent(icon);
    button->SetBitmapFocus(icon);
    button->SetBitmapPressed(icon);
    button->SetToolTip(tip);
    button->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent&) { emitAction(action); });
    sizer->Add(button, wxSizerFlags().Border(wxALL, kButtonGap));
}

void CodeActionBar::emitAction(const CodeAction action) {
    wxCommandEvent event(EVT_CODE_ACTION, GetId());
    event.SetEventObject(this);
    event.SetInt(static_cast<int>(action));
    ProcessWindowEvent(event); // unhandled here — propagates to the host
}

void CodeActionBar::onLeave(wxMouseEvent& event) {
    // wxEVT_LEAVE_WINDOW also fires when the pointer moves onto a child
    // button — only report a leave when it is truly outside the bar.
    const wxRect screenRect(GetScreenPosition(), GetSize());
    if (!screenRect.Contains(wxGetMousePosition())) {
        wxCommandEvent leaveEvent(EVT_CODE_BAR_LEAVE, GetId());
        leaveEvent.SetEventObject(this);
        ProcessWindowEvent(leaveEvent);
    }
    event.Skip();
}
