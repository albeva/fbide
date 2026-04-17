//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "lib/analyses/lexer/Lexer.hpp"
#include "lib/config/Keywords.hpp"
#include "../src/lib/format/transformers/reformat/ReFormatter.hpp"
#include <gtest/gtest.h>

using namespace fbide;
using namespace fbide::reformat;

class ReFormatterTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;
    static constexpr std::size_t tabSize = 4;

    void SetUp() override {
        Keywords kw;
        kw.load(testDataPath + "fbfull.lng");
        m_lexer = std::make_unique<lexer::Lexer>(kw);
    }

    auto format(const char* source, const bool anchoredPP = false) -> std::string {
        return formatWith(source, { .tabSize = tabSize, .anchoredPP = anchoredPP });
    }

    auto formatWith(const char* source, const FormatOptions& options) -> std::string {
        const auto tokens = m_lexer->tokenise(source);
        ReFormatter formatter(options);
        return joinText(formatter.apply(tokens));
    }

    static auto joinText(const std::vector<lexer::Token>& tokens) -> std::string {
        std::string out;
        std::size_t size = 0;
        for (const auto& tok : tokens) {
            size += tok.text.size();
        }
        out.reserve(size);
        for (const auto& tok : tokens) {
            out += tok.text;
        }
        return out;
    }

    std::unique_ptr<lexer::Lexer> m_lexer;
};

// ---------------------------------------------------------------------------
// Callable block openers — Sub / Function / Constructor / Destructor / Operator
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, SubBlock) {
    EXPECT_EQ(format(
        "Sub Main\n"
        "Print \"hello\"\n"
        "End Sub\n"
    ),
        "Sub Main\n"
        "    Print \"hello\"\n"
        "End Sub\n"
    );
}

TEST_F(ReFormatterTests, FunctionBlock) {
    EXPECT_EQ(format(
        "Function Add(a As Integer, b As Integer) As Integer\n"
        "Return a + b\n"
        "End Function\n"
    ),
        "Function Add(a As Integer, b As Integer) As Integer\n"
        "    Return a + b\n"
        "End Function\n"
    );
}

TEST_F(ReFormatterTests, ConstructorBlock) {
    EXPECT_EQ(format(
        "Constructor MyType()\n"
        "x = 1\n"
        "End Constructor\n"
    ),
        "Constructor MyType()\n"
        "    x = 1\n"
        "End Constructor\n"
    );
}

TEST_F(ReFormatterTests, DestructorBlock) {
    EXPECT_EQ(format(
        "Destructor MyType()\n"
        "x = 0\n"
        "End Destructor\n"
    ),
        "Destructor MyType()\n"
        "    x = 0\n"
        "End Destructor\n"
    );
}

TEST_F(ReFormatterTests, OperatorBlock) {
    EXPECT_EQ(format(
        "Operator MyType.Cast() As String\n"
        "Return \"hello\"\n"
        "End Operator\n"
    ),
        "Operator MyType.Cast() As String\n"
        "    Return \"hello\"\n"
        "End Operator\n"
    );
}

// ---------------------------------------------------------------------------
// Declare forms don't open blocks
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, DeclareFormsDoNotOpenBlocks) {
    EXPECT_EQ(format(
        "Declare Sub S()\n"
        "Declare Function F() As Integer\n"
        "Declare Constructor()\n"
        "Declare Destructor()\n"
        "Declare Operator Cast() As String\n"
        "Dim x = 1\n"
    ),
        "Declare Sub S()\n"
        "Declare Function F() As Integer\n"
        "Declare Constructor()\n"
        "Declare Destructor()\n"
        "Declare Operator Cast() As String\n"
        "Dim x = 1\n"
    );
}

// ---------------------------------------------------------------------------
// Function = value (property setter) must not be mistaken for block opener
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, FunctionPropertySetter) {
    EXPECT_EQ(format(
        "Function Foo() As Integer\n"
        "Function = 10\n"
        "End Function\n"
    ),
        "Function Foo() As Integer\n"
        "    Function = 10\n"
        "End Function\n"
    );
}

// ---------------------------------------------------------------------------
// Exit Sub — bare callable keyword with no name following is not a body opener
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, ExitSubIsNotBlockOpener) {
    EXPECT_EQ(format(
        "If done Then\n"
        "Exit Sub\n"
        "End If\n"
    ),
        "If done Then\n"
        "    Exit Sub\n"
        "End If\n"
    );
}

TEST_F(ReFormatterTests, ExitLoopKeywordsDoNotOpenBlock) {
    EXPECT_EQ(format(
        "If true Then\n"
        "Exit Do, Do\n"
        "End If\n"
    ),
        "If true Then\n"
        "    Exit Do, Do\n"
        "End If\n"
    );

    EXPECT_EQ(format(
        "Do\n"
        "Exit Do\n"
        "Loop\n"
    ),
        "Do\n"
        "    Exit Do\n"
        "Loop\n"
    );

    EXPECT_EQ(format(
        "For i = 1 To 10\n"
        "Continue For\n"
        "Next\n"
    ),
        "For i = 1 To 10\n"
        "    Continue For\n"
        "Next\n"
    );
}

// ---------------------------------------------------------------------------
// Nested blocks
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, NestedBlocks) {
    EXPECT_EQ(format(
        "Sub Main\n"
        "For i = 1 To 10\n"
        "Print i\n"
        "Next\n"
        "End Sub\n"
    ),
        "Sub Main\n"
        "    For i = 1 To 10\n"
        "        Print i\n"
        "    Next\n"
        "End Sub\n"
    );
}

TEST_F(ReFormatterTests, DeeplyNestedBlocks) {
    EXPECT_EQ(format(
        "Sub Main\n"
        "If x Then\n"
        "For i = 1 To 10\n"
        "Print i\n"
        "Next\n"
        "End If\n"
        "End Sub\n"
    ),
        "Sub Main\n"
        "    If x Then\n"
        "        For i = 1 To 10\n"
        "            Print i\n"
        "        Next\n"
        "    End If\n"
        "End Sub\n"
    );
}

// ---------------------------------------------------------------------------
// If / Then — multi-line vs single-line
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, MultiLineIfThenElseIf) {
    EXPECT_EQ(format(
        "If x > 0 Then\n"
        "Print \"pos\"\n"
        "ElseIf x < 0 Then\n"
        "Print \"neg\"\n"
        "Else\n"
        "Print \"zero\"\n"
        "End If\n"
    ),
        "If x > 0 Then\n"
        "    Print \"pos\"\n"
        "ElseIf x < 0 Then\n"
        "    Print \"neg\"\n"
        "Else\n"
        "    Print \"zero\"\n"
        "End If\n"
    );
}

TEST_F(ReFormatterTests, SingleLineIfDoesNotOpenBlock) {
    // Single-line `If x Then <stmt>` (no trailing Then) is a statement;
    // the following line must not be indented.
    EXPECT_EQ(format(
        "If x > 0 Then Print x\n"
        "If x > 0 Then Print \"pos\" Else Print \"neg\"\n"
        "Print \"done\"\n"
    ),
        "If x > 0 Then Print x\n"
        "If x > 0 Then Print \"pos\" Else Print \"neg\"\n"
        "Print \"done\"\n"
    );
}

TEST_F(ReFormatterTests, IfWithTrailingCommentStillOpensBlock) {
    // Last significant keyword is Then; the comment after Then is ignored
    // for block detection but preserved verbatim.
    EXPECT_EQ(format(
        "If x > 0 Then ' check positive\n"
        "Print x\n"
        "End If\n"
    ),
        "If x > 0 Then ' check positive\n"
        "    Print x\n"
        "End If\n"
    );
}

// ---------------------------------------------------------------------------
// Other control-flow blocks
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, ForNext) {
    EXPECT_EQ(format(
        "For i = 1 To 10\n"
        "Print i\n"
        "Next\n"
    ),
        "For i = 1 To 10\n"
        "    Print i\n"
        "Next\n"
    );
}

TEST_F(ReFormatterTests, DoLoop) {
    EXPECT_EQ(format(
        "Do\n"
        "x += 1\n"
        "Loop\n"
    ),
        "Do\n"
        "    x += 1\n"
        "Loop\n"
    );
}

TEST_F(ReFormatterTests, WhileWend) {
    EXPECT_EQ(format(
        "While x > 0\n"
        "x -= 1\n"
        "Wend\n"
    ),
        "While x > 0\n"
        "    x -= 1\n"
        "Wend\n"
    );
}

TEST_F(ReFormatterTests, SelectCase) {
    EXPECT_EQ(format(
        "Select Case x\n"
        "Case 1\n"
        "Print \"one\"\n"
        "Case 2\n"
        "Print \"two\"\n"
        "Case Else\n"
        "Print \"other\"\n"
        "End Select\n"
    ),
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

TEST_F(ReFormatterTests, ScopeBlock) {
    EXPECT_EQ(format(
        "Scope\n"
        "Dim x = 1\n"
        "End Scope\n"
    ),
        "Scope\n"
        "    Dim x = 1\n"
        "End Scope\n"
    );
}

// ---------------------------------------------------------------------------
// Type — block vs alias form
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, TypeBlock) {
    EXPECT_EQ(format(
        "Type MyType\n"
        "x As Integer\n"
        "y As Integer\n"
        "End Type\n"
    ),
        "Type MyType\n"
        "    x As Integer\n"
        "    y As Integer\n"
        "End Type\n"
    );
}

TEST_F(ReFormatterTests, TypeAsAlias) {
    // `Type As <T> <name>` is an alias declaration, not a block.
    EXPECT_EQ(format(
        "Type As Integer MyInt\n"
        "Dim x As MyInt\n"
    ),
        "Type As Integer MyInt\n"
        "Dim x As MyInt\n"
    );
}

// ---------------------------------------------------------------------------
// Colon splitting (reFormat=true default)
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, ColonSplitsStatements) {
    EXPECT_EQ(format("x = 1 : y = 2\n"),
        "x = 1\n"
        "y = 2\n"
    );
}

TEST_F(ReFormatterTests, ColonSplitsIntoBlock) {
    EXPECT_EQ(format("If x Then : Print x : End If\n"),
        "If x Then\n"
        "    Print x\n"
        "End If\n"
    );
}

TEST_F(ReFormatterTests, ColonSplitNestedBlock) {
    EXPECT_EQ(format(
        "Sub Main : If x Then : Print x : End If : End Sub\n"
    ),
        "Sub Main\n"
        "    If x Then\n"
        "        Print x\n"
        "    End If\n"
        "End Sub\n"
    );
}

// ---------------------------------------------------------------------------
// Comments
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, StandaloneComment) {
    EXPECT_EQ(format(
        "' this is a comment\n"
        "Print x\n"
    ),
        "' this is a comment\n"
        "Print x\n"
    );
}

// ---------------------------------------------------------------------------
// Re-indent existing over-indented code
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, StripExistingIndent) {
    EXPECT_EQ(format(
        "Sub Main\n"
        "            Print \"over-indented\"\n"
        "End Sub\n"
    ),
        "Sub Main\n"
        "    Print \"over-indented\"\n"
        "End Sub\n"
    );
}

// ---------------------------------------------------------------------------
// Preprocessor blocks
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, PPIfdefElseEndif) {
    EXPECT_EQ(format(
        "#ifdef DEBUG\n"
        "#define LOG(x) Print x\n"
        "#else\n"
        "#define LOG(x)\n"
        "#endif\n"
    ),
        "#ifdef DEBUG\n"
        "    #define LOG(x) Print x\n"
        "#else\n"
        "    #define LOG(x)\n"
        "#endif\n"
    );
}

TEST_F(ReFormatterTests, PPNested) {
    EXPECT_EQ(format(
        "#ifdef A\n"
        "#ifdef B\n"
        "#define X 1\n"
        "#endif\n"
        "#endif\n"
    ),
        "#ifdef A\n"
        "    #ifdef B\n"
        "        #define X 1\n"
        "    #endif\n"
        "#endif\n"
    );
}

TEST_F(ReFormatterTests, PPInsideCodeBlock) {
    EXPECT_EQ(format(
        "Sub Main\n"
        "#ifdef DEBUG\n"
        "Print \"debug\"\n"
        "#endif\n"
        "Print \"hello\"\n"
        "End Sub\n"
    ),
        "Sub Main\n"
        "    #ifdef DEBUG\n"
        "        Print \"debug\"\n"
        "    #endif\n"
        "    Print \"hello\"\n"
        "End Sub\n"
    );
}

TEST_F(ReFormatterTests, PPMacroBlock) {
    EXPECT_EQ(format(
        "#macro MyMacro(x)\n"
        "Print x\n"
        "#endmacro\n"
    ),
        "#macro MyMacro(x)\n"
        "    Print x\n"
        "#endmacro\n"
    );
}

TEST_F(ReFormatterTests, PPElseIfVariants) {
    EXPECT_EQ(format(
        "#ifdef A\n"
        "#define X 1\n"
        "#elseifdef B\n"
        "#define X 2\n"
        "#elseifndef C\n"
        "#define X 3\n"
        "#elseif D = 1\n"
        "#define X 4\n"
        "#else\n"
        "#define X 0\n"
        "#endif\n"
    ),
        "#ifdef A\n"
        "    #define X 1\n"
        "#elseifdef B\n"
        "    #define X 2\n"
        "#elseifndef C\n"
        "    #define X 3\n"
        "#elseif D = 1\n"
        "    #define X 4\n"
        "#else\n"
        "    #define X 0\n"
        "#endif\n"
    );
}

TEST_F(ReFormatterTests, PPNonBlockIncludeIndentsWithSurrounding) {
    // #include is not a PP block — it indents with whatever surrounds it.
    EXPECT_EQ(format(
        "Sub Main\n"
        "#include \"file.bi\"\n"
        "Print x\n"
        "End Sub\n"
    ),
        "Sub Main\n"
        "    #include \"file.bi\"\n"
        "    Print x\n"
        "End Sub\n"
    );
}

TEST_F(ReFormatterTests, PPBoundaryClosesUnclosedCodeBlock) {
    // #else arrives while `if x then` is still open — auto-close at the
    // PP boundary so indent resets on the #else branch.
    EXPECT_EQ(format(
        "#ifdef DEBUG\n"
        "if x then\n"
        "print x\n"
        "#else\n"
        "print y\n"
        "#endif\n"
    ),
        "#ifdef DEBUG\n"
        "    if x then\n"
        "        print x\n"
        "#else\n"
        "    print y\n"
        "#endif\n"
    );
}

TEST_F(ReFormatterTests, PPEmptyBlock) {
    EXPECT_EQ(format(
        "#ifdef DEBUG\n"
        "#endif\n"
    ),
        "#ifdef DEBUG\n"
        "#endif\n"
    );
}

// ---------------------------------------------------------------------------
// Malformed input — recovers gracefully
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, ExtraClosersAtTopLevel) {
    EXPECT_EQ(format(
        "End Sub\n"
        "End If\n"
        "Next\n"
    ),
        "End Sub\n"
        "End If\n"
        "Next\n"
    );
}

TEST_F(ReFormatterTests, UnmatchedCodeClosersInsidePP) {
    // `end if` without matching `if` — emitted as statement, never closes the PP block.
    EXPECT_EQ(format(
        "if x then\n"
        "#if TEST\n"
        "end if\n"
        "end if\n"
        "#endif\n"
        "end if\n"
    ),
        "if x then\n"
        "    #if TEST\n"
        "        end if\n"
        "        end if\n"
        "    #endif\n"
        "end if\n"
    );
}

// ---------------------------------------------------------------------------
// Blank lines
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, BlankLinesPreserved) {
    EXPECT_EQ(format(
        "x = 1\n"
        "\n"
        "y = 2\n"
    ),
        "x = 1\n"
        "\n"
        "y = 2\n"
    );
}

TEST_F(ReFormatterTests, MultipleBlankLinesCollapsed) {
    EXPECT_EQ(format(
        "x = 1\n"
        "\n"
        "\n"
        "\n"
        "y = 2\n"
    ),
        "x = 1\n"
        "\n"
        "y = 2\n"
    );
}

TEST_F(ReFormatterTests, BlankLineInsertedBetweenDefinitions) {
    // Adjacent Sub definitions get a blank line between them.
    EXPECT_EQ(format(
        "Sub Foo\n"
        "End Sub\n"
        "Sub Bar\n"
        "End Sub\n"
    ),
        "Sub Foo\n"
        "End Sub\n"
        "\n"
        "Sub Bar\n"
        "End Sub\n"
    );
}

TEST_F(ReFormatterTests, NoBlankLineBetweenNonDefinitions) {
    // For/While are not definitions — no auto blank line.
    EXPECT_EQ(format(
        "Sub Main\n"
        "For i = 1 To 10\n"
        "Next\n"
        "While x > 0\n"
        "Wend\n"
        "End Sub\n"
    ),
        "Sub Main\n"
        "    For i = 1 To 10\n"
        "    Next\n"
        "    While x > 0\n"
        "    Wend\n"
        "End Sub\n"
    );
}

TEST_F(ReFormatterTests, ExistingBlankLineNotDuplicated) {
    EXPECT_EQ(format(
        "Sub Foo\n"
        "End Sub\n"
        "\n"
        "Sub Bar\n"
        "End Sub\n"
    ),
        "Sub Foo\n"
        "End Sub\n"
        "\n"
        "Sub Bar\n"
        "End Sub\n"
    );
}

// ---------------------------------------------------------------------------
// Spacing — transformative inputs exercising needsSpaceBefore rules
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, SpacingNormalizesBinaryOperators) {
    // Arithmetic / comparison / shift / compound-assign all get single-space padding.
    EXPECT_EQ(format(
        "x=a+b*c-d\n"
        "y=x>=0\n"
        "z=y<<2\n"
        "x+=1\n"
    ),
        "x = a + b * c - d\n"
        "y = x >= 0\n"
        "z = y << 2\n"
        "x += 1\n"
    );
}

TEST_F(ReFormatterTests, SpacingUnaryOperatorsHaveNoTrailingSpace) {
    // Unary -, *, @ hug their operand.
    EXPECT_EQ(format(
        "x = -3\n"
        "y = a + -b\n"
        "v = *ptr\n"
        "p = @x\n"
    ),
        "x = -3\n"
        "y = a + -b\n"
        "v = *ptr\n"
        "p = @x\n"
    );
}

TEST_F(ReFormatterTests, SpacingCallsAndIndexingVsGrouping) {
    // After an identifier/keyword/closing, '(' and '[' are call/index — no space.
    // After an operator, '(' is grouping — space before.
    EXPECT_EQ(format(
        "x=foo(a,b,c)\n"
        "y=a[i]\n"
        "z=(a+b)*c\n"
    ),
        "x = foo(a, b, c)\n"
        "y = a[i]\n"
        "z = (a + b) * c\n"
    );
}

TEST_F(ReFormatterTests, SpacingMemberAccessAndBraces) {
    EXPECT_EQ(format(
        "x = foo.bar\n"
        "y = ptr->field\n"
        "z = { 1, 2, 3 }\n"
    ),
        "x = foo.bar\n"
        "y = ptr->field\n"
        "z = { 1, 2, 3 }\n"
    );
}

// ---------------------------------------------------------------------------
// Anchored # mode
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, AnchoredHashSimple) {
    EXPECT_EQ(format(
        "#ifdef DEBUG\n"
        "#define X 1\n"
        "#endif\n",
        true
    ),
        "#ifdef DEBUG\n"
        "#   define X 1\n"
        "#endif\n"
    );
}

TEST_F(ReFormatterTests, AnchoredHashInsideCodeBlock) {
    EXPECT_EQ(format(
        "Sub Main\n"
        "#ifdef DEBUG\n"
        "print x\n"
        "#endif\n"
        "End Sub\n",
        true
    ),
        "Sub Main\n"
        "#   ifdef DEBUG\n"
        "        print x\n"
        "#   endif\n"
        "End Sub\n"
    );
}

TEST_F(ReFormatterTests, AnchoredHashBranchesStayAtParentDepth) {
    // #else/#endif are branches/closers of the enclosing #if — their
    // directive keyword must sit at the same indent column as #if.
    EXPECT_EQ(format(
        "Sub Main\n"
        "#if DEBUG\n"
        "print \"dbg\"\n"
        "#else\n"
        "print \"rel\"\n"
        "#endif\n"
        "End Sub\n",
        true
    ),
        "Sub Main\n"
        "#   if DEBUG\n"
        "        print \"dbg\"\n"
        "#   else\n"
        "        print \"rel\"\n"
        "#   endif\n"
        "End Sub\n"
    );
}

TEST_F(ReFormatterTests, AnchoredHashBranchesInsideNestedBlocks) {
    // Reproduction of the operator-with-#if-#else case: branches at
    // deep code nesting should still render at the #if's column.
    EXPECT_EQ(format(
        "Type Foo\n"
        "Operator Cast() As Integer\n"
        "#if FAST\n"
        "Return 1\n"
        "#else\n"
        "Return 0\n"
        "#endif\n"
        "End Operator\n"
        "End Type\n",
        true
    ),
        "Type Foo\n"
        "    Operator Cast() As Integer\n"
        "#       if FAST\n"
        "            Return 1\n"
        "#       else\n"
        "            Return 0\n"
        "#       endif\n"
        "    End Operator\n"
        "End Type\n"
    );
}

// ---------------------------------------------------------------------------
// reIndent = false — preserve original leading whitespace
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, PreserveIndent_NonStandardIndent) {
    // 2-space indent preserved (would be re-indented to 4 with reIndent=true).
    EXPECT_EQ(formatWith(
        "Sub Main\n"
        "  Print \"a\"\n"
        "End Sub\n",
        { .tabSize = tabSize, .reIndent = false }
    ),
        "Sub Main\n"
        "  Print \"a\"\n"
        "End Sub\n"
    );
}

TEST_F(ReFormatterTests, PreserveIndent_MixedTabsAndSpaces) {
    EXPECT_EQ(formatWith(
        "Sub Main\n"
        "\t  Print x\n"
        "End Sub\n",
        { .tabSize = tabSize, .reIndent = false }
    ),
        "Sub Main\n"
        "\t  Print x\n"
        "End Sub\n"
    );
}

TEST_F(ReFormatterTests, PreserveIndent_InterTokenStillNormalized) {
    // reIndent=false but reFormat=true — indent kept, inter-token spacing still normalized.
    EXPECT_EQ(formatWith(
        "    x=1+2\n",
        { .tabSize = tabSize, .reIndent = false }
    ),
        "    x = 1 + 2\n"
    );
}

// ---------------------------------------------------------------------------
// reFormat = false — preserve inter-token whitespace and original layout
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, PreserveFormat_QuirkySpacing) {
    EXPECT_EQ(formatWith(
        "x=  1  +  2\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "x=  1  +  2\n"
    );
}

TEST_F(ReFormatterTests, PreserveFormat_ContinuationPreserved) {
    EXPECT_EQ(formatWith(
        "Dim x = 1 _\n"
        "        + 2\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "Dim x = 1 _\n"
        "        + 2\n"
    );
}

TEST_F(ReFormatterTests, PreserveFormat_ColonSeparated) {
    // Colon stays inline (default behavior would split onto multiple lines).
    EXPECT_EQ(formatWith(
        "x = 1 : y = 2\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "x = 1 : y = 2\n"
    );
}

TEST_F(ReFormatterTests, PreserveFormat_ForNextOnOneLine) {
    // Self-contained `For ... Next` on one line must not open a phantom block.
    EXPECT_EQ(formatWith(
        "For i = 1 To 10 : Print i : Next\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "For i = 1 To 10 : Print i : Next\n"
    );
}

TEST_F(ReFormatterTests, PreserveFormat_BlankLineRunsPreserved) {
    EXPECT_EQ(formatWith(
        "x = 1\n"
        "\n"
        "\n"
        "\n"
        "y = 2\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "x = 1\n"
        "\n"
        "\n"
        "\n"
        "y = 2\n"
    );
}

TEST_F(ReFormatterTests, PreserveFormat_NoAutoBlankBetweenDefs) {
    EXPECT_EQ(formatWith(
        "Sub A\n"
        "End Sub\n"
        "Sub B\n"
        "End Sub\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "Sub A\n"
        "End Sub\n"
        "Sub B\n"
        "End Sub\n"
    );
}

// ---------------------------------------------------------------------------
// Flag interactions
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, AnchoredPPBypassedWhenPreservingIndent) {
    // anchoredPP only applies when rebuilding indent.
    EXPECT_EQ(formatWith(
        "Sub Main\n"
        "  #ifdef DEBUG\n"
        "  Print x\n"
        "  #endif\n"
        "End Sub\n",
        { .tabSize = tabSize, .anchoredPP = true, .reIndent = false }
    ),
        "Sub Main\n"
        "  #ifdef DEBUG\n"
        "  Print x\n"
        "  #endif\n"
        "End Sub\n"
    );
}

TEST_F(ReFormatterTests, PassthroughWithBothFlagsOff) {
    const char* source =
        "Sub Main\n"
        "  x=1+2\n"
        "  Print x\n"
        "End Sub\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}

// ---------------------------------------------------------------------------
// Round-trip acceptance — {reIndent=false, reFormat=false} preserves source
// byte-for-byte on well-formed input.
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, RoundTrip_ContinuationAndColons) {
    const char* source =
        "Dim total = 1 _\n"
        "          + 2 _\n"
        "          + 3\n"
        "x = 1 : y = 2 : z = 3\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}

TEST_F(ReFormatterTests, RoundTrip_PPDirectives) {
    const char* source =
        "Sub Main\n"
        "  #ifdef DEBUG\n"
        "    Print \"dbg\"\n"
        "  #else\n"
        "    Print \"rel\"\n"
        "  #endif\n"
        "End Sub\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}

TEST_F(ReFormatterTests, RoundTrip_BlankLineRuns) {
    const char* source =
        "Sub A\n"
        "End Sub\n"
        "\n"
        "\n"
        "\n"
        "Sub B\n"
        "End Sub\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}
