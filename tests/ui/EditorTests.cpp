//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "EditorTestShim.hpp"
#include "config/ThemeCategory.hpp"

using namespace fbide;
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

// ---------------------------------------------------------------------------
// Comment / uncomment must not change the selection. With no selection the
// caret simply follows the inserted/removed prefix — the whole line is NOT
// selected afterwards (#113).
// ---------------------------------------------------------------------------

TEST(EditorTests, CommentWithoutSelectionDoesNotSelectLine) {
    EditorTestShim shim;
    shim.setText("Print 1");
    auto& editor = shim.editor();
    editor.SetSelection(3, 3); // caret mid-line, empty selection
    editor.commentSelection();
    EXPECT_EQ(shim.getText(), "'Print 1");
    EXPECT_EQ(editor.GetSelectionStart(), editor.GetSelectionEnd());
}

TEST(EditorTests, UncommentWithoutSelectionDoesNotSelectLine) {
    EditorTestShim shim;
    shim.setText("'Print 1");
    auto& editor = shim.editor();
    editor.SetSelection(4, 4);
    editor.uncommentSelection();
    EXPECT_EQ(shim.getText(), "Print 1");
    EXPECT_EQ(editor.GetSelectionStart(), editor.GetSelectionEnd());
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

TEST(EditorTests, AutoIndentEnterMidLineLeavesCaretBeforeMovedContent) {
    // Repro: caret between "foo " and "bar"; pressing Enter must leave the
    // caret at the start of the moved content on the new line, not jump it
    // past the content to the line end. Non-opener line, no indent change.
    EditorTestShim shim;
    shim.editor().SetText("foo bar");
    shim.editor().GotoPos(4); // between "foo " and "bar"
    shim.run([&] {
        shim.typeText("\n");
    });
    EXPECT_EQ(shim.getText(), "foo \nbar");
    // Caret must land at start of "bar" (position 5: "foo \n" = 5 chars, then b),
    // not at end of "bar" (position 8).
    EXPECT_EQ(shim.editor().GetCurrentPos(), 5);
    EXPECT_EQ(shim.editor().GetCurrentLine(), 1);
    EXPECT_EQ(shim.editor().GetColumn(shim.editor().GetCurrentPos()), 0);
}

// ---------------------------------------------------------------------------
// Ctrl+click hotspots on `#include "path"`. The full openInclude flow needs
// a DocumentManager + SymbolTable; the shim runs without those, so these
// tests assert the prerequisite state instead — that the editor marks the
// include path (StringPP) clickable when Ctrl is held, AND that the lexer
// has actually painted the path bytes with that style.
// ---------------------------------------------------------------------------

TEST(EditorTests, CtrlEnablesHotspotOnIncludePathStyle) {
    EditorTestShim shim;
    shim.editor().SetText("#include \"foo.bi\"\n");
    shim.run([&] {
        wxUIActionSimulator sim;
        sim.KeyDown(WXK_CONTROL);
        wxYield();
    });
    // Pressing Ctrl flips hotspot styling on for the include path (StringPP)
    // so Ctrl+click on the quoted path fires `EVT_STC_HOTSPOT_CLICK`.
    EXPECT_TRUE(shim.editor().StyleGetHotSpot(+ThemeCategory::StringPP));
    // Every other PP-context and regular-code style stays non-clickable —
    // Ctrl+click off the path must not be hijacked by the include handler.
    EXPECT_FALSE(shim.editor().StyleGetHotSpot(+ThemeCategory::Preprocessor));
    EXPECT_FALSE(shim.editor().StyleGetHotSpot(+ThemeCategory::KeywordPP));
    EXPECT_FALSE(shim.editor().StyleGetHotSpot(+ThemeCategory::IdentifierPP));
    EXPECT_FALSE(shim.editor().StyleGetHotSpot(+ThemeCategory::Default));
    EXPECT_FALSE(shim.editor().StyleGetHotSpot(+ThemeCategory::Keywords));
    EXPECT_FALSE(shim.editor().StyleGetHotSpot(+ThemeCategory::String));
}

TEST(EditorTests, IncludePathBytesHaveClickableStyle) {
    // Regression for production bug: Ctrl+click on the quoted path was
    // inactive because StringPP wasn't on the hotspot list. The lexer
    // paints the path as StringPP, so once Ctrl enables the hotspot the
    // path bytes must report `StyleGetHotSpot == true`.
    EditorTestShim shim;
    const wxString src = "#include \"foo.bi\"\n";
    shim.editor().SetText(src);
    shim.run([&] {
        wxUIActionSimulator sim;
        sim.KeyDown(WXK_CONTROL);
        wxYield();
    });
    // Path span: bytes 9..16 covering `"foo.bi"`.
    for (int i = 9; i <= 16; i++) {
        const int style = shim.editor().GetStyleAt(i);
        EXPECT_EQ(style, +ThemeCategory::StringPP)
            << "byte " << i << " (`" << static_cast<char>(src[i]) << "`)";
        EXPECT_TRUE(shim.editor().StyleGetHotSpot(style))
            << "byte " << i << " style not clickable";
    }
}

TEST(EditorTests, ReleasingCtrlDeactivatesHotspots) {
    // Counterpart: releasing Ctrl must clear hotspot styling so plain
    // mouse clicks land in the editor as caret moves, not include jumps.
    EditorTestShim shim;
    shim.editor().SetText("#include \"foo.bi\"\n");
    shim.run([&] {
        wxUIActionSimulator sim;
        sim.KeyDown(WXK_CONTROL);
        wxYield();
        sim.KeyUp(WXK_CONTROL);
        wxYield();
    });
    EXPECT_FALSE(shim.editor().StyleGetHotSpot(+ThemeCategory::Preprocessor));
    EXPECT_FALSE(shim.editor().StyleGetHotSpot(+ThemeCategory::KeywordPP));
    EXPECT_FALSE(shim.editor().StyleGetHotSpot(+ThemeCategory::StringPP));
    EXPECT_FALSE(shim.editor().StyleGetHotSpot(+ThemeCategory::IdentifierPP));
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

TEST(EditorTests, CommentPreservesPartialSelection) {
    EditorTestShim shim;
    shim.setText("foobar");
    auto& editor = shim.editor();
    editor.SetSelection(1, 3); // "oo"
    editor.commentSelection();
    EXPECT_EQ(shim.getText(), "'foobar");
    EXPECT_EQ(editor.GetSelectedText(), "oo"); // selection tracks the same text
}

TEST(EditorTests, UncommentPreservesPartialSelection) {
    EditorTestShim shim;
    shim.setText("'foobar");
    auto& editor = shim.editor();
    editor.SetSelection(2, 4); // "oo" (after the leading quote)
    editor.uncommentSelection();
    EXPECT_EQ(shim.getText(), "foobar");
    EXPECT_EQ(editor.GetSelectedText(), "oo");
}

TEST(EditorTests, CommentUncommentRoundTripRestoresSelection) {
    EditorTestShim shim;
    shim.setText("foo\nbar");
    auto& editor = shim.editor();
    editor.SetSelection(0, 7); // both lines' content
    editor.commentSelection();
    EXPECT_EQ(shim.getText(), "'foo\n'bar");
    editor.uncommentSelection();
    EXPECT_EQ(shim.getText(), "foo\nbar");
    EXPECT_EQ(editor.GetSelectedText(), "foo\nbar"); // original selection restored
}

TEST(EditorTests, CommentUncommentWholeLineMultiSelectionRoundTrip) {
    EditorTestShim shim;
    shim.setText("aaa\nbbb\nccc\n");
    auto& editor = shim.editor();
    editor.SetSelection(0, 8); // whole lines 0-1; caret rests at the start of line 2
    editor.commentSelection();
    EXPECT_EQ(shim.getText(), "'aaa\n'bbb\nccc\n"); // line 2 must stay untouched
    editor.uncommentSelection();
    EXPECT_EQ(shim.getText(), "aaa\nbbb\nccc\n");
    EXPECT_EQ(editor.GetSelectionStart(), 0);
    EXPECT_EQ(editor.GetSelectionEnd(), 8); // original selection restored
}

TEST(EditorTests, CommentBackwardSelectionKeepsDirection) {
    EditorTestShim shim;
    shim.setText("foobar");
    auto& editor = shim.editor();
    editor.SetAnchor(3);
    editor.SetCurrentPos(1); // backward "oo": caret at the front
    editor.commentSelection();
    EXPECT_EQ(shim.getText(), "'foobar");
    EXPECT_EQ(editor.GetSelectedText(), "oo");
    EXPECT_EQ(editor.GetCurrentPos(), editor.GetSelectionStart()); // caret still leads
}

TEST(EditorTests, CommentBackwardMultilineKeepsDirectionRoundTrip) {
    EditorTestShim shim;
    shim.setText("aaa\nbbb\n");
    auto& editor = shim.editor();
    editor.SetAnchor(7);     // end of "bbb"
    editor.SetCurrentPos(0); // backward: caret at the very front
    editor.commentSelection();
    EXPECT_EQ(shim.getText(), "'aaa\n'bbb\n");
    EXPECT_EQ(editor.GetCurrentPos(), editor.GetSelectionStart());
    editor.uncommentSelection();
    EXPECT_EQ(shim.getText(), "aaa\nbbb\n");
    EXPECT_EQ(editor.GetCurrentPos(), editor.GetSelectionStart());
}

TEST(EditorTests, CommentRectangularSelectionStaysRectangularRoundTrip) {
    EditorTestShim shim;
    shim.setText("aaa\nbbb\nccc\n");
    auto& editor = shim.editor();
    editor.SetSelectionMode(wxSTC_SEL_RECTANGLE);
    editor.SetRectangularSelectionAnchor(0);
    editor.SetRectangularSelectionCaret(6); // block across lines 0-1
    editor.commentSelection();
    EXPECT_EQ(shim.getText(), "'aaa\n'bbb\nccc\n");
    EXPECT_TRUE(editor.SelectionIsRectangle()); // block kept, not flattened to a stream
    editor.uncommentSelection();
    EXPECT_EQ(shim.getText(), "aaa\nbbb\nccc\n");
    EXPECT_TRUE(editor.SelectionIsRectangle());
    EXPECT_EQ(editor.GetRectangularSelectionAnchor(), 0); // original block restored
    EXPECT_EQ(editor.GetRectangularSelectionCaret(), 6);
}
