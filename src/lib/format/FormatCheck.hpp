//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Safety net run before a formatted result replaces its source. Formatting may
/// change layout (whitespace, line breaks), ASCII letter case, and drop `:`
/// statement separators it split onto new lines — nothing else. Returns false
/// when `formatted` is not valid UTF-8 or its remaining text differs from
/// `source`, so the caller keeps the original instead of losing code.
///
/// Both strings are UTF-8 bytes, as produced by the lexer / renderer.
[[nodiscard]] auto isSafeFormatResult(std::string_view source, std::string_view formatted) -> bool;

} // namespace fbide
