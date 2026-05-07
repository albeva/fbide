//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "EditorTestShim.hpp"

using namespace fbide::tests;

TEST(EditorTests, InsertAndReadBack) {
    EditorTestShim shim;
    shim.setText("Hello, FBIde");
    EXPECT_EQ(shim.getText(), "Hello, FBIde");
}

TEST(EditorTests, EmptyByDefault) {
    const EditorTestShim shim;
    EXPECT_EQ(shim.getText(), "");
}

TEST(EditorTests, MultilineRoundTrip) {
    EditorTestShim shim;
    shim.setText("Sub Foo\n    Print 1\nEnd Sub\n");
    EXPECT_EQ(shim.getText(), "Sub Foo\n    Print 1\nEnd Sub\n");
    EXPECT_EQ(shim.editor().GetLineCount(), 4);
}

TEST(EditorTests, CaseTransformOnWordBoundary) {
    EditorTestShim shim;
    shim.run([&] {
        shim.typeText("dim x as integer ");
    });
    EXPECT_EQ(shim.getText(), "DIM x AS Integer ");
}

TEST(EditorTests, CaseTransformIdentifierUntouched) {
    EditorTestShim shim;
    shim.run([&] {
        shim.typeText("myvar = 1 ");
    });
    EXPECT_EQ(shim.getText(), "myvar = 1 ");
}

TEST(EditorTests, CaseTransformOperatorKeywordLowered) {
    EditorTestShim shim;
    shim.run([&] {
        shim.typeText("x AND y ");
    });
    EXPECT_EQ(shim.getText(), "x and y ");
}

TEST(EditorTests, CaseTransformAcrossNewline) {
    EditorTestShim shim;
    shim.run([&] {
        shim.typeText("dim x as integer ");
    });
    EXPECT_EQ(shim.getText(), "DIM x AS Integer ");
}
