//
// Created by Albert Varaksin on 11/04/2026.
//
#include "Panel.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Lang.hpp"

using namespace fbide;
Panel::Panel(Context& ctx, const wxWindowID id, wxWindow* parent)
: wxPanel(
      parent, id,
      wxDefaultPosition, wxDefaultSize,
      wxNO_BORDER | wxCLIP_CHILDREN | wxTAB_TRAVERSAL
  )
, m_vbox(make_unowned<wxBoxSizer>(wxVERTICAL))
, m_ctx(ctx) {
    SetSizer(m_vbox);
}

auto Panel::getConfig() const -> Config& {
    return m_ctx.getConfig();
}

void Panel::makeTitle(const LangId langId) {
    makeText(m_vbox, langId);
    makeSeparator(m_vbox, wxHORIZONTAL);
}

void Panel::makeText(wxSizer* sizer, const LangId langId, const int flags) {
    const auto text = make_unowned<wxStaticText>(
        this, wxID_STATIC,
        m_ctx.getLang()[langId],
        wxDefaultPosition, wxDefaultSize, 0
    );
    sizer->Add(text, 0, flags, 5);
}

void Panel::makeSeparator(wxSizer* sizer, const long style) {
    const auto line = make_unowned<wxStaticLine>(
        this, wxID_STATIC,
        wxDefaultPosition, wxDefaultSize,
        style
    );
    sizer->Add(line, 0, wxGROW, 5);
}

void Panel::makeCheckBox(wxSizer* sizer, bool& value, const LangId langId) {
    const auto chk = make_unowned<wxCheckBox>(this, wxID_ANY, m_ctx.getLang()[langId]);
    sizer->Add(chk, 0, wxGROW | wxALL, 5);
    chk->SetValue(value);
    chk->Bind(wxEVT_CHECKBOX, [&](const wxCommandEvent& evt) {
        value = evt.IsChecked();
    });
}

void Panel::makeSpinCtrl(wxSizer* sizer, int& value, const LangId langId, const int minVal, const int maxVal, const int width) {
    const auto row = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    sizer->Add(row, 0, wxALL, 0);

    // control
    const auto spin = make_unowned<wxSpinCtrl>(
        this, wxID_ANY, "",
        wxDefaultPosition, wxSize(width, -1),
        wxSP_ARROW_KEYS,
        minVal, maxVal, value
    );
    row->Add(spin, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    // label
    // row->Add(make_unowned<wxStaticText>(this, wxID_ANY, m_ctx.getLang()[langId]), 0, wxALIGN_CENTER_VERTICAL | wxALL);
    makeText(row, langId, wxALIGN_CENTER_VERTICAL | wxALL);

    spin->Bind(wxEVT_SPINCTRL, [&](const wxSpinEvent& evt) {
        value = evt.GetInt();
    });
}

void Panel::makeChoice(wxSizer* sizer, wxString& value, const LangId langId, const wxArrayString& choices) {
    const auto row = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    sizer->Add(row, 0, wxEXPAND | wxALL, 5);

    makeText(row, langId, wxALIGN_CENTER_VERTICAL);

    const auto choice = make_unowned<wxChoice>(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
    row->AddSpacer(5);
    row->Add(choice, 1, wxGROW | wxALL, 0);

    const auto sel = choice->FindString(value);
    choice->SetSelection(sel != wxNOT_FOUND ? sel : 0);

    choice->Bind(wxEVT_CHOICE, [&](const wxCommandEvent& evt) {
        value = evt.GetString();
    });
}
