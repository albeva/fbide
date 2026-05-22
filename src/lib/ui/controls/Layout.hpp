//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "SmartBoxSizer.hpp"

namespace fbide {

/// Concept matching any wx-window-like type that accepts a `wxSizer*` via `SetSizer`.
template<typename T>
concept SizerAware = requires(T& parent, wxSizer* sizer) {
    { parent.SetSizer(sizer) } -> std::same_as<void>;
};

/// Container options — drive the `SmartBoxSizer` that backs every
/// `vbox` / `hbox`. `border` is the per-item border (margin) applied
/// to every visible child; `gap` is the per-item border inside the
/// inner sizer when `border != gap` triggers `SmartBoxSizer`'s nested
/// path. `center` adds cross-axis centring to every non-`wxEXPAND`
/// item. Container-level `proportion` / `expand` decide how the
/// container sits inside its parent.
struct LayoutContainerOptions final {
    int proportion = 0;
    bool expand = true;
    SmartBoxSizer::Alignment alignment = SmartBoxSizer::Alignment::None;
    bool margin = true;
    int gap = SmartBoxSizer::DEFAULT_SIZE;
};

/// Item-level options — `proportion` and whether the item should
/// fill the cross axis. Per-item border is owned by `SmartBoxSizer`
/// so callers don't pass it.
struct LayoutItemOptions final {
    int proportion = 0;
    bool expand = true;
};

/// Templated layout helper that can extend any wxWindow-derived class.
/// Builds a tree of `SmartBoxSizer`s through nested `hbox` / `vbox`
/// blocks. The root sizer is a `SmartBoxSizer` with default options.
///
/// Usage:
///   class MyPanel : public Layout<wxPanel> { ... };
///   class MyDialog : public Layout<wxDialog> { ... };
template<SizerAware Base>
class Layout : public Base {
public:
    NO_COPY_AND_MOVE(Layout)

    /// Forward-construct the wx base, then install the root `SmartBoxSizer`.
    template<typename... Args>
    explicit Layout(Args&&... args)
    : Base(std::forward<Args>(args)...) {
        m_currentSizer = make_unowned<SmartBoxSizer>(SmartBoxSizer::Options {}, wxVERTICAL);
    }

    /// Platform default border — at least 5 px. Exposed so callers
    /// (and `SmartBoxSizer` itself, via `DEFAULT_PADDING`) agree on
    /// the same fallback value.
    static auto defaultBorder() -> int {
        return std::max(5, wxSizerFlags::GetDefaultBorder());
    }

    // -----------------------------------------------------------------------
    // Item insertion
    // -----------------------------------------------------------------------

    /// Add a window. Border / centring is handled by the active
    /// `SmartBoxSizer`; only proportion + expand are passed in.
    void add(wxWindow* view, const LayoutItemOptions opts = {}) {
        const int flags = opts.expand ? wxEXPAND : 0;
        m_currentSizer->Add(view, opts.proportion, flags);
    }

    /// Add a child sizer. Same proportion / expand semantics as the
    /// window variant — the sub-sizer carries its own internal
    /// border / gap / centring via its own options.
    void add(wxSizer* sizer, const LayoutContainerOptions opts = {}) {
        const int flags = opts.expand ? wxEXPAND : 0;
        m_currentSizer->Add(sizer, opts.proportion, flags);
    }

    /// Add a separator line. Orientation is the cross axis of the
    /// active sizer.
    void separator(const LayoutItemOptions opts = {}) {
        const bool horizontal = m_currentSizer->GetOrientation() == wxHORIZONTAL;
        const auto line = make_unowned<wxStaticLine>(
            m_currentParent, wxID_STATIC,
            wxDefaultPosition, wxDefaultSize,
            horizontal ? wxVERTICAL : wxHORIZONTAL
        );
        add(line, opts);
    }

    /// Add an explicit pixel-sized gap. Niche — `SmartBoxSizer`
    /// already inserts the `gap` between visible items; reach for
    /// this only when you need an off-grid spacer.
    void spacer(const int size = SmartBoxSizer::DEFAULT_SIZE) {
        m_currentSizer->AddSpacer(size < 0 ? defaultBorder() : size);
    }

    // -----------------------------------------------------------------------
    // Controls
    // -----------------------------------------------------------------------

    /// Add a static text label.
    auto label(const wxString& str, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxStaticText> {
        const auto ctrl = make_unowned<wxStaticText>(m_currentParent, id, str, wxDefaultPosition, wxDefaultSize, style);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a checkbox bound to `value`.
    auto checkBox(bool& value, const wxString& str, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxCheckBox> {
        const auto ctrl = checkBox(str, opts, id, style);
        ctrl->SetValue(value);
        ctrl->Bind(wxEVT_CHECKBOX, [&](const wxCommandEvent& evt) {
            value = evt.IsChecked();
        });
        return ctrl;
    }

    /// Add a stand-alone checkbox without a bound variable.
    auto checkBox(const wxString& str, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxCheckBox> {
        const auto ctrl = make_unowned<wxCheckBox>(m_currentParent, id, str, wxDefaultPosition, wxDefaultSize, style);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a spin control bound to `value`.
    auto spinCtrl(int& value, const int minVal, const int maxVal, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxSpinCtrl> {
        const auto ctrl = spinCtrl(minVal, maxVal, opts, id, style);
        ctrl->SetValue(value);
        ctrl->Bind(wxEVT_SPINCTRL, [&](const wxSpinEvent& evt) {
            value = evt.GetInt();
        });
        return ctrl;
    }

    /// Add a spin control.
    auto spinCtrl(int minVal, int maxVal, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxSpinCtrl> {
        const auto ctrl = make_unowned<wxSpinCtrl>(
            m_currentParent, id, "",
            wxDefaultPosition, wxDefaultSize,
            wxSP_ARROW_KEYS | style, minVal, maxVal
        );
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a choice dropdown bound to `value`.
    auto choice(wxString& value, const wxArrayString& choices, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxChoice> {
        const auto ctrl = choice(choices, opts, id, style);
        const auto sel = ctrl->FindString(value);
        ctrl->SetSelection(sel != wxNOT_FOUND ? sel : 0);
        ctrl->Bind(wxEVT_CHOICE, [&](const wxCommandEvent& evt) {
            value = evt.GetString();
        });
        return ctrl;
    }

    /// Add a choice dropdown.
    auto choice(const wxArrayString& choices, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxChoice> {
        const auto ctrl = make_unowned<wxChoice>(m_currentParent, id, wxDefaultPosition, wxDefaultSize, choices, style);
        add(ctrl, opts);
        return ctrl;
    }

    /// Add a text field bound to `value`.
    auto textField(wxString& value, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxTextCtrl> {
        const auto ctrl = textField(opts, id, style);
        ctrl->SetValue(value);
        ctrl->Bind(wxEVT_TEXT, [&](const wxCommandEvent& evt) {
            value = evt.GetString();
        });
        return ctrl;
    }

    /// Add a text field.
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

    /// Add a radio button.
    auto radio(const wxString& str, const LayoutItemOptions opts = {}, const wxWindowID id = wxID_ANY, const long style = 0) -> Unowned<wxRadioButton> {
        const auto ctrl = make_unowned<wxRadioButton>(m_currentParent, id, str, wxDefaultPosition, wxDefaultSize, style);
        add(ctrl, opts);
        return ctrl;
    }

    /// Connect a label so clicking it focuses `control`.
    void connect(wxStaticText* label, wxControl* control) {
        label->Bind(wxEVT_LEFT_DOWN, [control](const auto&) {
            control->SetFocus();
        });
    }

    // -----------------------------------------------------------------------
    // HBOX / VBOX
    // -----------------------------------------------------------------------

    template<std::invocable Func>
    void hbox(const LayoutContainerOptions opts, Func&& func) {
        makeBox(wxEmptyString, wxHORIZONTAL, opts, std::forward<Func>(func));
    }

    template<std::invocable Func>
    void hbox(const wxString& title, const LayoutContainerOptions opts, Func&& func) {
        makeBox(title, wxHORIZONTAL, opts, std::forward<Func>(func));
    }

    template<std::invocable Func>
    void vbox(const LayoutContainerOptions opts, Func&& func) {
        makeBox(wxEmptyString, wxVERTICAL, opts, std::forward<Func>(func));
    }

    template<std::invocable Func>
    void vbox(const wxString& title, const LayoutContainerOptions opts, Func&& func) {
        makeBox(title, wxVERTICAL, opts, std::forward<Func>(func));
    }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /// Active sizer (top of the implicit stack). The DSL guarantees
    /// this is a `SmartBoxSizer`, but the static type is the base for
    /// uniform interaction with `wxSizer` APIs.
    [[nodiscard]] auto currentSizer() const -> wxBoxSizer* { return m_currentSizer; }

    /// Active wx parent for new controls — defaults to `this`, but
    /// becomes the static box's content window inside titled
    /// `vbox(title, ...)` / `hbox(title, ...)`.
    [[nodiscard]] auto currentParent() const -> wxWindow* { return m_currentParent; }

private:
    /// Push a new `SmartBoxSizer` (optionally inside a static box),
    /// run `func` with it as the active sizer, then pop.
    template<std::invocable Func>
    void makeBox(const wxString& title, const int direction, const LayoutContainerOptions opts, Func&& func) {
        const ValueRestorer restoreState { m_currentSizer, m_currentParent };

        const SmartBoxSizer::Options smartOpts {
            .gap = opts.gap,
            .alignment = opts.alignment,
            .margin = opts.margin
        };

        const auto smart = make_unowned<SmartBoxSizer>(
            smartOpts, static_cast<wxOrientation>(direction)
        );

        if (title.empty()) {
            add(smart, opts);
        } else {
            // Wrap in a wxStaticBoxSizer; the SmartBoxSizer goes
            // inside as the static box's content sizer.
            wxStaticBoxSizer* const staticSizer = make_unowned<wxStaticBoxSizer>(
                direction, m_currentParent, title
            );
            staticSizer->Add(smart, 1, wxEXPAND);
            m_currentParent = staticSizer->GetStaticBox();
            add(staticSizer, opts);
        }

        m_currentSizer = smart;
        std::invoke(std::forward<Func>(func));
    }

    wxBoxSizer* m_currentSizer = nullptr; ///< Active SmartBoxSizer (held as base).
    wxWindow* m_currentParent = this;     ///< Active wx parent for new controls.
};

} // namespace fbide
