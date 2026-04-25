//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "StyledSource.hpp"
#include <wx/stc/stc.h>
using namespace fbide::lexer;

auto WxStcStyledSource::length() const -> Sci_PositionU {
    return static_cast<Sci_PositionU>(m_stc.GetLength());
}

auto WxStcStyledSource::styleAt(const Sci_PositionU pos) const -> ThemeCategory {
    return static_cast<ThemeCategory>(m_stc.GetStyleAt(static_cast<int>(pos)));
}

void WxStcStyledSource::getCharRange(char* buffer, const Sci_PositionU pos, const Sci_PositionU len) const {
    // GetTextRangeRaw returns UTF-8 directly from Scintilla — no wxString conversion alloc.
    const auto raw = m_stc.GetTextRangeRaw(static_cast<int>(pos), static_cast<int>(pos + len));
    std::memcpy(buffer, raw.data(), len);
}

auto WxStcStyledSource::lineFromPosition(const Sci_PositionU pos) const -> Sci_Position {
    return m_stc.LineFromPosition(static_cast<int>(pos));
}

auto WxStcStyledSource::lineState(const Sci_Position line) const -> FBSciLexer::LineState {
    return FBSciLexer::LineState::fromInt(m_stc.GetLineState(static_cast<int>(line)));
}
