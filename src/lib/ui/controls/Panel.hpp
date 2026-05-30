//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Layout.hpp"

namespace fbide {
class Context;

/// Settings panel base — extends Layout<wxPanel> with Context access.
class Panel : public Layout<wxPanel> {
public:
    NO_COPY_AND_MOVE(Panel)

    /// Construct as a child of `parent` with `id`; subclass `create()` builds widgets.
    Panel(Context& ctx, wxWindowID id, wxWindow* parent);
    /// Build the panel widgets — subclasses implement.
    virtual void create() = 0;
    /// Commit edits back into config — subclasses implement. Returns
    /// `true` on success; `false` reports a validation error and asks
    /// `SettingsDialog` to keep the dialog open and select this panel.
    virtual auto apply() -> bool = 0;
    /// Discard any pending edits — called by `SettingsDialog` when the
    /// user clicks Cancel or closes the dialog via the window control.
    /// Default is a no-op; panels that mutate global state during
    /// editing (e.g. the compiler-config catalog) override to restore.
    virtual void cancel() {}

    /// Move keyboard focus to a sub-location within this panel,
    /// addressed by a slash-delimited `path` (the remainder after the
    /// page segment of a settings deep-link). Default is a no-op;
    /// panels with addressable fields override. Called by
    /// `SettingsDialog` after the target tab is shown.
    virtual void focusPath(const wxString& /*path*/) {}

protected:
    /// Access the application context.
    [[nodiscard]] auto getContext() const -> Context& { return m_ctx; }

    /// Add a section title plus a horizontal separator.
    void makeTitle(const wxString& labelText);

    /// Static text (synonym for `Layout::label`, avoids a name clash with any future `Layout::text`).
    auto text(const wxString& labelText, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxStaticText>;

    /// Spin control bound to `value` with a trailing static label.
    auto spinCtrl(int& value, const wxString& labelText, int minVal, int maxVal, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxSpinCtrl>;
    /// Spin control with a trailing static label and no bound variable.
    auto spinCtrl(const wxString& labelText, int minVal, int maxVal, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxSpinCtrl>;

    // Pull in Layout's label/button/checkBox/etc overloads.
    using Layout::button;
    using Layout::checkBox;
    using Layout::choice;
    using Layout::label;
    using Layout::textField;

private:
    Context& m_ctx; ///< Application context.
};

} // namespace fbide
