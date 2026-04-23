//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <span>
#include <string_view>

namespace fbide::indent {

/// Indent decision for the line that follows `prevLine`.
struct Decision {
    /// Indent delta (in levels) for the new line, applied on top of
    /// `prevLine`'s indentation (after `dedentPrev` is honoured).
    ///   +1 = open block, indent one level deeper
    ///    0 = continue at the same indent
    ///   -1 = unused for now
    int deltaLevels = 0;

    /// True when `prevLine` is itself a block closer or mid-block keyword
    /// (`End If`, `Loop`, `Else`, `Case`, ...) and should be dedented one
    /// level relative to its current indent before placing the new line.
    bool dedentPrev = false;

    /// Canonical closing keyword(s) to auto-insert below when `prevLine`
    /// opens a block. Each element is a single keyword, lowercase, joined
    /// with single spaces by the renderer. Empty span = no closer.
    /// Examples: { "loop" }, { "next" }, { "end", "if" }, { "end", "sub" }.
    /// Storage lives in static `constexpr` arrays — caller must not retain
    /// the span past the lifetime of the program.
    std::span<const std::string_view> closerKeywords;
};

/// Compute the indent decision triggered by pressing Enter at the end of
/// `prevLine`. Pure function — no Editor / Context dependency. Tokenises
/// `prevLine` via the existing `fbide::lexer::Lexer` (with empty keyword
/// groups; structural classification works through the lexer's hardcoded
/// fallback table).
[[nodiscard]] auto decide(const wxString& prevLine) -> Decision;

} // namespace fbide::indent
