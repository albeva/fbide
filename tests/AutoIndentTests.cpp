//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/geobide
//
#include <gtest/gtest.h>
#include "editor/AutoIndent.hpp"

using namespace fbide;
using namespace fbide::indent;

namespace {

auto opener(const wxString& line) -> testing::AssertionResult {
    const auto d = decide(line);
    if (d.deltaLevels == 1 && !d.dedentPrev) {
        return testing::AssertionSuccess();
    }
    return testing::AssertionFailure()
        << "expected opener {+1, false} got {" << d.deltaLevels << ", " << d.dedentPrev << "} for: " << line;
}

auto neutral(const wxString& line) -> testing::AssertionResult {
    const auto d = decide(line);
    if (d.deltaLevels == 0 && !d.dedentPrev) {
        return testing::AssertionSuccess();
    }
    return testing::AssertionFailure()
        << "expected neutral {0, false} got {" << d.deltaLevels << ", " << d.dedentPrev << "} for: " << line;
}

auto closer(const wxString& line) -> testing::AssertionResult {
    const auto d = decide(line);
    if (d.deltaLevels == 0 && d.dedentPrev) {
        return testing::AssertionSuccess();
    }
    return testing::AssertionFailure()
        << "expected closer {0, true} got {" << d.deltaLevels << ", " << d.dedentPrev << "} for: " << line;
}

auto mid(const wxString& line) -> testing::AssertionResult {
    const auto d = decide(line);
    if (d.deltaLevels == 1 && d.dedentPrev) {
        return testing::AssertionSuccess();
    }
    return testing::AssertionFailure()
        << "expected mid {+1, true} got {" << d.deltaLevels << ", " << d.dedentPrev << "} for: " << line;
}

} // namespace

class AutoIndentTests : public testing::Test {};

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

TEST_F(AutoIndentTests, EmptyAndCommentOnlyLines) {
    EXPECT_TRUE(neutral(""));
    EXPECT_TRUE(neutral("    "));
    EXPECT_TRUE(neutral("' just a comment"));
    EXPECT_TRUE(neutral("Print x"));
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
// Auto-close — opener line populates insertCloser with canonical closer
// ---------------------------------------------------------------------------

TEST_F(AutoIndentTests, AutoCloseIfThen) {
    EXPECT_EQ(decide("If x Then").insertCloser.value_or(""), "End If");
}

TEST_F(AutoIndentTests, AutoCloseControlFlow) {
    EXPECT_EQ(decide("Do").insertCloser.value_or(""), "Loop");
    EXPECT_EQ(decide("For i = 1 To 10").insertCloser.value_or(""), "Next");
    EXPECT_EQ(decide("While x > 0").insertCloser.value_or(""), "Wend");
}

TEST_F(AutoIndentTests, AutoCloseCallables) {
    EXPECT_EQ(decide("Sub Main").insertCloser.value_or(""), "End Sub");
    EXPECT_EQ(decide("Function Add(a As Integer) As Integer").insertCloser.value_or(""), "End Function");
    EXPECT_EQ(decide("Constructor MyType()").insertCloser.value_or(""), "End Constructor");
    EXPECT_EQ(decide("Destructor MyType()").insertCloser.value_or(""), "End Destructor");
    EXPECT_EQ(decide("Operator Cast() As Integer").insertCloser.value_or(""), "End Operator");
}

TEST_F(AutoIndentTests, AutoCloseAggregates) {
    EXPECT_EQ(decide("Type Foo").insertCloser.value_or(""), "End Type");
    EXPECT_EQ(decide("Enum Color").insertCloser.value_or(""), "End Enum");
    EXPECT_EQ(decide("Union U").insertCloser.value_or(""), "End Union");
    EXPECT_EQ(decide("Select Case x").insertCloser.value_or(""), "End Select");
    EXPECT_EQ(decide("With foo").insertCloser.value_or(""), "End With");
    EXPECT_EQ(decide("Namespace N").insertCloser.value_or(""), "End Namespace");
    EXPECT_EQ(decide("Scope").insertCloser.value_or(""), "End Scope");
    EXPECT_EQ(decide("Asm").insertCloser.value_or(""), "End Asm");
}

TEST_F(AutoIndentTests, NoCloserForNonOpeners) {
    EXPECT_FALSE(decide("If x Then Print x").insertCloser.has_value());
    EXPECT_FALSE(decide("If x Then : Print x :").insertCloser.has_value());
    EXPECT_FALSE(decide("Declare Sub Foo()").insertCloser.has_value());
    EXPECT_FALSE(decide("Type X As Integer").insertCloser.has_value());
    EXPECT_FALSE(decide("Exit Sub").insertCloser.has_value());
    EXPECT_FALSE(decide("End If").insertCloser.has_value());
    EXPECT_FALSE(decide("Print x").insertCloser.has_value());
}
