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
, m_rootSizer(make_unowned<wxBoxSizer>(wxVERTICAL))
, m_currentSizer(m_rootSizer)
, m_ctx(ctx) {
    SetSizer(m_rootSizer);
}

auto Panel::getConfig() const -> Config& {
    return m_ctx.getConfig();
}

void Panel::makeTitle(const LangId langId) {
    text(langId);
    separator(0);
}

void Panel::add(wxWindow* view, const LayoutOptions options){
    m_currentSizer->Add(view, options.proportion, options.flag, options.border);
}

void Panel::text(const LangId langId, const LayoutOptions options) {
    const auto text = make_unowned<wxStaticText>(
        this, wxID_STATIC,
        m_ctx.getLang()[langId],
        wxDefaultPosition, wxDefaultSize, 0
    );
    add(text, options);
}

void Panel::separator(const int border) {
    const bool vertical =  m_currentSizer->GetOrientation() == wxHORIZONTAL;
    const auto line = make_unowned<wxStaticLine>(
        this, wxID_STATIC,
        wxDefaultPosition, wxDefaultSize,
        vertical ? wxVERTICAL : wxHORIZONTAL
    );
    add(line, { .flag = wxEXPAND | (vertical ? wxLEFT | wxRIGHT : wxTOP | wxBOTTOM), .border = border });
}

auto Panel::checkBox(bool& value, const LangId langId, const LayoutOptions options) -> Unowned<wxCheckBox> {
    const auto chk = checkBox(langId, options);
    chk->SetValue(value);
    chk->Bind(wxEVT_CHECKBOX, [&](const wxCommandEvent& evt) {
        value = evt.IsChecked();
    });
    return chk;
}

auto Panel::checkBox(const LangId langId, const LayoutOptions options) -> Unowned<wxCheckBox> {
    const auto chk = make_unowned<wxCheckBox>(this, wxID_ANY, m_ctx.getLang()[langId]);
    add(chk, options);
    return chk;
}

auto Panel::spinCtrl(int& value, const LangId langId, const int minVal, const int maxVal, const LayoutOptions options) -> Unowned<wxSpinCtrl> {
    const auto spin = spinCtrl(langId, minVal, maxVal, options);
    spin->SetValue(value);
    spin->Bind(wxEVT_SPINCTRL, [&](const wxSpinEvent& evt) {
        value = evt.GetInt();
    });
    return spin;
}

auto Panel::spinCtrl(const LangId langId, const int minVal, const int maxVal, const LayoutOptions options) -> Unowned<wxSpinCtrl> {
    if (langId == LangId::EmptyString) {
        const auto spin = make_unowned<wxSpinCtrl>(
            this, wxID_ANY, "",
            wxDefaultPosition, wxDefaultSize,
            wxSP_ARROW_KEYS,
            minVal, maxVal
        );
        add(spin, options);
        return spin;
    }
    return hbox(options, [&] {
        // control
        const auto spin = make_unowned<wxSpinCtrl>(
            this, wxID_ANY, "",
            wxDefaultPosition, wxDefaultSize,
            wxSP_ARROW_KEYS,
            minVal, maxVal
        );
        add(spin, { .flag = wxALIGN_CENTER_VERTICAL | wxALL });

        // label
        text(langId, { .flag = wxALIGN_CENTER_VERTICAL | wxALL });
        return spin;
    });
}

auto Panel::choice(wxString& value, const wxArrayString& choices, const LayoutOptions options) -> Unowned<wxChoice> {
    auto cb = choice(choices, options);

    const auto sel = cb->FindString(value);
    cb->SetSelection(sel != wxNOT_FOUND ? sel : 0);

    cb->Bind(wxEVT_CHOICE, [&](const wxCommandEvent& evt) {
        value = evt.GetString();
    });

    return cb;
}

auto Panel::choice(const wxArrayString& choices, const LayoutOptions options) -> Unowned<wxChoice> {
    auto cb = make_unowned<wxChoice>(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices, wxCB_SORT);
    add(cb, options);
    return cb;
}

auto Panel::textField(wxString& value, const LayoutOptions options) -> Unowned<wxTextCtrl> {
    const auto text = textField(options);
    text->SetValue(value);
    text->Bind(wxEVT_TEXT, [&](const wxCommandEvent& evt) {
        value = evt.GetString();
    });
    return text;
}

auto Panel::textField(const LayoutOptions options) -> Unowned<wxTextCtrl> {
    const auto text = make_unowned<wxTextCtrl>(this, wxID_ANY);
    add(text, options);
    return text;
}

auto Panel::button(const LangId langId, const LayoutOptions options) -> Unowned<wxButton> {
    return button(m_ctx.getLang()[langId], options);
}

auto Panel::button(const wxString& str, const LayoutOptions options) -> Unowned<wxButton> {
    auto btnSave = make_unowned<wxButton>(this, wxID_ANY, str);
    add(btnSave, options);
    return btnSave;
}
