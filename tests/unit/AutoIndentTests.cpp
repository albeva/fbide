//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/geobide
//
#include <gtest/gtest.h>
#include "TestHelpers.hpp"
#include "editor/AutoIndent.hpp"

using namespace fbide;
using namespace fbide::indent;

class AutoIndentTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;

    void SetUp() override {
        m_lexer = tests::createFbLexer(testDataPath + "fbfull.lng");
    }

    void TearDown() override {
        m_lexer->Release();
        m_lexer = nullptr;
    }

    auto decide(const wxString& line) const -> Decision {
        const auto tokens = tests::tokenise(*m_lexer, line.utf8_string());
        return Decision::decide(tokens);
    }

    auto opener(const wxString& line) const -> testing::AssertionResult {
        const auto d = decide(line);
        if (d.deltaLevels == 1 && !d.dedentPrev) {
            return testing::AssertionSuccess();
        }
        return testing::AssertionFailure()
            << "expected opener {+1, false} got {" << d.deltaLevels << ", " << d.dedentPrev << "} for: " << line;
    }

    auto neutral(const wxString& line) const -> testing::AssertionResult {
        const auto d = decide(line);
        if (d.deltaLevels == 0 && !d.dedentPrev) {
            return testing::AssertionSuccess();
        }
        return testing::AssertionFailure()
            << "expected neutral {0, false} got {" << d.deltaLevels << ", " << d.dedentPrev << "} for: " << line;
    }

    auto closer(const wxString& line) const -> testing::AssertionResult {
        const auto d = decide(line);
        if (d.deltaLevels == 0 && d.dedentPrev) {
            return testing::AssertionSuccess();
        }
        return testing::AssertionFailure()
            << "expected closer {0, true} got {" << d.deltaLevels << ", " << d.dedentPrev << "} for: " << line;
    }

    auto mid(const wxString& line) const -> testing::AssertionResult {
        const auto d = decide(line);
        if (d.deltaLevels == 1 && d.dedentPrev) {
            return testing::AssertionSuccess();
        }
        return testing::AssertionFailure()
            << "expected mid {+1, true} got {" << d.deltaLevels << ", " << d.dedentPrev << "} for: " << line;
    }

    auto closerWords(const wxString& line) const -> std::vector<std::string_view> {
        const auto d = decide(line);
        return { d.closerKeywords.begin(), d.closerKeywords.end() };
    }

    Scintilla::ILexer5* m_lexer { nullptr };
};

// ---------------------------------------------------------------------------
// Block openers
// ---------------------------------------------------------------------------

TEST_F(AutoIndentTests, IfThenOpens) {
    EXPECT_TRUE(opener("If x > 0 Then"));
    EXPECT_TRUE(opener("If foo() Then"));
    EXPECT_TRUE(opener("    If x Then"));
}

TEST_F(AutoIndentTests, IfThenWithTrailingCommentOpens) {
    EXPECT_TRUE(opener("If x Then ' comment"));
}

TEST_F(AutoIndentTests, DoOpens) {
    EXPECT_TRUE(opener("Do"));
    EXPECT_TRUE(opener("Do While x > 0"));
    EXPECT_TRUE(opener("Do Until done"));
}

TEST_F(AutoIndentTests, ForOpens) {
    EXPECT_TRUE(opener("For i = 1 To 10"));
    EXPECT_TRUE(opener("For Each item In list"));
}

TEST_F(AutoIndentTests, WhileOpens) {
    EXPECT_TRUE(opener("While x > 0"));
}

TEST_F(AutoIndentTests, SubFunctionOpen) {
    EXPECT_TRUE(opener("Sub Main"));
    EXPECT_TRUE(opener("Sub Main()"));
    EXPECT_TRUE(opener("Private Sub Foo()"));
    EXPECT_TRUE(opener("Function Add(a As Integer, b As Integer) As Integer"));
    EXPECT_TRUE(opener("Constructor MyType()"));
    EXPECT_TRUE(opener("Destructor MyType()"));
    EXPECT_TRUE(opener("Operator Cast() As Integer"));
}

TEST_F(AutoIndentTests, BlockOpenersMisc) {
    EXPECT_TRUE(opener("Select Case x"));
    EXPECT_TRUE(opener("With foo"));
    EXPECT_TRUE(opener("Scope"));
    EXPECT_TRUE(opener("Enum Color"));
    EXPECT_TRUE(opener("Union U"));
    EXPECT_TRUE(opener("Namespace N"));
    EXPECT_TRUE(opener("Asm"));
}

TEST_F(AutoIndentTests, TypeBlockOpens) {
    EXPECT_TRUE(opener("Type Foo"));
    EXPECT_TRUE(opener("Type Foo Extends Bar"));
}

// ---------------------------------------------------------------------------
// Skip cases — must NOT open a block
// ---------------------------------------------------------------------------

TEST_F(AutoIndentTests, SingleLineIfDoesNotOpen) {
    EXPECT_TRUE(neutral("If x > 0 Then Print x"));
    EXPECT_TRUE(neutral("If x Then Print \"a\" Else Print \"b\""));
}

TEST_F(AutoIndentTests, IfWithColonStatementDoesNotOpen) {
    EXPECT_TRUE(neutral("If x > 0 Then : Print x :"));
}

TEST_F(AutoIndentTests, DeclareDoesNotOpen) {
    EXPECT_TRUE(neutral("Declare Sub Foo()"));
    EXPECT_TRUE(neutral("Declare Function Bar() As Integer"));
}

TEST_F(AutoIndentTests, ExitContinueDoNotOpen) {
    EXPECT_TRUE(neutral("Exit Sub"));
    EXPECT_TRUE(neutral("Exit For"));
    EXPECT_TRUE(neutral("Continue For"));
}

TEST_F(AutoIndentTests, TypeAliasDoesNotOpen) {
    EXPECT_TRUE(neutral("Type X As Integer"));
}

TEST_F(AutoIndentTests, BareSubFunctionWithoutNameDoesNotOpen) {
    // `Function = expr` is an assignment to the implicit return name —
    // no name follows the keyword.
    EXPECT_TRUE(neutral("Function = 10"));
}

TEST_F(AutoIndentTests, InlineForLoopWithNextDoesNotOpen) {
    // `For i = 1 To 10 : Print i : Next` — closer present on same line.
    EXPECT_TRUE(neutral("For i = 1 To 10 : Print i : Next"));
}

TEST_F(AutoIndentTests, SingleLineAsmDoesNotOpen) {
    // FB single-line `asm <stmt>` is a one-shot statement — no block.
    EXPECT_TRUE(neutral("Asm mov eax, 10"));
    EXPECT_TRUE(neutral("Asm nop"));
    EXPECT_TRUE(neutral("    Asm mov eax, 10"));
}

TEST_F(AutoIndentTests, SingleLineAsmEmitsNoCloser) {
    EXPECT_TRUE(closerWords("Asm mov eax, 10").empty());
}

TEST_F(AutoIndentTests, EmptyAndCommentOnlyLines) {
    EXPECT_TRUE(neutral(""));
    EXPECT_TRUE(neutral("    "));
    EXPECT_TRUE(neutral("' just a comment"));
    EXPECT_TRUE(neutral("Print x"));
}

// ---------------------------------------------------------------------------
// Keyword reuse — only the first keyword of the statement decides whether
// a block opens. Reused keywords inside other statements (e.g. `For` in
// `Open ... For Input`) must not push a block.
// ---------------------------------------------------------------------------

TEST_F(AutoIndentTests, OpenForInputDoesNotOpenForBlock) {
    EXPECT_TRUE(neutral("Open \"file\" For Input As #f"));
    EXPECT_TRUE(neutral("Open \"file\" For Output As #1"));
    EXPECT_TRUE(neutral("Open \"file\" For Append As #fileNum"));
}

// ---------------------------------------------------------------------------
// Access modifiers (Private/Public/Protected) are transparent prefixes —
// the keyword that follows decides the block.
// ---------------------------------------------------------------------------

TEST_F(AutoIndentTests, AccessModifiersTransparentForBlockOpeners) {
    EXPECT_TRUE(opener("Private Sub Foo()"));
    EXPECT_TRUE(opener("Public Sub Foo()"));
    EXPECT_TRUE(opener("Protected Sub Foo()"));
    EXPECT_TRUE(opener("Public Function Bar() As Integer"));
    EXPECT_TRUE(opener("Private Operator MyType.Cast() As Integer"));
    EXPECT_TRUE(opener("Public Type MyType"));
    EXPECT_TRUE(opener("Public Enum Color"));
}

TEST_F(AutoIndentTests, PublicTypeAliasDoesNotOpen) {
    // `Public Type X As Integer` is an alias declaration — modifier transparent
    // but the Type-As form must still resolve to a statement.
    EXPECT_TRUE(neutral("Public Type X As Integer"));
}

TEST_F(AutoIndentTests, PublicLabelInsideTypeBodyDoesNotOpen) {
    // `public:` (with trailing colon) is a C++-style visibility label inside
    // a Type body, not a Sub/Function modifier — must not open a block.
    EXPECT_TRUE(neutral("public:"));
    EXPECT_TRUE(neutral("private:"));
    EXPECT_TRUE(neutral("protected:"));
}

// ---------------------------------------------------------------------------
// Block closers
// ---------------------------------------------------------------------------

TEST_F(AutoIndentTests, EndIfCloses) {
    EXPECT_TRUE(closer("End If"));
    EXPECT_TRUE(closer("EndIf"));
    EXPECT_TRUE(closer("ENDIF"));
}

TEST_F(AutoIndentTests, EndSubFunctionCloses) {
    EXPECT_TRUE(closer("End Sub"));
    EXPECT_TRUE(closer("End Function"));
    EXPECT_TRUE(closer("End Type"));
    EXPECT_TRUE(closer("End Select"));
    EXPECT_TRUE(closer("End Enum"));
}

TEST_F(AutoIndentTests, LoopNextWendClose) {
    EXPECT_TRUE(closer("Loop"));
    EXPECT_TRUE(closer("Loop While x > 0"));
    EXPECT_TRUE(closer("Next"));
    EXPECT_TRUE(closer("Next i"));
    EXPECT_TRUE(closer("Wend"));
}

// ---------------------------------------------------------------------------
// Mid-block keywords
// ---------------------------------------------------------------------------

TEST_F(AutoIndentTests, ElseDedentsAndReindents) {
    EXPECT_TRUE(mid("Else"));
    EXPECT_TRUE(mid("ElseIf x > 0 Then"));
    EXPECT_TRUE(mid("Case 1"));
    EXPECT_TRUE(mid("Case Else"));
}

// ---------------------------------------------------------------------------
// Auto-close — opener line populates closerKeywords with lowercase words
// ---------------------------------------------------------------------------

TEST_F(AutoIndentTests, AutoCloseIfThen) {
    EXPECT_EQ(closerWords("If x Then"), (std::vector<std::string_view> { "end", "if" }));
}

TEST_F(AutoIndentTests, AutoCloseControlFlow) {
    EXPECT_EQ(closerWords("Do"), (std::vector<std::string_view> { "loop" }));
    EXPECT_EQ(closerWords("For i = 1 To 10"), (std::vector<std::string_view> { "next" }));
    EXPECT_EQ(closerWords("While x > 0"), (std::vector<std::string_view> { "wend" }));
}

TEST_F(AutoIndentTests, AutoCloseCallables) {
    EXPECT_EQ(closerWords("Sub Main"), (std::vector<std::string_view> { "end", "sub" }));
    EXPECT_EQ(closerWords("Function Add(a As Integer) As Integer"), (std::vector<std::string_view> { "end", "function" }));
    EXPECT_EQ(closerWords("Constructor MyType()"), (std::vector<std::string_view> { "end", "constructor" }));
    EXPECT_EQ(closerWords("Destructor MyType()"), (std::vector<std::string_view> { "end", "destructor" }));
    EXPECT_EQ(closerWords("Operator Cast() As Integer"), (std::vector<std::string_view> { "end", "operator" }));
}

TEST_F(AutoIndentTests, AutoCloseAggregates) {
    EXPECT_EQ(closerWords("Type Foo"), (std::vector<std::string_view> { "end", "type" }));
    EXPECT_EQ(closerWords("Enum Color"), (std::vector<std::string_view> { "end", "enum" }));
    EXPECT_EQ(closerWords("Union U"), (std::vector<std::string_view> { "end", "union" }));
    EXPECT_EQ(closerWords("Select Case x"), (std::vector<std::string_view> { "end", "select" }));
    EXPECT_EQ(closerWords("With foo"), (std::vector<std::string_view> { "end", "with" }));
    EXPECT_EQ(closerWords("Namespace N"), (std::vector<std::string_view> { "end", "namespace" }));
    EXPECT_EQ(closerWords("Scope"), (std::vector<std::string_view> { "end", "scope" }));
    EXPECT_EQ(closerWords("Asm"), (std::vector<std::string_view> { "end", "asm" }));
}

TEST_F(AutoIndentTests, NoCloserForNonOpeners) {
    EXPECT_TRUE(closerWords("If x Then Print x").empty());
    EXPECT_TRUE(closerWords("If x Then : Print x :").empty());
    EXPECT_TRUE(closerWords("Declare Sub Foo()").empty());
    EXPECT_TRUE(closerWords("Type X As Integer").empty());
    EXPECT_TRUE(closerWords("Exit Sub").empty());
    EXPECT_TRUE(closerWords("End If").empty());
    EXPECT_TRUE(closerWords("Print x").empty());
}
