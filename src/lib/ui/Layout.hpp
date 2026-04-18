//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

template<typename T>
concept SizerAware = requires(T& parent, wxSizer* sizer) {
    { parent.SetSizer(sizer) } -> std::same_as<void>;
};

/// Default gaps and paddings, adhere to platform default
inline constexpr auto DEFAULT_PADDING = -1;

/// Layout options container sizers
struct LayoutContainerOptions final {
    /// Sizing proportion inside parent container
    int proportion = 0;
    /// Fill view along container axis
    bool expand = true;
    /// Add leading (left or top) space
    bool space = true;
    /// Apply parent container padding
    bool padding = true;
    /// Center child items within the container
    bool center = false;
    /// Space around child items
    int border = DEFAULT_PADDING;
    /// Space between child items
    int gap = DEFAULT_PADDING;
};

/// Layout options for individual managed items
struct LayoutItemOptions final {
    /// Sizing proportion inside parent container
    int proportion = 0;
    /// Fill view along container axis
    bool expand = true;
    /// Add leading (left or top) space
    bool space = true;
    /// Apply parent container padding
    bool padding = true;
};

constexpr LayoutItemOptions defaultItemOptions = {};
constexpr LayoutItemOptions defaultSeparatorOptions = { .padding = false };

/// Templated layout helper that can extend any wxWindow-derived class.
/// Provides automatic gap management between sibling elements.
///
/// Usage:
///   class MyPanel : public Layout<wxPanel> { ... };
///   class MyDialog : public Layout<wxDialog> { ... };
template<SizerAware Base>
class Layout : public Base {
public:
    NO_COPY_AND_MOVE(Layout)

    template<typename... Args>
    explicit Layout(Args&&... args)
    : Base(std::forward<Args>(args)...) {
        m_currentSizer = make_unowned<wxBoxSizer>(wxVERTICAL);
        Base::SetSizerAndFit(m_currentSizer);
    }

    static auto defaultBorder() -> int {
        return std::max(5, wxSizerFlags::GetDefaultBorder());
    }

    static auto resolveBorder(const int border) -> int {
        return border == DEFAULT_PADDING ? defaultBorder() : border;
    }

    /// Add a window to the current sizer with automatic gap.
    void add(wxWindow* view, const LayoutItemOptions opts = {}) {
        const auto calc = calculate(opts);
        if (calc.space != 0) {
            m_currentSizer->AddSpacer(resolveBorder(calc.space));
        }
        m_currentSizer->Add(view, calc.proportion, calc.flags, resolveBorder(calc.border));
    }

    /// Add child sizer
    void add(wxSizer* sizer, const LayoutContainerOptions opts = {}) {
        const auto calc = calculate({ .proportion = opts.proportion,
            .expand = opts.expand,
            .space = opts.space,
            .padding = opts.padding });
        if (calc.space != 0) {
            m_currentSizer->AddSpacer(resolveBorder(calc.space));
        }
        m_currentSizer->Add(sizer, calc.proportion, calc.flags, resolveBorder(calc.border));
    }

    /// Add a separator line (orientation auto-detected from parent sizer).
    void separator(const LayoutItemOptions opts = defaultSeparatorOptions) {
        const bool horizontal = m_currentSizer->GetOrientation() == wxHORIZONTAL;
        const auto line = make_unowned<wxStaticLine>(
            m_currentParent, wxID_STATIC,
            wxDefaultPosition, wxDefaultSize,
            horizontal ? wxVERTICAL : wxHORIZONTAL
        );
        add(line, opts);
    }

    /// Add a space
    void spacer(const int size = DEFAULT_PADDING) { currentSizer()->AddSpacer(resolveBorder(size)); }

    // -----------------------------------------------------------------------
    // Controls
    // -----------------------------------------------------------------------

    /// Add a static text label.
    auto label(const wxString& str, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxStaticText> {
        const auto ctrl = make_unowned<wxStaticText>(m_currentParent, id, str, wxDefaultPosition, wxDefaultSize, style);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a checkbox
    auto checkBox(bool& value, const wxString& str, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxCheckBox> {
        const auto ctrl = checkBox(str, opts, id, style);
        ctrl->SetValue(value);
        ctrl->Bind(wxEVT_CHECKBOX, [&](const wxCommandEvent& evt) {
            value = evt.IsChecked();
        });
        return ctrl;
    }

    auto checkBox(const wxString& str, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxCheckBox> {
        const auto ctrl = make_unowned<wxCheckBox>(m_currentParent, id, str, wxDefaultPosition, wxDefaultSize, style);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a spin control with initial value
    auto spinCtrl(int& value, const int minVal, const int maxVal, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxSpinCtrl> {
        const auto ctrl = spinCtrl(minVal, maxVal, opts, id, style);
        ctrl->SetValue(value);
        ctrl->Bind(wxEVT_SPINCTRL, [&](const wxSpinEvent& evt) {
            value = evt.GetInt();
        });
        return ctrl;
    }

    /// Add spin control
    auto spinCtrl(int minVal, int maxVal, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxSpinCtrl> {
        const auto ctrl = make_unowned<wxSpinCtrl>(
            m_currentParent, id, "",
            wxDefaultPosition, wxDefaultSize,
            wxSP_ARROW_KEYS | style, minVal, maxVal
        );
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a choice dropdown with initial value.
    auto choice(wxString& value, const wxArrayString& choices, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxChoice> {
        const auto ctrl = choice(choices, opts, id, style);
        const auto sel = ctrl->FindString(value);
        ctrl->SetSelection(sel != wxNOT_FOUND ? sel : 0);
        ctrl->Bind(wxEVT_CHOICE, [&](const wxCommandEvent& evt) {
            value = evt.GetString();
        });
        return ctrl;
    }

    /// Add a choice dropdown
    auto choice(const wxArrayString& choices, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxChoice> {
        const auto ctrl = make_unowned<wxChoice>(m_currentParent, id, wxDefaultPosition, wxDefaultSize, choices, style);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a text field with initial value
    auto textField(wxString& value, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxTextCtrl> {
        const auto ctrl = textField(opts, id, style);
        ctrl->SetValue(value);
        ctrl->Bind(wxEVT_TEXT, [&](const wxCommandEvent& evt) {
            value = evt.GetString();
        });
        return ctrl;
    }

    /// Add a text field
    auto textField(const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxTextCtrl> {
        const auto ctrl = make_unowned<wxTextCtrl>(m_currentParent, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, style);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a button.
    auto button(const wxString& str, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxButton> {
        const auto ctrl = make_unowned<wxButton>(m_currentParent, id, str, wxDefaultPosition, wxDefaultSize, style);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a radio button
    auto radio(const wxString& str, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxRadioButton> {
        const auto ctrl = make_unowned<wxRadioButton>(m_currentParent, id, str, wxDefaultPosition, wxDefaultSize, style);
        add(ctrl, opts);
        return ctrl;
    }

    /// Connect label to a control
    void connect(wxStaticText* label, wxControl* control) {
        label->Bind(wxEVT_LEFT_DOWN, [control](const auto&) {
            control->SetFocus();
        });
    }

    // -----------------------------------------------------------------------
    // HBOX
    // -----------------------------------------------------------------------

    /// Lay child items out horizontally
    template<std::invocable Func>
    void hbox(const LayoutContainerOptions opts, Func&& func) {
        makeBox(wxEmptyString, wxHORIZONTAL, opts, std::forward<Func>(func));
    }

    /// Lay child items out horizontally in a named bordered box
    template<std::invocable Func>
    void hbox(const wxString& title, const LayoutContainerOptions opts, Func&& func) {
        makeBox(title, wxHORIZONTAL, opts, std::forward<Func>(func));
    }

    // -----------------------------------------------------------------------
    // VBOX
    // -----------------------------------------------------------------------

    /// Lay child items out vertically
    template<std::invocable Func>
    void vbox(const LayoutContainerOptions opts, Func&& func) {
        makeBox(wxEmptyString, wxVERTICAL, opts, std::forward<Func>(func));
    }

    /// Lay child items out vertically in a named bordered box
    template<std::invocable Func>
    void vbox(const wxString& title, const LayoutContainerOptions opts, Func&& func) {
        makeBox(title, wxVERTICAL, opts, std::forward<Func>(func));
    }

    // -----------------------------------------------------------------------
    // Layout options
    // -----------------------------------------------------------------------

    /// Gey current managed sizer (hbox or vbox)
    [[nodiscard]] auto currentSizer() const -> wxBoxSizer* { return m_currentSizer; }

    /// Get current parent that should own the added view
    [[nodiscard]] auto currentParent() const -> wxWindow* { return m_currentParent; }

    /// Get current layout options
    [[nodiscard]] auto currentOptions() -> LayoutContainerOptions& { return m_currentOptions; }

private:
    struct CalculatedOptions final {
        int space;
        int proportion;
        int flags;
        int border;
    };

    auto calculate(const LayoutItemOptions opts) -> CalculatedOptions {
        const bool isFirst = m_currentSizer->GetItemCount() == 0;
        const bool vertical = m_currentSizer->IsVertical();
        int flags = vertical ? wxLEFT | wxRIGHT : wxTOP | wxBOTTOM;
        const int padding = opts.padding ? m_currentOptions.border : 0;
        const int border = padding;
        const int space = opts.space ? (isFirst ? padding : m_currentOptions.gap) : 0;

        if (opts.expand) {
            flags |= wxEXPAND;
        } else if (m_currentOptions.center) {
            flags |= vertical ? wxALIGN_CENTER_HORIZONTAL : wxALIGN_CENTER_VERTICAL;
        }

        return { .space = space, .proportion = opts.proportion, .flags = flags, .border = border };
    }

    template<std::invocable Func>
    void makeBox(const wxString& title, const int direction, const LayoutContainerOptions opts, Func&& func) {
        const ValueRestorer restoreSizer { m_currentSizer, m_currentOptions, m_currentParent };
        auto* sizer = [&] -> wxBoxSizer* {
            if (title.empty()) {
                return make_unowned<wxBoxSizer>(direction);
            }
            const auto ss = make_unowned<wxStaticBoxSizer>(direction, m_currentParent, title);
            m_currentParent = ss->GetStaticBox();
            return ss;
        }();
        add(sizer, opts);
        m_currentSizer = sizer;
        m_currentOptions = opts;
        std::invoke(std::forward<Func>(func));
        closeContainer();
    }

    void closeContainer() {
        if (m_currentSizer->IsEmpty()) {
            return;
        }
        m_currentSizer->AddSpacer(resolveBorder(m_currentOptions.border));
    }

    wxBoxSizer* m_currentSizer = nullptr;
    wxWindow* m_currentParent = this;
    LayoutContainerOptions m_currentOptions = {};
};

} // namespace fbide
