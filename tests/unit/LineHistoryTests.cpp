//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <random>
#include <gtest/gtest.h>
#include "editor/LineHistory.hpp"

using namespace fbide;

namespace {

/// Concise constructor for the per-line `wxString` snapshot — accepts
/// braced initializer lists in test bodies instead of explicit casts.
auto makeSnapshot(std::initializer_list<const char*> lines) -> std::vector<wxString> {
    std::vector<wxString> out;
    out.reserve(lines.size());
    for (const auto* line : lines) {
        out.emplace_back(line);
    }
    return out;
}

} // namespace

class LineHistoryTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Snapshot + stateOf (no edits)
// ---------------------------------------------------------------------------

TEST_F(LineHistoryTests, EmptySnapshotHasNoLines) {
    LineHistory history;
    history.snapshot({});
    EXPECT_EQ(history.lineCount(), 0);
}

TEST_F(LineHistoryTests, SnapshotInitialisesIdentityMapping) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta", "gamma" }));
    ASSERT_EQ(history.lineCount(), 3);
    EXPECT_EQ(history.stateOf(0, "alpha"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(1, "beta"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(2, "gamma"), LineHistory::State::Unchanged);
}

TEST_F(LineHistoryTests, DifferentTextOnSnapshottedLineIsModified) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta", "gamma" }));
    EXPECT_EQ(history.stateOf(1, "BETA"), LineHistory::State::Modified);
}

TEST_F(LineHistoryTests, RevertingTextToSavedReturnsToUnchanged) {
    // No edit machinery yet — just verify that `stateOf` compares against
    // the live text, so a caller writing back the saved value reads as
    // Unchanged again.
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta", "gamma" }));
    EXPECT_EQ(history.stateOf(0, "ALPHA"), LineHistory::State::Modified);
    EXPECT_EQ(history.stateOf(0, "alpha"), LineHistory::State::Unchanged);
}

TEST_F(LineHistoryTests, StateOfOutOfRangeIsUnchanged) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha" }));
    EXPECT_EQ(history.stateOf(-1, "alpha"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(99, "alpha"), LineHistory::State::Unchanged);
}

// ---------------------------------------------------------------------------
// applyInsert
// ---------------------------------------------------------------------------

TEST_F(LineHistoryTests, InsertInMiddleAddsLineAndShiftsOrigins) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta", "gamma" }));
    history.applyInsert(/*startLine=*/1, /*linesAdded=*/1);
    ASSERT_EQ(history.lineCount(), 4);
    EXPECT_EQ(history.stateOf(0, "alpha"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(1, "inserted"), LineHistory::State::Added);
    // The original "beta" shifted down one — still Unchanged at its new index.
    EXPECT_EQ(history.stateOf(2, "beta"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(3, "gamma"), LineHistory::State::Unchanged);
}

TEST_F(LineHistoryTests, InsertAtStartBumpsAllOrigins) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta" }));
    history.applyInsert(0, 1);
    ASSERT_EQ(history.lineCount(), 3);
    EXPECT_EQ(history.stateOf(0, "new"), LineHistory::State::Added);
    EXPECT_EQ(history.stateOf(1, "alpha"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(2, "beta"), LineHistory::State::Unchanged);
}

TEST_F(LineHistoryTests, InsertAtEndAppends) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta" }));
    history.applyInsert(2, 1);
    ASSERT_EQ(history.lineCount(), 3);
    EXPECT_EQ(history.stateOf(0, "alpha"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(1, "beta"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(2, "tail"), LineHistory::State::Added);
}

TEST_F(LineHistoryTests, InsertPastEndIsClampedToAppend) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha" }));
    history.applyInsert(99, 2);
    ASSERT_EQ(history.lineCount(), 3);
    EXPECT_EQ(history.stateOf(0, "alpha"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(1, "x"), LineHistory::State::Added);
    EXPECT_EQ(history.stateOf(2, "y"), LineHistory::State::Added);
}

TEST_F(LineHistoryTests, BulkInsertMarksEveryNewLineAdded) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta" }));
    history.applyInsert(1, 4); // simulate pasting four lines between alpha + beta
    ASSERT_EQ(history.lineCount(), 6);
    EXPECT_EQ(history.stateOf(0, "alpha"), LineHistory::State::Unchanged);
    for (int line = 1; line <= 4; line++) {
        EXPECT_EQ(history.stateOf(line, "anything"), LineHistory::State::Added);
    }
    EXPECT_EQ(history.stateOf(5, "beta"), LineHistory::State::Unchanged);
}

TEST_F(LineHistoryTests, InsertZeroLinesIsNoop) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha" }));
    history.applyInsert(0, 0);
    EXPECT_EQ(history.lineCount(), 1);
}

// ---------------------------------------------------------------------------
// applyDelete
// ---------------------------------------------------------------------------

TEST_F(LineHistoryTests, DeleteInMiddleShiftsTailUp) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta", "gamma" }));
    history.applyDelete(/*startLine=*/1, /*linesRemoved=*/1);
    ASSERT_EQ(history.lineCount(), 2);
    EXPECT_EQ(history.stateOf(0, "alpha"), LineHistory::State::Unchanged);
    // "gamma" preserves its origin — it just shifted up one slot.
    EXPECT_EQ(history.stateOf(1, "gamma"), LineHistory::State::Unchanged);
}

TEST_F(LineHistoryTests, DeleteThenReinsertSameTextIsAdded) {
    // The originIndex approach is intentionally not fooled into thinking
    // a re-inserted line came from the original — the dropped origin is
    // gone. We accept this; a save re-baselines.
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta" }));
    history.applyDelete(0, 1);
    history.applyInsert(0, 1);
    ASSERT_EQ(history.lineCount(), 2);
    EXPECT_EQ(history.stateOf(0, "alpha"), LineHistory::State::Added);
    EXPECT_EQ(history.stateOf(1, "beta"), LineHistory::State::Unchanged);
}

TEST_F(LineHistoryTests, InsertThenDeleteSameCountRestoresLineCount) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta" }));
    history.applyInsert(1, 3);
    history.applyDelete(1, 3);
    ASSERT_EQ(history.lineCount(), 2);
    EXPECT_EQ(history.stateOf(0, "alpha"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(1, "beta"), LineHistory::State::Unchanged);
}

TEST_F(LineHistoryTests, DeletePastEndIsClampedToAvailable) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta", "gamma" }));
    history.applyDelete(1, 99);
    ASSERT_EQ(history.lineCount(), 1);
    EXPECT_EQ(history.stateOf(0, "alpha"), LineHistory::State::Unchanged);
}

TEST_F(LineHistoryTests, DeleteFromEmptyIsNoop) {
    LineHistory history;
    history.snapshot({});
    history.applyDelete(0, 5);
    EXPECT_EQ(history.lineCount(), 0);
}

TEST_F(LineHistoryTests, DeleteZeroLinesIsNoop) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha" }));
    history.applyDelete(0, 0);
    EXPECT_EQ(history.lineCount(), 1);
}

// ---------------------------------------------------------------------------
// Combined behaviour + re-snapshot
// ---------------------------------------------------------------------------

TEST_F(LineHistoryTests, InsertedLinesStayAddedEvenAfterEdit) {
    // The origin of an inserted line is -1 regardless of what text we
    // later put on it — it didn't exist at snapshot time.
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha", "beta" }));
    history.applyInsert(1, 1);
    EXPECT_EQ(history.stateOf(1, "first text"), LineHistory::State::Added);
    // Same line, different text — still Added; not Modified.
    EXPECT_EQ(history.stateOf(1, "later edit"), LineHistory::State::Added);
}

TEST_F(LineHistoryTests, ReSnapshotResetsTheBaseline) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "alpha" }));
    history.applyInsert(1, 1);
    EXPECT_EQ(history.stateOf(1, "added line"), LineHistory::State::Added);
    // After save: re-snapshot with the new lines as the baseline.
    history.snapshot(makeSnapshot({ "alpha", "added line" }));
    EXPECT_EQ(history.stateOf(0, "alpha"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(1, "added line"), LineHistory::State::Unchanged);
}

TEST_F(LineHistoryTests, OriginIndicesShiftCorrectlyAcrossMixedEdits) {
    // Sequence: insert two lines at the top, then delete one of the
    // originals — verify the surviving origin still maps back to the
    // correct snapshot entry.
    LineHistory history;
    history.snapshot(makeSnapshot({ "a", "b", "c" }));
    history.applyInsert(0, 2); // [-1, -1,  0,  1,  2]
    history.applyDelete(3, 1); // [-1, -1,  0,  2]
    ASSERT_EQ(history.lineCount(), 4);
    EXPECT_EQ(history.stateOf(0, "x"), LineHistory::State::Added);
    EXPECT_EQ(history.stateOf(1, "y"), LineHistory::State::Added);
    EXPECT_EQ(history.stateOf(2, "a"), LineHistory::State::Unchanged);
    // The original "c" survived; the dropped "b" took its slot.
    EXPECT_EQ(history.stateOf(3, "c"), LineHistory::State::Unchanged);
    EXPECT_EQ(history.stateOf(3, "C"), LineHistory::State::Modified);
}

// ---------------------------------------------------------------------------
// Stress — many random ops keep `lineCount` consistent with the operations
// we issued. Acts as a sanity floor for the splice arithmetic.
// ---------------------------------------------------------------------------

TEST_F(LineHistoryTests, MixedEditStressKeepsLineCountConsistent) {
    LineHistory history;
    history.snapshot(makeSnapshot({ "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" }));
    int expected = 10;

    // Deterministic pseudo-random sequence so the test is reproducible.
    std::mt19937 rng(123456U);
    std::uniform_int_distribution<int> op(0, 1);

    for (int step = 0; step < 1000; step++) {
        if (op(rng) == 0 || expected == 0) {
            const int at = std::uniform_int_distribution<int>(0, expected)(rng);
            const int count = std::uniform_int_distribution<int>(1, 4)(rng);
            history.applyInsert(at, count);
            expected += count;
        } else {
            const int at = std::uniform_int_distribution<int>(0, expected - 1)(rng);
            const int maxCount = std::min(4, expected - at);
            const int count = std::uniform_int_distribution<int>(1, maxCount)(rng);
            history.applyDelete(at, count);
            expected -= count;
        }
        ASSERT_EQ(history.lineCount(), expected) << "step " << step;
    }
}
