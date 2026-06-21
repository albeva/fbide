//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Parse the `FBDEF <name>` lines emitted by the fbc-defines probe stub
/// (resources/ide/fbc-defines.bas) into a set of lowercased define names.
///
/// Each `#print FBDEF <name>` produces a line containing the marker `FBDEF`
/// followed by one predefined symbol; the symbol after the marker is taken and
/// any compiler-added prefix (a source location) or trailing tokens are ignored.
/// Lines without the marker are skipped. Names are lowercased to match
/// FreeBASIC's case-insensitive symbols.
[[nodiscard]] auto parseFbcDefines(const wxArrayString& lines) -> std::unordered_set<std::string>;

} // namespace fbide
