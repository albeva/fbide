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

// Pack a Button bit into the `void* ClientData` slot.
constexpr auto toVoidPtr(const std::uint8_t bit) -> void* {
    return reinterpret_cast<void*>(static_cast<std::intptr_t>(bit));
}

// Unpack a Button bit from the `void* ClientData` slot.
constexpr auto toBit(void* data) -> std::uint8_t {
    return static_cast<std::uint8_t>(reinterpret_cast<std::intptr_t>(data));
}

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
    // Parent view sets an I-beam cursor over message bubbles. On MSW the
    // child panel inherits that cursor unless it sets its own — force an
    // arrow here so hovering the bar / its buttons doesn't show I-beam.
    wxPanel::SetCursor(wxCursor(wxCURSOR_ARROW));

    const auto& art = ctx.getUIManager().getArtProvider();

    SetSizer(new SmartBoxSizer({ .gap = kButtonPadding, .alignment = SmartBoxSizer::Alignment::Center }, wxHORIZONTAL));
    addButton(Copy, art.getBitmap(CommandId::Copy), ID_CodeCopy, "Copy code");
    addButton(Insert, art.getBitmap(CommandId::Paste), ID_CodeInsert, "Insert into editor");
    addButton(Run, art.getBitmap(CommandId::QuickRun), ID_CodeRun, "Compile && run");
    addButton(Apply, art.getBitmap(CommandId::Accept), ID_PatchApply, "Apply this edit");
    addButton(Reject, art.getBitmap(CommandId::Reject), ID_PatchReject, "Reject this edit");
    addButton(Collapse, art.getBitmap(CommandId::Collapse), ID_BlockCollapse, "Collapse this block");
    addButton(Expand, art.getBitmap(CommandId::Expand), ID_BlockExpand, "Expand this block");

    m_buttons = 0xFF;
    setButtons(0);
}

void CodeActionBar::onPaint(wxPaintEvent& /*event*/) {
    wxPaintDC dc(this);
    const wxRect r = GetClientRect();
    dc.SetBrush(wxBrush(GetBackgroundColour()));
    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW)));
    dc.DrawRectangle(r);
}

void CodeActionBar::setButtons(const std::uint8_t buttons) {
    if (m_buttons == buttons) {
        return;
    }
    m_buttons = buttons;
    for (const auto* child : GetSizer()->GetChildren()) {
        if (auto* button = wxDynamicCast(child->GetWindow(), wxBitmapButton)) {
            const std::uint8_t bit = toBit(button->GetClientData());
            button->Show((bit & m_buttons) != 0);
        }
    }
    Layout();
    Fit();
}

void CodeActionBar::addButton(
    const Button button,
    const wxBitmap& icon,
    const int id,
    const wxString& tip
) {
    // Idle shows a muted icon; mouse-over / focus / pressed show it full. The
    // button keeps its own id — its wxEVT_BUTTON propagates to the host.
    const auto bitmapButton = make_unowned<wxBitmapButton>(
        this, id, faded(icon, kMutedAlpha), wxDefaultPosition, wxDefaultSize,
        wxBU_EXACTFIT | wxBORDER_NONE
    );
    bitmapButton->SetBitmapCurrent(icon);
    bitmapButton->SetBitmapFocus(icon);
    bitmapButton->SetBitmapPressed(icon);
    bitmapButton->SetToolTip(tip);
    bitmapButton->SetClientData(toVoidPtr(static_cast<std::uint8_t>(button)));
    GetSizer()->Add(bitmapButton);
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
