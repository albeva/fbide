//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Templated layout helper that can extend any wxWindow-derived class.
/// Provides automatic gap management between sibling elements.
///
/// Usage:
///   class MyPanel : public Layout<wxPanel> { ... };
///   class MyDialog : public Layout<wxDialog> { ... };
template<typename Base>
class Layout : public Base {
public:
    NO_COPY_AND_MOVE(Layout)

    static constexpr auto DEFAULT_MARGIN = 5;
    static constexpr auto DEFAULT_GAP = 5;

    /// Layout options for an element placed in a sizer.
    struct LayoutOptions final {
        int proportion = 0;
        int flag = wxEXPAND | wxALL;
        int margin = 0;
        int gap = DEFAULT_GAP;
    };

    template<typename... Args>
    explicit Layout(Args&&... args)
    : Base(std::forward<Args>(args)...) {
        m_currentSizer = make_unowned<wxBoxSizer>(wxVERTICAL);
        Base::SetSizer(m_currentSizer);
    }

protected:

    /// Add a window to the current sizer with automatic gap.
    void add(wxWindow* view, LayoutOptions opts = {}) {
        addGapIfNeeded();
        m_currentSizer->Add(view, opts.proportion, opts.flag, opts.margin);
    }

    /// Add a sizer to the current sizer with automatic gap.
    void add(wxSizer* sizer, LayoutOptions opts = {}) {
        addGapIfNeeded();
        m_currentSizer->Add(sizer, opts.proportion, opts.flag, opts.margin);
    }

    /// Add a separator line (orientation auto-detected from parent sizer).
    void separator() {
        const bool horizontal = m_currentSizer->GetOrientation() == wxHORIZONTAL;
        const auto line = make_unowned<wxStaticLine>(
            this, wxID_STATIC,
            wxDefaultPosition, wxDefaultSize,
            horizontal ? wxVERTICAL : wxHORIZONTAL
        );
        add(line, { .flag = wxEXPAND });
    }

    // -- View creators --

    /// Add a static text label.
    auto label(const wxString& str, const LayoutOptions opts = {}) -> Unowned<wxStaticText> {
        const auto ctrl = make_unowned<wxStaticText>(this, wxID_ANY, str);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a checkbox. Binds value automatically.
    auto checkBox(bool& value, const wxString& str, const LayoutOptions opts = {}) -> Unowned<wxCheckBox> {
        const auto ctrl = checkBox(str, opts);
        ctrl->SetValue(value);
        ctrl->Bind(wxEVT_CHECKBOX, [&](const wxCommandEvent& evt) { value = evt.IsChecked(); });
        return ctrl;
    }

    auto checkBox(const wxString& str, const LayoutOptions opts = {}) -> Unowned<wxCheckBox> {
        const auto ctrl = make_unowned<wxCheckBox>(this, wxID_ANY, str);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a spin control. Binds value automatically.
    auto spinCtrl(int& value, const int minVal, const int maxVal, const LayoutOptions opts = {}) -> Unowned<wxSpinCtrl> {
        const auto ctrl = spinCtrl(minVal, maxVal, opts);
        ctrl->SetValue(value);
        ctrl->Bind(wxEVT_SPINCTRL, [&](const wxSpinEvent& evt) { value = evt.GetInt(); });
        return ctrl;
    }

    auto spinCtrl(int minVal, int maxVal, const LayoutOptions opts = {}) -> Unowned<wxSpinCtrl> {
        const auto ctrl = make_unowned<wxSpinCtrl>(
            this, wxID_ANY, "",
            wxDefaultPosition, wxDefaultSize,
            wxSP_ARROW_KEYS, minVal, maxVal
        );
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a choice dropdown. Binds value automatically.
    auto choice(wxString& value, const wxArrayString& choices, const LayoutOptions opts = {}) -> Unowned<wxChoice> {
        const auto ctrl = choice(choices, opts);
        const auto sel = ctrl->FindString(value);
        ctrl->SetSelection(sel != wxNOT_FOUND ? sel : 0);
        ctrl->Bind(wxEVT_CHOICE, [&](const wxCommandEvent& evt) { value = evt.GetString(); });
        return ctrl;
    }

    auto choice(const wxArrayString& choices, LayoutOptions opts = {}) -> Unowned<wxChoice> {
        const auto ctrl = make_unowned<wxChoice>(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a text field. Binds value automatically.
    auto textField(wxString& value, const LayoutOptions opts = {}) -> Unowned<wxTextCtrl> {
        const auto ctrl = textField(opts);
        ctrl->SetValue(value);
        ctrl->Bind(wxEVT_TEXT, [&](const wxCommandEvent& evt) { value = evt.GetString(); });
        return ctrl;
    }

    auto textField(const LayoutOptions opts = {}) -> Unowned<wxTextCtrl> {
        const auto ctrl = make_unowned<wxTextCtrl>(this, wxID_ANY);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a button.
    auto button(const wxString& str, const LayoutOptions opts = {}) -> Unowned<wxButton> {
        const auto ctrl = make_unowned<wxButton>(this, wxID_ANY, str);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a radio button. Pass wxRB_GROUP in opts.flag to start a new group.
    auto radioButton(const wxString& str, const LayoutOptions opts = {}) -> Unowned<wxRadioButton> {
        const auto ctrl = make_unowned<wxRadioButton>(this, wxID_ANY, str, wxDefaultPosition, wxDefaultSize,
            opts.flag & wxRB_GROUP ? wxRB_GROUP : 0);
        add(ctrl, { .proportion = opts.proportion, .flag = opts.flag & ~wxRB_GROUP, .margin = opts.margin });
        return ctrl;
    }

    // -- Box containers --

    template<std::invocable Func>
    auto hbox(const LayoutOptions opts, Func&& func) -> std::invoke_result_t<Func> {
        return makeBox(wxEmptyString, wxHORIZONTAL, opts, std::forward<Func>(func));
    }

    template<std::invocable Func>
    auto hbox(const wxString& title, const LayoutOptions opts, Func&& func) -> std::invoke_result_t<Func> {
        return makeBox(title, wxHORIZONTAL, opts, std::forward<Func>(func));
    }

    template<std::invocable Func>
    auto vbox(const LayoutOptions opts, Func&& func) -> std::invoke_result_t<Func> {
        return makeBox(wxEmptyString, wxVERTICAL, opts, std::forward<Func>(func));
    }

    template<std::invocable Func>
    auto vbox(const wxString& title, const LayoutOptions opts, Func&& func) -> std::invoke_result_t<Func> {
        return makeBox(title, wxVERTICAL, opts, std::forward<Func>(func));
    }

private:
    void addGapIfNeeded() {
        if (m_currentSizer->GetItemCount() > 0 && m_currentOptions.gap > 0) {
            m_currentSizer->AddSpacer(m_currentOptions.gap);
        }
    }

    template<std::invocable Func>
    auto makeBox(const wxString& title, const int direction, const LayoutOptions opts, Func&& func) -> std::invoke_result_t<Func> {
        const auto sizer = [&] -> wxBoxSizer* {
            if (title.empty()) {
                return make_unowned<wxBoxSizer>(direction);
            }
            return make_unowned<wxStaticBoxSizer>(direction, this, title);
        }();

        add(sizer, { .proportion = opts.proportion, .flag = opts.flag, .margin = opts.margin });

        const ValueRestorer restoreSizer { m_currentSizer, m_currentOptions };
        m_currentSizer = sizer;
        m_currentOptions = opts;
        return std::invoke(std::forward<Func>(func));
    }

    wxBoxSizer* m_currentSizer = nullptr;
    LayoutOptions m_currentOptions {};
};

} // namespace fbide
