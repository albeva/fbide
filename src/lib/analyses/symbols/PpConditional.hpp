//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "analyses/lexer/Token.hpp"

namespace fbide {

/// Tri-state result of evaluating a preprocessor conditional.
enum class PpEval : std::uint8_t { False,
    True,
    Unknown };

/// Evaluate a preprocessor conditional opener — `#if` / `#ifdef` / `#ifndef` /
/// `#elseif` / `#elseifdef` / `#elseifndef` — against the set of currently
/// defined symbol names.
///
/// Only `defined(x)`, a bare symbol `x` (treated as a defined-check), and the
/// boolean operators `and` / `or` / `not` with parentheses are understood. Value
/// checks, comparisons, arithmetic, or any unrecognised token yield `Unknown`,
/// so the caller can conservatively keep the branch rather than wrongly drop it.
///
/// `defines` holds lowercased names; matching is case-insensitive (FreeBASIC
/// symbols are case-insensitive). `#else` carries no condition and is not passed.
[[nodiscard]] auto evaluatePpCondition(
    const std::vector<lexer::Token>& opener,
    const std::unordered_set<std::string>& defines
) -> PpEval;

} // namespace fbide
