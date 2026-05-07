//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "EditorTestShim.hpp"

using namespace fbide::tests;

// TEST(EditorTests, InsertAndReadBack) {
//     EditorTestShim shim;
//     shim.setText("Hello, FBIde");
//     EXPECT_EQ(shim.getText(), "Hello, FBIde");
// }
//
// TEST(EditorTests, EmptyByDefault) {
//     EditorTestShim shim;
//     EXPECT_EQ(shim.getText(), "");
// }
//
// TEST(EditorTests, MultilineRoundTrip) {
//     EditorTestShim shim;
//     shim.setText("Sub Foo\n    Print 1\nEnd Sub\n");
//     EXPECT_EQ(shim.getText(), "Sub Foo\n    Print 1\nEnd Sub\n");
//     EXPECT_EQ(shim.editor().GetLineCount(), 4); // last \n splits into a trailing empty line
// }
//
// TEST(EditorTests, CaseTransformOnWordBoundary) {
//     // keywords.ini test config:
//     //   Keywords      → Upper (dim, as)
//     //   KeywordTypes  → Mixed (integer)
//     // The trailing space at end of input is the word boundary that
//     // triggers the transform on the last word ("integer").
//     EditorTestShim shim;
//     shim.typeText("dim x as integer ");
//     EXPECT_EQ(shim.getText(), "DIM x AS Integer ");
// }
//
// TEST(EditorTests, CaseTransformIdentifierUntouched) {
//     // Plain identifiers are not in any keyword group and stay as typed.
//     EditorTestShim shim;
//     shim.typeText("myVar = 1 ");
//     EXPECT_EQ(shim.getText(), "myVar = 1 ");
// }
//
// TEST(EditorTests, CaseTransformOperatorKeywordLowered) {
//     // KeywordOperators → Lower in the test config.
//     EditorTestShim shim;
//     shim.typeText("x AND y ");
//     EXPECT_EQ(shim.getText(), "x and y ");
// }

TEST(EditorTests, CaseTransformAcrossNewline) {
    // Newline is a word boundary; the preceding word transforms before
    // the newline lands. Trailing space closes the second word.
    const auto shim = fbide::make_unowned<EditorTestShim>();
    shim->callAfter([shim] {
        shim->typeText("dim x\nas integer ");
        EXPECT_EQ(shim->getText(), "DIM x\nAS Integer ");
    });
    wxYield();
}
