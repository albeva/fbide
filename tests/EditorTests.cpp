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
    EditorTestShim shim;
    EXPECT_EQ(shim.getText(), "");
}

TEST(EditorTests, MultilineRoundTrip) {
    EditorTestShim shim;
    shim.setText("Sub Foo\n    Print 1\nEnd Sub\n");
    EXPECT_EQ(shim.getText(), "Sub Foo\n    Print 1\nEnd Sub\n");
    EXPECT_EQ(shim.editor().GetLineCount(), 4); // last \n splits into a trailing empty line
}
