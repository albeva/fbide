//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Token.hpp"

namespace fbide::lexer {

/// Scan the token stream for `' format off` / `' format on` pragma comments and
/// mark every token inside a region with `verbatim = true`.
///
/// Pragmas must be single-line comments (`'` or `REM` form) alone on their line;
/// body must match `^\s*format\s+(on|off)\s*$` case-insensitive. Nested pragmas
/// adjust a depth counter; tokens are verbatim iff any enclosing depth is > 0.
/// Unbalanced `on` is a no-op. Unbalanced `off` leaves the rest of the file
/// verbatim.
void annotateVerbatim(std::vector<Token>& tokens);

} // namespace fbide::lexer
