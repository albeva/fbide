//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide::ai {

/// How `findPatchMatch` arrived at its match. Lets callers report
/// finer-grained feedback to the model — e.g. "matched on whitespace
/// normalisation" is a hint that the model's SEARCH text isn't quite
/// byte-accurate and a retry on the next round might want to be more
/// careful.
enum class MatchKind : std::uint8_t {
    Exact,                ///< Byte-for-byte exact match.
    TrimmedNewline,       ///< Matched after stripping a trailing `\n` from search.
    NormalizedWhitespace, ///< Matched after collapsing leading/trailing whitespace per line.
};

/// Result of `findPatchMatch`.
///
/// `offset` is the byte offset into the source buffer where the SEARCH
/// text was located, or negative when not found. `length` spans the
/// matched run (the source bytes that will be replaced — covers
/// whichever variant matched). `replacement` is the substitution text
/// — possibly trimmed of its trailing newline if the trim happened on
/// the search. `kind` reports which match strategy succeeded.
struct PatchMatch {
    int offset = -1;                   ///< Byte offset into source; <0 = not found.
    int length = 0;                    ///< Length of matched run in source.
    wxString replacement;              ///< Text to substitute (possibly trimmed).
    MatchKind kind = MatchKind::Exact; ///< Which match strategy hit.
};

/// Locate `search` within `source` with three ordered fallbacks:
///
/// 1. Exact, case-sensitive, byte-wise — what the model emitted.
/// 2. Trailing-newline trim — covers a model-emitted SEARCH block
///    that ends with `\n` while the target line in source has no
///    final EOL. The replacement is also trimmed of its trailing
///    `\n` when present so the source's EOL handling stays intact.
/// 3. Whitespace-normalised line match — splits both `search` and
///    `source` by lines, collapses leading/trailing whitespace on
///    each line, and looks for a contiguous run in source whose
///    normalised lines equal the normalised search lines. On success,
///    the source bytes spanning those lines are replaced verbatim
///    with `replacement`. Handles the common "model used 4 spaces,
///    file uses 2" / "model added/dropped a trailing space" misses.
///
/// Returns the first hit (with the appropriate `kind`) or
/// `{ .offset = -1 }`. An empty `search` is reported as not-found
/// defensively — otherwise it would silently match at offset 0 and
/// replace nothing.
[[nodiscard]] auto findPatchMatch(
    std::string_view source,
    const wxString& search,
    const wxString& replacement
) -> PatchMatch;

} // namespace fbide::ai
