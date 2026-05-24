//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/Patch.hpp"
using namespace fbide;
using namespace fbide::ai;

// ---------------------------------------------------------------------------
// Plain matches — no trailing-newline retry needed.
// ---------------------------------------------------------------------------

TEST(FindPatchMatch, MatchAtStart) {
    const auto match = findPatchMatch("hello world", "hello", "hi");
    EXPECT_EQ(0, match.offset);
    EXPECT_EQ(5, match.length);
    EXPECT_EQ("hi", match.replacement);
}

TEST(FindPatchMatch, MatchInMiddle) {
    const auto match = findPatchMatch("abc target xyz", "target", "Target");
    EXPECT_EQ(4, match.offset);
    EXPECT_EQ(6, match.length);
}

TEST(FindPatchMatch, MatchAtEnd) {
    const auto match = findPatchMatch("prefix end", "end", "fin");
    EXPECT_EQ(7, match.offset);
    EXPECT_EQ(3, match.length);
}

TEST(FindPatchMatch, NoMatchReturnsNegativeOffset) {
    const auto match = findPatchMatch("hello world", "missing", "x");
    EXPECT_LT(match.offset, 0);
}

TEST(FindPatchMatch, MatchIsCaseSensitive) {
    const auto match = findPatchMatch("Hello", "hello", "x");
    EXPECT_LT(match.offset, 0);
}

TEST(FindPatchMatch, EmptySourceWithNonEmptySearchReturnsNotFound) {
    const auto match = findPatchMatch("", "anything", "x");
    EXPECT_LT(match.offset, 0);
}

TEST(FindPatchMatch, EmptySearchReturnsNotFound) {
    // Defensive: an empty search would otherwise match at offset 0 and
    // happily replace nothing — not what the user means by an empty
    // proposal.
    const auto match = findPatchMatch("hello", "", "x");
    EXPECT_LT(match.offset, 0);
}

// ---------------------------------------------------------------------------
// Trailing-newline fallback — the case from the original AiChatView
// implementation: a model emits a SEARCH block ending with `\n` but the
// matching line in the source has no final EOL.
// ---------------------------------------------------------------------------

TEST(FindPatchMatch, TrailingNewlineMatchesAsIs) {
    // When the source DOES contain the trailing newline, no retry is
    // needed. Length includes the newline.
    const auto match = findPatchMatch("first line\nsecond line\n", "first line\n", "FIRST\n");
    EXPECT_EQ(0, match.offset);
    EXPECT_EQ(11, match.length); // "first line\n" = 11 bytes
    EXPECT_EQ("FIRST\n", match.replacement);
}

TEST(FindPatchMatch, TrailingNewlineRetryMatchesWhenSourceLacksFinalEol) {
    // Source has no final \n; the SEARCH block has one. Should match
    // the trimmed form and report the trimmed replacement.
    const auto match = findPatchMatch("last line no eol", "last line no eol\n", "replaced\n");
    EXPECT_EQ(0, match.offset);
    EXPECT_EQ(16, match.length);              // length is of the trimmed search
    EXPECT_EQ("replaced", match.replacement); // \n trimmed off the replacement too
}

TEST(FindPatchMatch, TrailingNewlineRetryTrimsReplacementOnlyWhenItAlsoEndsWithNewline) {
    // Original behaviour: trim search always; trim replacement only when
    // it ALSO ends with \n. A replacement without trailing \n stays
    // unchanged on the retry.
    const auto match = findPatchMatch("last line", "last line\n", "replaced");
    EXPECT_EQ(0, match.offset);
    EXPECT_EQ(9, match.length);
    EXPECT_EQ("replaced", match.replacement);
}

TEST(FindPatchMatch, TrailingNewlineRetryFailsWhenEvenTrimmedFormDoesNotMatch) {
    const auto match = findPatchMatch("a different line", "missing\n", "x\n");
    EXPECT_LT(match.offset, 0);
}

TEST(FindPatchMatch, NoTrailingNewlineNoRetry) {
    // Search without trailing \n — no fallback path.
    const auto match = findPatchMatch("abc", "xyz", "q");
    EXPECT_LT(match.offset, 0);
}

// ---------------------------------------------------------------------------
// Multi-line patches — the common SEARCH/REPLACE shape.
// ---------------------------------------------------------------------------

TEST(FindPatchMatch, MultilineSearchMatches) {
    const std::string source = "line one\nline two\nline three\n";
    const wxString search = "line one\nline two\n";
    const wxString replacement = "rewritten\n";
    const auto match = findPatchMatch(source, search, replacement);
    EXPECT_EQ(0, match.offset);
    EXPECT_EQ(static_cast<int>(search.utf8_string().size()), match.length);
    EXPECT_EQ("rewritten\n", match.replacement);
}
