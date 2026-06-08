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
    /// Validate the pending edits without committing them. Returns
    /// `true` when the panel is valid; `false` reports a validation
    /// error and asks `SettingsDialog` to keep the dialog open and
    /// select this panel. Called for every panel before any `apply()`,
    /// so a failure on one tab commits nothing on any tab. Default is a
    /// no-op that always succeeds; panels with required fields override.
    virtual auto validate() -> bool { return true; }
    /// Commit edits back into config — subclasses implement. Only
    /// called after every panel's `validate()` has passed, so it may
    /// assume its input is valid and must not fail.
    virtual void apply() = 0;
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
