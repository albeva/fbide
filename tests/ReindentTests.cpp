//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "lib/analyses/lexer/Lexer.hpp"
#include "lib/config/Keywords.hpp"
#include "lib/format/formatters/ReindentTransform.hpp"
#include <gtest/gtest.h>

using namespace fbide;

class ReindentTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;
    static constexpr int tabSize = 4;

    void SetUp() override {
        Keywords kw;
        kw.load(testDataPath + "fbfull.lng");
        m_lexer = std::make_unique<lexer::Lexer>(kw);
    }

    /// Tokenise, reindent, and return the resulting text.
    auto reindent(const wxString& source) -> wxString {
        auto tokens = m_lexer->tokenise(source);
        const ReindentTransform transform(tabSize);
        tokens = transform.apply(std::move(tokens));

        wxString result;
        for (const auto& tok : tokens) {
            result += tok.text;
        }
        return result;
    }

    std::unique_ptr<lexer::Lexer> m_lexer;
};

// ---------------------------------------------------------------------------
// Sub / Function
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, SubBlock) {
    const auto result = reindent(
        "Sub Main\n"
        "Print \"hello\"\n"
        "End Sub\n"
    );
    EXPECT_EQ(result,
        "Sub Main\n"
        "    Print \"hello\"\n"
        "End Sub\n"
    );
}

TEST_F(ReindentTests, FunctionBlock) {
    const auto result = reindent(
        "Function Add(a As Integer, b As Integer) As Integer\n"
        "Return a + b\n"
        "End Function\n"
    );
    EXPECT_EQ(result,
        "Function Add(a As Integer, b As Integer) As Integer\n"
        "    Return a + b\n"
        "End Function\n"
    );
}

TEST_F(ReindentTests, NestedBlocks) {
    const auto result = reindent(
        "Sub Main\n"
        "For i = 1 To 10\n"
        "Print i\n"
        "Next\n"
        "End Sub\n"
    );
    EXPECT_EQ(result,
        "Sub Main\n"
        "    For i = 1 To 10\n"
        "        Print i\n"
        "    Next\n"
        "End Sub\n"
    );
}

// ---------------------------------------------------------------------------
// If / Then — multi-line
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, MultiLineIfThen) {
    const auto result = reindent(
        "If x > 0 Then\n"
        "Print x\n"
        "End If\n"
    );
    EXPECT_EQ(result,
        "If x > 0 Then\n"
        "    Print x\n"
        "End If\n"
    );
}

TEST_F(ReindentTests, MultiLineIfThenElse) {
    const auto result = reindent(
        "If x > 0 Then\n"
        "Print \"pos\"\n"
        "Else\n"
        "Print \"neg\"\n"
        "End If\n"
    );
    EXPECT_EQ(result,
        "If x > 0 Then\n"
        "    Print \"pos\"\n"
        "Else\n"
        "    Print \"neg\"\n"
        "End If\n"
    );
}

TEST_F(ReindentTests, MultiLineIfThenElseIf) {
    const auto result = reindent(
        "If x > 0 Then\n"
        "Print \"pos\"\n"
        "ElseIf x < 0 Then\n"
        "Print \"neg\"\n"
        "Else\n"
        "Print \"zero\"\n"
        "End If\n"
    );
    EXPECT_EQ(result,
        "If x > 0 Then\n"
        "    Print \"pos\"\n"
        "ElseIf x < 0 Then\n"
        "    Print \"neg\"\n"
        "Else\n"
        "    Print \"zero\"\n"
        "End If\n"
    );
}

// ---------------------------------------------------------------------------
// If / Then — single-line (no indent change)
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, SingleLineIfThen) {
    const auto result = reindent(
        "If x > 0 Then Print x\n"
    );
    EXPECT_EQ(result,
        "If x > 0 Then Print x\n"
    );
}

TEST_F(ReindentTests, SingleLineIfThenElse) {
    const auto result = reindent(
        "If x > 0 Then Print \"pos\" Else Print \"neg\"\n"
    );
    EXPECT_EQ(result,
        "If x > 0 Then Print \"pos\" Else Print \"neg\"\n"
    );
}

TEST_F(ReindentTests, SingleLineIfNoIndentOnNext) {
    // Line after single-line If should NOT be indented
    const auto result = reindent(
        "If x > 0 Then Print x\n"
        "Print \"done\"\n"
    );
    EXPECT_EQ(result,
        "If x > 0 Then Print x\n"
        "Print \"done\"\n"
    );
}

// ---------------------------------------------------------------------------
// Colon-separated statements (no indent change)
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, ColonSeparatedIfThenEndIf) {
    const auto result = reindent(
        "If x Then : Print x : End If\n"
        "Print \"done\"\n"
    );
    EXPECT_EQ(result,
        "If x Then : Print x : End If\n"
        "Print \"done\"\n"
    );
}

TEST_F(ReindentTests, ColonSeparatedForNext) {
    const auto result = reindent(
        "For i = 1 To 10 : Print i : Next\n"
        "Print \"done\"\n"
    );
    EXPECT_EQ(result,
        "For i = 1 To 10 : Print i : Next\n"
        "Print \"done\"\n"
    );
}

// ---------------------------------------------------------------------------
// For / Next
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, ForNext) {
    const auto result = reindent(
        "For i = 1 To 10\n"
        "Print i\n"
        "Next\n"
    );
    EXPECT_EQ(result,
        "For i = 1 To 10\n"
        "    Print i\n"
        "Next\n"
    );
}

// ---------------------------------------------------------------------------
// Do / Loop
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, DoLoop) {
    const auto result = reindent(
        "Do\n"
        "x += 1\n"
        "Loop\n"
    );
    EXPECT_EQ(result,
        "Do\n"
        "    x += 1\n"
        "Loop\n"
    );
}

// ---------------------------------------------------------------------------
// While / Wend
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, WhileWend) {
    const auto result = reindent(
        "While x > 0\n"
        "x -= 1\n"
        "Wend\n"
    );
    EXPECT_EQ(result,
        "While x > 0\n"
        "    x -= 1\n"
        "Wend\n"
    );
}

// ---------------------------------------------------------------------------
// Select Case
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, SelectCase) {
    const auto result = reindent(
        "Select Case x\n"
        "Case 1\n"
        "Print \"one\"\n"
        "Case 2\n"
        "Print \"two\"\n"
        "Case Else\n"
        "Print \"other\"\n"
        "End Select\n"
    );
    EXPECT_EQ(result,
        "Select Case x\n"
        "Case 1\n"
        "    Print \"one\"\n"
        "Case 2\n"
        "    Print \"two\"\n"
        "Case Else\n"
        "    Print \"other\"\n"
        "End Select\n"
    );
}

// ---------------------------------------------------------------------------
// Type
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, TypeBlock) {
    const auto result = reindent(
        "Type MyType\n"
        "x As Integer\n"
        "y As Integer\n"
        "End Type\n"
    );
    EXPECT_EQ(result,
        "Type MyType\n"
        "    x As Integer\n"
        "    y As Integer\n"
        "End Type\n"
    );
}

TEST_F(ReindentTests, TypeAsAlias) {
    // "Type As ..." is an alias, not a block — no indent
    const auto result = reindent(
        "Type As Integer MyInt\n"
        "Dim x As MyInt\n"
    );
    EXPECT_EQ(result,
        "Type As Integer MyInt\n"
        "Dim x As MyInt\n"
    );
}

// ---------------------------------------------------------------------------
// Scope
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, ScopeBlock) {
    const auto result = reindent(
        "Scope\n"
        "Dim x = 1\n"
        "End Scope\n"
    );
    EXPECT_EQ(result,
        "Scope\n"
        "    Dim x = 1\n"
        "End Scope\n"
    );
}

// ---------------------------------------------------------------------------
// Preprocessor stays at column 0
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, PreprocessorFlushLeft) {
    const auto result = reindent(
        "Sub Main\n"
        "#include \"file.bi\"\n"
        "Print x\n"
        "End Sub\n"
    );
    EXPECT_EQ(result,
        "Sub Main\n"
        "#include \"file.bi\"\n"
        "    Print x\n"
        "End Sub\n"
    );
}

// ---------------------------------------------------------------------------
// Comments after block keywords
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, BlockKeywordWithComment) {
    // Comment after Then should not prevent indent
    const auto result = reindent(
        "If x > 0 Then ' check\n"
        "Print x\n"
        "End If\n"
    );
    EXPECT_EQ(result,
        "If x > 0 Then ' check\n"
        "    Print x\n"
        "End If\n"
    );
}

// ---------------------------------------------------------------------------
// Already indented code gets re-indented
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, StripExistingIndent) {
    const auto result = reindent(
        "Sub Main\n"
        "            Print \"over-indented\"\n"
        "End Sub\n"
    );
    EXPECT_EQ(result,
        "Sub Main\n"
        "    Print \"over-indented\"\n"
        "End Sub\n"
    );
}
