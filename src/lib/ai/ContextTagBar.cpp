//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ContextTagBar.hpp"
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include "AiContext.hpp"
#include "AiManager.hpp"
#include "app/Context.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace fbide::ai {
wxDEFINE_EVENT(EVT_CONTEXT_TAGS_CHANGED, wxCommandEvent);
} // namespace fbide::ai

namespace {
// Gap between chips.
constexpr int kChipGap = 4;
// Padding inside a chip.
constexpr int kChipPad = 3;
} // namespace

ContextTagBar::ContextTagBar(wxWindow* parent, Context& ctx)
: wxPanel(parent, wxID_ANY)
, m_ctx(ctx) {
    SetSizer(make_unowned<wxBoxSizer>(wxHORIZONTAL));
    refresh();
}

void ContextTagBar::refresh() {
    wxSizer* sizer = GetSizer();
    sizer->Clear(true); // delete the previous chip windows

    const auto& items = m_ctx.getAiManager().context().items();
    for (std::size_t index = 0; index < items.size(); index++) {
        // One chip: a bordered panel with the file name and a close button.
        auto chip = make_unowned<wxPanel>(
            this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_SIMPLE
        );
        chip->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

        auto chipSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
        auto label = make_unowned<wxStaticText>(chip, wxID_ANY, items[index]->label());
        chipSizer->Add(label, wxSizerFlags().Centre().Border(wxLEFT | wxTOP | wxBOTTOM, kChipPad));

        auto close = make_unowned<wxButton>(
            chip, wxID_ANY, wxString::FromUTF8("\xC3\x97"), // multiplication sign ×
            wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT | wxBORDER_NONE
        );
        close->SetToolTip("Remove from context");
        close->Bind(wxEVT_BUTTON, [this, index](wxCommandEvent&) { removeItem(index); });
        chipSizer->Add(close, wxSizerFlags().Centre().Border(wxALL, kChipPad));

        chip->SetSizer(chipSizer);
        sizer->Add(chip, wxSizerFlags().Centre().Border(wxRIGHT, kChipGap));
    }

    Show(!items.empty());
    sizer->Layout();
}

void ContextTagBar::removeItem(const std::size_t index) {
    m_ctx.getAiManager().context().removeAt(index);
    refresh();

    wxCommandEvent event(EVT_CONTEXT_TAGS_CHANGED, GetId());
    event.SetEventObject(this);
    ProcessWindowEvent(event); // unhandled here — propagates to the host
}
