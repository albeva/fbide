//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide::ai {

/// Result of `findPatchMatch`.
///
/// `offset` is the byte offset into the source buffer where the SEARCH
/// text was located, or negative when not found. `length` spans the
/// matched run (which may be the original or the trimmed-of-trailing-
/// newline form). `replacement` is the substitution text — possibly
/// trimmed of its trailing newline if the trim happened on the search.
struct PatchMatch {
    int offset = -1;      ///< Byte offset into source; <0 = not found.
    int length = 0;       ///< Length of matched run in source.
    wxString replacement; ///< Text to substitute (possibly trimmed).
};

/// Locate `search` within `source` (exact, case-sensitive, byte-wise)
/// with a single trailing-newline fallback.
///
/// The fallback covers a model-emitted SEARCH block that ends with `\n`
/// while the target line in the source has no final EOL: when the as-is
/// search misses and `search` ends with `\n`, retry once with both
/// `search` and `replacement` trimmed of their trailing newline (the
/// replacement is trimmed only when it also ends in `\n`). Returns the
/// match (with the trimmed replacement) or `{ .offset = -1 }`.
///
/// An empty `search` is reported as not-found defensively — otherwise it
/// would silently match at offset 0 and replace nothing.
[[nodiscard]] auto findPatchMatch(
    std::string_view source,
    const wxString& search,
    const wxString& replacement
) -> PatchMatch;

} // namespace fbide::ai
