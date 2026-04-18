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
enum class LangId : int;

/// Settings panel base — extends Layout<wxPanel> with Context and LangId support.
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
    void makeTitle(LangId langId);

    /// LangId overloads — delegate to Layout's string-based methods.
    auto text(LangId langId, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxStaticText>;
    auto checkBox(bool& value, LangId langId, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxCheckBox>;
    auto checkBox(LangId langId, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxCheckBox>;
    auto spinCtrl(int& value, LangId langId, int minVal, int maxVal, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxSpinCtrl>;
    auto spinCtrl(LangId langId, int minVal, int maxVal, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxSpinCtrl>;
    auto button(LangId langId, LayoutItemOptions opts = {}, wxWindowID id = wxID_ANY, long style = 0) -> Unowned<wxButton>;

    // Pull in Layout's string-based overloads (otherwise hidden by the LangId ones).
    using Layout::button;
    using Layout::checkBox;
    using Layout::choice;
    using Layout::label;
    using Layout::spinCtrl;
    using Layout::textField;

private:
    Context& m_ctx;
};

} // namespace fbide
