//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// End-to-end smoke tests for the change-tracking margin. Labelled `gui`
// by the ui-tests target — on macOS these are excluded by
// `ctest -LE gui` (flaky there); the pure-logic coverage lives in
// `LineHistoryTests` and runs on every platform.
//
#include <gtest/gtest.h>
#include "EditorTestShim.hpp"

using namespace fbide;
using namespace fbide::tests;

namespace {

auto markerMask(Editor& editor, const int line) -> int {
    return editor.MarkerGet(line);
}

auto hasAddedMarker(Editor& editor, const int line) -> bool {
    return (markerMask(editor, line) & (1 << Editor::kAddedMarker)) != 0;
}

auto hasModifiedMarker(Editor& editor, const int line) -> bool {
    return (markerMask(editor, line) & (1 << Editor::kModifiedMarker)) != 0;
}

} // namespace

TEST(EditorChangeMarginTests, TypingCharMarksLineModified) {
    EditorTestShim shim;
    shim.setText("alpha\nbeta\ngamma\n");
    shim.editor().SetSavePoint(); // baseline snapshot
    shim.editor().GotoPos(shim.editor().PositionFromLine(1));
    shim.run([&] { shim.typeText("x"); });

    EXPECT_TRUE(hasModifiedMarker(shim.editor(), 1));
    EXPECT_FALSE(hasAddedMarker(shim.editor(), 1));
    EXPECT_FALSE(hasModifiedMarker(shim.editor(), 0));
    EXPECT_FALSE(hasModifiedMarker(shim.editor(), 2));
}

TEST(EditorChangeMarginTests, BackspaceRevertingTextClearsMarker) {
    EditorTestShim shim;
    shim.setText("alpha\n");
    shim.editor().SetSavePoint();
    shim.editor().GotoPos(shim.editor().GetLineEndPosition(0));
    shim.run([&] {
        shim.typeText("x"); // line 0 now "alphax"
    });
    EXPECT_TRUE(hasModifiedMarker(shim.editor(), 0));

    shim.run([&] {
        wxUIActionSimulator sim;
        sim.Char(WXK_BACK);
        wxYield();
        wxMilliSleep(10);
    });

    EXPECT_FALSE(hasModifiedMarker(shim.editor(), 0))
        << "reverting to saved text should clear the modified marker";
    EXPECT_FALSE(hasAddedMarker(shim.editor(), 0));
}

TEST(EditorChangeMarginTests, PressingEnterMarksNewLineAdded) {
    EditorTestShim shim;
    shim.setText("alpha\nbeta\n");
    shim.editor().SetSavePoint();
    // Put the caret at the end of "alpha"
    shim.editor().GotoPos(shim.editor().GetLineEndPosition(0));
    shim.run([&] { shim.typeText("\n"); });

    // After Enter at end of line 0: a new line appears at index 1; the
    // old "beta" shifted to index 2.
    EXPECT_FALSE(hasAddedMarker(shim.editor(), 0));
    EXPECT_FALSE(hasModifiedMarker(shim.editor(), 0));
    EXPECT_TRUE(hasAddedMarker(shim.editor(), 1));
    EXPECT_FALSE(hasModifiedMarker(shim.editor(), 1));
    EXPECT_FALSE(hasAddedMarker(shim.editor(), 2));
    EXPECT_FALSE(hasModifiedMarker(shim.editor(), 2));
}

TEST(EditorChangeMarginTests, SavePointReachedClearsAllMarkers) {
    EditorTestShim shim;
    shim.setText("alpha\nbeta\n");
    shim.editor().SetSavePoint();
    shim.editor().GotoPos(shim.editor().PositionFromLine(0));
    shim.run([&] { shim.typeText("x"); });
    EXPECT_TRUE(hasModifiedMarker(shim.editor(), 0));

    // Re-baseline as if the user saved.
    shim.editor().SetSavePoint();
    EXPECT_FALSE(hasModifiedMarker(shim.editor(), 0));
    EXPECT_FALSE(hasAddedMarker(shim.editor(), 0));
}
