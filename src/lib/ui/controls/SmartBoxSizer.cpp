//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SmartBoxSizer.hpp"
using namespace fbide;

SmartBoxSizer::SmartBoxSizer(const Options options, const wxOrientation orientation)
: wxBoxSizer(orientation)
, m_options(options) {}

auto SmartBoxSizer::CalcMin() -> wxSize {
    applyAutoLayout();
    return wxBoxSizer::CalcMin();
}

auto SmartBoxSizer::defaultSize(const int value) -> int {
    if (value == -1) {
        return std::max(DEFAULT_SIZE, wxSizerFlags::GetDefaultBorder());
    }
    return value;
}

void SmartBoxSizer::applyAutoLayout() {
    auto visible = GetChildren() | std::views::filter(&wxSizerItem::IsShown);
    if (visible.empty()) {
        return;
    }

    const bool isHorizontal = GetOrientation() == wxHORIZONTAL;
    const auto gap = resolvedGap();

    auto iter = visible.begin();
    const auto end = visible.end();
    while (iter != end) {
        const auto isFirst = iter == visible.begin();
        auto* child = *iter++;
        const auto isLast = iter == end;

        int flags = child->GetFlag() & ~static_cast<int>(wxALL);
        if (gap != 0) {
            if (!isFirst || m_options.margin) {
                flags |= isHorizontal ? wxLEFT : wxTOP;
            }
            if (isLast && m_options.margin) {
                flags |= isHorizontal ? wxRIGHT : wxBOTTOM;
            }
            if (m_options.margin) {
                flags |= isHorizontal ? wxTOP | wxBOTTOM : wxLEFT | wxRIGHT;
            }
        }

        child->SetFlag(withAlignment(flags));
        child->SetBorder(gap);
    }
}

auto SmartBoxSizer::withAlignment(int flags) const -> int {
    if (m_options.alignment == Alignment::None) {
        return flags;
    }

    if ((flags & wxEXPAND) != 0) {
        return flags;
    }

    flags &= ~(wxALIGN_RIGHT | wxALIGN_BOTTOM | wxALIGN_CENTER);
    const bool isHorizontal = GetOrientation() == wxHORIZONTAL;

    switch (m_options.alignment) {
    case Alignment::Leading:
        flags |= isHorizontal ? wxALIGN_LEFT : wxALIGN_TOP;
        break;
    case Alignment::Center:
        flags |= isHorizontal ? wxALIGN_CENTER_VERTICAL : wxALIGN_CENTER_HORIZONTAL;
        break;
    case Alignment::Trailing:
        flags |= isHorizontal ? wxALIGN_BOTTOM : wxALIGN_RIGHT;
        break;
    case Alignment::None:
        break;
    }

    return flags;
}
