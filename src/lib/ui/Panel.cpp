//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Panel.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
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

auto Panel::tr(const wxString& path) const -> wxString {
    return m_ctx.getConfigManager().locale_or(path, "");
}

void Panel::makeTitle(const wxString& labelText) {
    text(labelText);
    separator();
}

auto Panel::text(const wxString& labelText, const LayoutItemOptions opts, const wxWindowID id, const long style) -> Unowned<wxStaticText> {
    return label(labelText, opts, id, style);
}

auto Panel::spinCtrl(int& value, const wxString& labelText, const int minVal, const int maxVal, const LayoutItemOptions opts, const wxWindowID id, const long style) -> Unowned<wxSpinCtrl> {
    if (labelText.empty()) {
        return Layout::spinCtrl(value, minVal, maxVal, opts, id, style);
    }

    Unowned<wxSpinCtrl> spin;
    hbox({ .proportion = opts.proportion,
             .expand = opts.expand,
             .space = opts.space,
             .padding = opts.padding,
             .center = true,
             .border = 0 },
        [&] {
            spin = Layout::spinCtrl(value, minVal, maxVal, {}, id, style);
            const auto lbl = text(labelText, { .expand = false });
            connect(lbl, spin);
        });
    return spin;
}

auto Panel::spinCtrl(const wxString& labelText, const int minVal, const int maxVal, const LayoutItemOptions opts, const wxWindowID id, const long style) -> Unowned<wxSpinCtrl> {
    if (labelText.empty()) {
        return Layout::spinCtrl(minVal, maxVal, opts, id, style);
    }

    Unowned<wxSpinCtrl> spin;
    hbox({ .proportion = opts.proportion,
             .expand = opts.expand,
             .space = opts.space,
             .padding = opts.padding,
             .center = true,
             .border = 0 },
        [&] {
            spin = Layout::spinCtrl(minVal, maxVal, {}, id, style);
            const auto lbl = text(labelText, { .expand = false });
            connect(lbl, spin);
        });
    return spin;
}
