//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CodeActionBar.hpp"
#include "app/Context.hpp"
#include "command/CommandId.hpp"
#include "ui/ArtiProvider.hpp"
#include "ui/UIManager.hpp"
#include "ui/controls/SmartBoxSizer.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace fbide::ai {
wxDEFINE_EVENT(EVT_CODE_BAR_LEAVE, wxCommandEvent);
} // namespace fbide::ai

wxBEGIN_EVENT_TABLE(CodeActionBar, wxPanel)
    EVT_PAINT(CodeActionBar::onPaint)
    EVT_LEAVE_WINDOW(CodeActionBar::onLeave)
wxEND_EVENT_TABLE()

namespace {
// Extra padding at the bar's left and right ends.
constexpr int kButtonPadding = 2;
// Opacity of an idle (non-hovered) icon.
constexpr double kMutedAlpha = 0.4;

auto kCodeSample = CodeActionBar::Mode::CodeSample;
auto PatchProposal = CodeActionBar::Mode::PatchProposal;

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
: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
    wxPanel::SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    wxPanel::SetBackgroundStyle(wxBG_STYLE_PAINT);

    const auto& art = ctx.getUIManager().getArtProvider();
    const auto buttons = make_unowned<SmartBoxSizer>(SmartBoxSizer::Options { .gap = kButtonPadding }, wxHORIZONTAL);
    addButton(buttons, &kCodeSample, art.getBitmap(CommandId::Copy), ID_CodeCopy, "Copy code");
    addButton(buttons, &kCodeSample, art.getBitmap(CommandId::Paste), ID_CodeInsert, "Insert into editor");
    addButton(buttons, &kCodeSample, art.getBitmap(CommandId::QuickRun), ID_CodeRun, "Compile && run");
    addButton(buttons, &PatchProposal, art.getBitmap(CommandId::Accept), ID_PatchApply, "Apply this edit");
    addButton(buttons, &PatchProposal, art.getBitmap(CommandId::Reject), ID_PatchReject, "Reject this edit");
    SetSizer(buttons);

    m_mode = Mode::PatchProposal;
    setMode(Mode::CodeSample);
}

void CodeActionBar::onPaint(wxPaintEvent& /*event*/) {
    wxPaintDC dc(this);
    const wxRect r = GetClientRect();
    dc.SetBrush(wxBrush(GetBackgroundColour()));
    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW)));
    dc.DrawRectangle(r);
}

void CodeActionBar::setMode(const Mode mode) {
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;

    for (const auto* child : GetSizer()->GetChildren()) {
        if (auto* button = wxDynamicCast(child->GetWindow(), wxBitmapButton)) {
            if (void* data = button->GetClientData()) {
                button->Show(*static_cast<Mode*>(data) == m_mode);
            }
        }
    }

    Layout();
    Fit();
}

void CodeActionBar::addButton(
    wxSizer* sizer,
    Mode* mode,
    const wxBitmap& icon,
    const int id,
    const wxString& tip
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
    button->SetClientData(static_cast<void*>(mode));
    sizer->Add(button);
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
