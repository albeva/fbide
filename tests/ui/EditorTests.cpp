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

// ---------------------------------------------------------------------------
// Auto-indent + closer insertion. Recreates issues from #46 / #48 / #50:
//   * Closer not inserted when block actually needs one.
//   * Closer NOT inserted twice when block already closed below.
//   * Closer line not de-indented past opener.
//   * Single-line `asm <stmt>` not treated as a block opener.
// ---------------------------------------------------------------------------

TEST(EditorTests, AutoIndentInsertsCloserForIfThen) {
    EditorTestShim shim;
    shim.run([&] {
        shim.typeText("if x then\n");
    });
    // Cursor lands on the indented body line; closer placed below at
    // opener's indent. Keyword case rules apply: keywords→Upper.
    EXPECT_EQ(shim.getText(), "IF x THEN\n    \nEND IF");
}

TEST(EditorTests, AutoIndentInsertsCloserForDoLoop) {
    EditorTestShim shim;
    shim.run([&] {
        shim.typeText("do\n");
    });
    EXPECT_EQ(shim.getText(), "DO\n    \nLOOP");
}

TEST(EditorTests, AutoIndentSkipsCloserWhenBlockAlreadyClosed) {
    // Pre-populate with a closed block. Pressing Enter at end of the
    // opener line must NOT inject a second `End If`.
    EditorTestShim shim;
    shim.editor().SetText("IF x THEN\nEND IF");
    shim.editor().GotoPos(shim.editor().GetLineEndPosition(0));
    shim.run([&] {
        shim.typeText("\n");
    });
    // One blank indented line inserted between opener and existing closer.
    EXPECT_EQ(shim.getText(), "IF x THEN\n    \nEND IF");
}

TEST(EditorTests, AutoIndentCloserStaysAtOpenerIndent) {
    // Issue #48: closer typed at body indent must dedent only to its
    // opener's indent, not all the way to column 0.
    EditorTestShim shim;
    shim.editor().SetText("    IF x THEN\n        END IF");
    shim.editor().GotoPos(shim.editor().GetLineEndPosition(1));
    shim.run([&] {
        shim.typeText("\n");
    });
    const auto text = shim.getText();
    EXPECT_TRUE(text.Contains("    IF x THEN\n    END IF"))
        << "got: " << text;
}

TEST(EditorTests, AutoIndentSingleLineAsmDoesNotOpenBlock) {
    // Issue #46: `asm <stmt>` is one line, not a block opener.
    EditorTestShim shim;
    shim.run([&] {
        shim.typeText("asm mov eax, 10\n");
    });
    const auto text = shim.getText();
    EXPECT_FALSE(text.Contains("END ASM")) << "got: " << text;
    EXPECT_FALSE(text.Contains("End Asm")) << "got: " << text;
}

TEST(EditorTests, AutoIndentBareAsmOpensBlock) {
    // `asm` alone on a line opens a multi-line block; closer expected.
    EditorTestShim shim;
    shim.run([&] {
        shim.typeText("asm\n");
    });
    EXPECT_EQ(shim.getText(), "ASM\n    \nEND ASM");
}

TEST(EditorTests, AutoIndentNestedCompoundStatements) {
    // Bug from session: typing nested openers (if → do → if) — first if
    // got no closer, do got it, inner if didn't. Each Enter must inject
    // the matching closer at the opener's indent level.
    EditorTestShim shim;
    shim.run([&] {
        shim.typeText("if x then\n");
        shim.typeText("do\n");
        shim.typeText("if z then\n");
    });
    EXPECT_EQ(shim.getText(),
        "IF x THEN\n"
        "    DO\n"
        "        IF z THEN\n"
        "            \n"
        "        END IF\n"
        "    LOOP\n"
        "END IF");
}

TEST(EditorTests, AutoIndentDoesNotDedentCloserAlreadyAtOpenerIndent) {
    // Issue: with closer already aligned with its opener, pressing Enter
    // on the closer line must NOT dedent it further (which previously
    // pushed END IF below column 0). Pre-populate via SetText so the
    // initial layout doesn't run through the auto-closer path; then
    // type only the trailing newline.
    EditorTestShim shim;
    shim.editor().SetText(
        "    IF x THEN\n"
        "    END IF");
    shim.editor().GotoPos(shim.editor().GetLineEndPosition(1));
    shim.run([&] {
        shim.typeText("\n");
    });
    // END IF stays at indent 4; the new line below inherits that indent.
    EXPECT_EQ(shim.getText(),
        "    IF x THEN\n"
        "    END IF\n"
        "    ");
}

TEST(EditorTests, AutoIndentDedentsOverIndentedCloser) {
    // Counterpart to above: when the closer is one level too deep, Enter
    // dedents it back to the opener's indent.
    EditorTestShim shim;
    shim.editor().SetText(
        "    IF x THEN\n"
        "        END IF");
    shim.editor().GotoPos(shim.editor().GetLineEndPosition(1));
    shim.run([&] {
        shim.typeText("\n");
    });
    EXPECT_EQ(shim.getText(),
        "    IF x THEN\n"
        "    END IF\n"
        "    ");
}
