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
using namespace fbide::ai;

namespace fbide::ai {
wxDEFINE_EVENT(EVT_CODE_BAR_LEAVE, wxCommandEvent);
} // namespace fbide::ai

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
    wxPanel::SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    wxPanel::SetDoubleBuffered(true); // flicker-free button hover repaints

    const auto& art = ctx.getUIManager().getArtProvider();

    const auto sizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    sizer->AddSpacer(kSidePadding);
    // Both button sets are added to the same row sizer up front; setMode
    // shows / hides the inactive group and re-runs Layout + Fit so the
    // bar shrinks to whichever set is visible.
    addButton(sizer, art.getBitmap(CommandId::Copy), ID_CodeCopy, "Copy code", m_codeButtons);
    addButton(sizer, art.getBitmap(CommandId::Paste), ID_CodeInsert, "Insert into editor", m_codeButtons);
    addButton(sizer, art.getBitmap(CommandId::QuickRun), ID_CodeRun, "Compile && run", m_codeButtons);
    addButton(sizer, art.getBitmap(CommandId::Accept), ID_PatchApply, "Apply this edit", m_patchButtons);
    addButton(sizer, art.getBitmap(CommandId::Reject), ID_PatchReject, "Reject this edit", m_patchButtons);
    sizer->AddSpacer(kSidePadding);

    SetSizer(sizer);

    // Hide the proposal set initially; CodeSample is the default mode and
    // matches the historical layout. Fit() sizes the bar to just the
    // visible buttons.
    for (auto* button : m_patchButtons) {
        button->Hide();
    }
    Layout();
    Fit();

    Bind(wxEVT_LEAVE_WINDOW, &CodeActionBar::onLeave, this);
}

void CodeActionBar::setMode(const Mode mode) {
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    const bool showCode = (mode == Mode::CodeSample);
    const bool showPatch = (mode == Mode::PatchProposal);
    for (auto* button : m_codeButtons) {
        button->Show(showCode);
    }
    for (auto* button : m_patchButtons) {
        button->Show(showPatch);
    }
    Layout();
    Fit();
}

void CodeActionBar::addButton(
    wxSizer* sizer,
    const wxBitmap& icon,
    const int id,
    const wxString& tip,
    std::vector<wxWindow*>& group
) {
    // Idle shows a muted icon; mouse-over / focus / pressed show it full. The
    // button keeps its own id — its wxEVT_BUTTON propagates to the host.
    const auto button = make_unowned<wxBitmapButton>(
        this, id, faded(icon, kMutedAlpha), wxDefaultPosition, wxDefaultSize,
        wxBU_EXACTFIT | wxBORDER_NONE
    );
    button->SetBitmapCurrent(icon);
    button->SetBitmapFocus(icon);
    button->SetBitmapPressed(icon);
    button->SetToolTip(tip);
    sizer->Add(button, wxSizerFlags().Border(wxALL, kButtonGap));
    group.push_back(button);
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
