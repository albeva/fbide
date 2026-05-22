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
, m_options(options) {
    // If we have a margin, create a nested sizer.
    if (const auto margin = resolvedBorder(); margin != 0) {
        m_inner = new wxBoxSizer(orientation);
        m_innerItem = wxBoxSizer::Add(
            m_inner,
            wxSizerFlags(1).Expand().Border(wxALL, margin)
        );
    }
    m_constructing = false;
}

auto SmartBoxSizer::DoInsert(const std::size_t index, wxSizerItem* item) -> wxSizerItem* {
    if (!m_constructing) {
        item->SetBorder(0);
        item->SetFlag(item->GetFlag() & ~static_cast<int>(wxALL));
        if (m_inner != nullptr) {
            return m_inner->Insert(index, item);
        }
    }
    return wxBoxSizer::DoInsert(index, item);
}

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
    const auto& children = m_inner != nullptr ? m_inner->GetChildren() : GetChildren();
    auto items = children | std::views::filter(&wxSizerItem::IsShown);
    if (items.empty()) {
        return;
    }

    const bool isHorizontal = GetOrientation() == wxHORIZONTAL;
    const auto gap = resolvedGap();

    bool isFirst = true;
    for (auto* item : items) {
        int flags = item->GetFlag() & ~static_cast<int>(wxALL);

        if (isFirst) {
            isFirst = false;
        } else if (gap != 0) {
            flags |= isHorizontal ? wxLEFT : wxTOP;
        }

        item->SetFlag(withAlignment(flags));
        item->SetBorder(gap);
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
