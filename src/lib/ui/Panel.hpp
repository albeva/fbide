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
class Config;

/// Settings panel base — extends Layout<wxPanel> with Context access.
class Panel : public Layout<wxPanel> {
public:
    NO_COPY_AND_MOVE(Panel)

    Panel(Context& ctx, wxWindowID id, wxWindow* parent);
    virtual void create() = 0;
    virtual void apply() = 0;

protected:
    [[nodiscard]] auto getContext() const -> Context& { return m_ctx; }
    [[nodiscard]] auto getConfig() const -> Config&;

    /// Title + separator.
    void makeTitle(const wxString& labelText);

    /// Static text (synonym for Layout::label, avoids name clash with Layout::text if added).
    auto text(const wxString& labelText, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxStaticText>;

    /// Spin control with optional trailing label.
    auto spinCtrl(int& value, const wxString& labelText, int minVal, int maxVal, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxSpinCtrl>;
    auto spinCtrl(const wxString& labelText, int minVal, int maxVal, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxSpinCtrl>;

    // Pull in Layout's label/button/checkBox/etc overloads.
    using Layout::button;
    using Layout::checkBox;
    using Layout::choice;
    using Layout::label;
    using Layout::textField;

private:
    Context& m_ctx;
};

} // namespace fbide
