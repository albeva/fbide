//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Panel.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Lang.hpp"
using namespace fbide;

Panel::Panel(Context& ctx, const wxWindowID id, wxWindow* parent)
: Layout(
      parent, id,
      wxDefaultPosition, wxDefaultSize,
      wxNO_BORDER | wxCLIP_CHILDREN | wxTAB_TRAVERSAL
  )
, m_ctx(ctx) {}

auto Panel::getConfig() const -> Config& {
    return m_ctx.getConfig();
}

void Panel::makeTitle(const LangId langId) {
    text(langId);
    separator();
}

auto Panel::text(const LangId langId, const LayoutItemOptions opts) -> Unowned<wxStaticText> {
    return label(m_ctx.getLang()[langId], opts);
}

auto Panel::checkBox(bool& value, const LangId langId, const LayoutItemOptions opts) -> Unowned<wxCheckBox> {
    return Layout::checkBox(value, m_ctx.getLang()[langId], opts);
}

auto Panel::checkBox(const LangId langId, const LayoutItemOptions opts) -> Unowned<wxCheckBox> {
    return Layout::checkBox(m_ctx.getLang()[langId], opts);
}

auto Panel::spinCtrl(int& value, const LangId langId, const int minVal, const int maxVal, const LayoutItemOptions opts) -> Unowned<wxSpinCtrl> {
    if (langId == LangId::EmptyString) {
        return Layout::spinCtrl(value, minVal, maxVal, opts);
    }
    return hbox({
        .proportion = opts.proportion,
        .expand = opts.expand,
        .space = opts.space,
        .padding = opts.padding,
        .center = true,
        .border = 0
    }, [&] {
        const auto spin = Layout::spinCtrl(value, minVal, maxVal, {});
        const auto lbl = text(langId, { .expand = false });
        connect(lbl, spin);
        return spin;
    });
}

auto Panel::spinCtrl(const LangId langId, const int minVal, const int maxVal, const LayoutItemOptions opts) -> Unowned<wxSpinCtrl> {
    if (langId == LangId::EmptyString) {
        return Layout::spinCtrl(minVal, maxVal, opts);
    }
    return hbox({}, [&] {
        const auto spin = Layout::spinCtrl(minVal, maxVal, { });
        text(langId, { });
        return spin;
    });
}

auto Panel::button(const LangId langId, const LayoutItemOptions opts) -> Unowned<wxButton> {
    return Layout::button(m_ctx.getLang()[langId], opts);
}
