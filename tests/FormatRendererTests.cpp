//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "lib/analyses/lexer/Lexer.hpp"
#include "lib/config/Keywords.hpp"
#include "../src/lib/format/formatters/reformat/ReFormatter.hpp"
#include <gtest/gtest.h>

using namespace fbide;
using namespace fbide::format;

class FormatRendererTests : public testing::Test {
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
// Simple statements
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, SingleStatement) {
    EXPECT_EQ(format("Print \"hello\"\n"), "Print \"hello\"\n");
}

TEST_F(FormatRendererTests, MultipleStatements) {
    EXPECT_EQ(format("x = 1\ny = 2\n"), "x = 1\ny = 2\n");
}

// ---------------------------------------------------------------------------
// Sub / Function blocks
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, SubBlock) {
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

TEST_F(FormatRendererTests, NestedBlocks) {
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

// ---------------------------------------------------------------------------
// If / Then / Else
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, MultiLineIfThen) {
    EXPECT_EQ(format(
        "If x > 0 Then\n"
        "Print x\n"
        "End If\n"
    ),
        "If x > 0 Then\n"
        "    Print x\n"
        "End If\n"
    );
}

TEST_F(FormatRendererTests, MultiLineIfThenElse) {
    EXPECT_EQ(format(
        "If x > 0 Then\n"
        "Print \"pos\"\n"
        "Else\n"
        "Print \"neg\"\n"
        "End If\n"
    ),
        "If x > 0 Then\n"
        "    Print \"pos\"\n"
        "Else\n"
        "    Print \"neg\"\n"
        "End If\n"
    );
}

TEST_F(FormatRendererTests, MultiLineIfThenElseIf) {
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

TEST_F(FormatRendererTests, SingleLineIfNotIndented) {
    EXPECT_EQ(format(
        "If x > 0 Then Print x\n"
        "Print \"done\"\n"
    ),
        "If x > 0 Then Print x\n"
        "Print \"done\"\n"
    );
}

// ---------------------------------------------------------------------------
// Select / Case
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, SelectCase) {
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

// ---------------------------------------------------------------------------
// For / Do / While
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, ForNext) {
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

TEST_F(FormatRendererTests, DoLoop) {
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

TEST_F(FormatRendererTests, WhileWend) {
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

// ---------------------------------------------------------------------------
// Type / Scope
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, TypeBlock) {
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

TEST_F(FormatRendererTests, ScopeBlock) {
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
// Preprocessor blocks
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, PPIfdefEndif) {
    EXPECT_EQ(format(
        "#ifdef DEBUG\n"
        "#define LOG(x) Print x\n"
        "#endif\n"
    ),
        "#ifdef DEBUG\n"
        "    #define LOG(x) Print x\n"
        "#endif\n"
    );
}

TEST_F(FormatRendererTests, PPIfdefElseEndif) {
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

TEST_F(FormatRendererTests, PPInsideCodeBlock) {
    EXPECT_EQ(format(
        "Sub Main\n"
        "#ifdef DEBUG\n"
        "Print \"debug\"\n"
        "#endif\n"
        "End Sub\n"
    ),
        "Sub Main\n"
        "    #ifdef DEBUG\n"
        "        Print \"debug\"\n"
        "    #endif\n"
        "End Sub\n"
    );
}

TEST_F(FormatRendererTests, PPCodeIndentResetAtElse) {
    EXPECT_EQ(format(
        "#ifdef DEBUG\n"
        "if x then\n"
        "print x\n"
        "end if\n"
        "#else\n"
        "if y then\n"
        "print y\n"
        "end if\n"
        "#endif\n"
    ),
        "#ifdef DEBUG\n"
        "    if x then\n"
        "        print x\n"
        "    end if\n"
        "#else\n"
        "    if y then\n"
        "        print y\n"
        "    end if\n"
        "#endif\n"
    );
}

TEST_F(FormatRendererTests, PPNested) {
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

// ---------------------------------------------------------------------------
// Colon splitting
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, ColonSplitsIntoBlock) {
    EXPECT_EQ(format(
        "If x Then : Print x : End If\n"
    ),
        "If x Then\n"
        "    Print x\n"
        "End If\n"
    );
}

// ---------------------------------------------------------------------------
// Strip existing indent
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, StripExistingIndent) {
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
// Blank lines
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, BlankLinesPreserved) {
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

// ---------------------------------------------------------------------------
// Spacing — binary operators
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, SpacingBinaryArithmetic) {
    EXPECT_EQ(format("x = a + b\n"), "x = a + b\n");
}

TEST_F(FormatRendererTests, SpacingCompoundAssign) {
    EXPECT_EQ(format("x += 1\n"), "x += 1\n");
}

TEST_F(FormatRendererTests, SpacingComparison) {
    EXPECT_EQ(format("If x >= 0 Then\nEnd If\n"), "If x >= 0 Then\nEnd If\n");
}

TEST_F(FormatRendererTests, SpacingShiftOperator) {
    EXPECT_EQ(format("x = y << 2\n"), "x = y << 2\n");
}

// ---------------------------------------------------------------------------
// Spacing — unary operators
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, SpacingUnaryNegate) {
    EXPECT_EQ(format("x = -3\n"), "x = -3\n");
}

TEST_F(FormatRendererTests, SpacingUnaryInExpression) {
    EXPECT_EQ(format("x = a + -b\n"), "x = a + -b\n");
}

TEST_F(FormatRendererTests, SpacingDereference) {
    EXPECT_EQ(format("x = *ptr\n"), "x = *ptr\n");
}

TEST_F(FormatRendererTests, SpacingAddressOf) {
    EXPECT_EQ(format("p = @x\n"), "p = @x\n");
}

// ---------------------------------------------------------------------------
// Spacing — parens and brackets
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, SpacingFunctionCall) {
    EXPECT_EQ(format("foo(x)\n"), "foo(x)\n");
}

TEST_F(FormatRendererTests, SpacingFunctionCallMultiArg) {
    EXPECT_EQ(format("foo(a, b, c)\n"), "foo(a, b, c)\n");
}

TEST_F(FormatRendererTests, SpacingArrayIndex) {
    EXPECT_EQ(format("a[i]\n"), "a[i]\n");
}

TEST_F(FormatRendererTests, SpacingNestedParens) {
    EXPECT_EQ(format("x = (a + b)\n"), "x = (a + b)\n");
}

// ---------------------------------------------------------------------------
// Spacing — braces
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, SpacingBraces) {
    EXPECT_EQ(format("x = { 1, 2, 3 }\n"), "x = { 1, 2, 3 }\n");
}

// ---------------------------------------------------------------------------
// Spacing — dot and arrow
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, SpacingDotAccess) {
    EXPECT_EQ(format("x.y\n"), "x.y\n");
}

TEST_F(FormatRendererTests, SpacingArrowAccess) {
    EXPECT_EQ(format("p->x\n"), "p->x\n");
}

// ---------------------------------------------------------------------------
// Spacing — keywords
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, SpacingKeywords) {
    EXPECT_EQ(format("Dim x As Integer\n"), "Dim x As Integer\n");
}

// ---------------------------------------------------------------------------
// Spacing — semicolon (Print separator)
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, SpacingSemicolon) {
    EXPECT_EQ(format("Print a ; b\n"), "Print a ; b\n");
}

// ---------------------------------------------------------------------------
// Blank lines — collapse and enforcement
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, MultipleBlankLinesCollapsed) {
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

TEST_F(FormatRendererTests, BlankLineBetweenDefinitions) {
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

TEST_F(FormatRendererTests, BlankLineBetweenFunctionAndType) {
    EXPECT_EQ(format(
        "Function Foo\n"
        "End Function\n"
        "Type Bar\n"
        "End Type\n"
    ),
        "Function Foo\n"
        "End Function\n"
        "\n"
        "Type Bar\n"
        "End Type\n"
    );
}

TEST_F(FormatRendererTests, NoBlankLineBetweenNonDefinitions) {
    // For/While are not definitions — no extra blank line
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

TEST_F(FormatRendererTests, ExistingBlankLineNotDuplicated) {
    // Already has a blank line — don't add another
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
// PP crossing code blocks — unclosed code blocks auto-close at PP boundary
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, PPElseClosesUnclosedCodeBlock) {
    // if x then is unclosed when #else arrives — auto-close it
    EXPECT_EQ(format(
        "#if _FBMAP_STOREHASH\n"
        "if x then\n"
        "#else\n"
        "print foo\n"
        "#endif\n"
    ),
        "#if _FBMAP_STOREHASH\n"
        "    if x then\n"
        "#else\n"
        "    print foo\n"
        "#endif\n"
    );
}

TEST_F(FormatRendererTests, PPWithUnmatchedCodeClosers) {
    // end if inside PP has no matching if — treated as statements
    EXPECT_EQ(format(
        "if x then\n"
        "#if TEST\n"
        "end if\n"
        "end if\n"
        "#else\n"
        "end if\n"
        "end if\n"
        "#endif\n"
        "end if\n"
    ),
        "if x then\n"
        "    #if TEST\n"
        "        end if\n"
        "        end if\n"
        "    #else\n"
        "        end if\n"
        "        end if\n"
        "    #endif\n"
        "end if\n"
    );
}

TEST_F(FormatRendererTests, PPEndifClosesUnclosedCodeBlock) {
    // For loop unclosed when #endif arrives
    EXPECT_EQ(format(
        "#ifdef DEBUG\n"
        "for i = 1 to 10\n"
        "print i\n"
        "#endif\n"
    ),
        "#ifdef DEBUG\n"
        "    for i = 1 to 10\n"
        "        print i\n"
        "#endif\n"
    );
}

// ---------------------------------------------------------------------------
// PP — elseif with unmatched closers
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, PPElseIfWithUnmatchedClosers) {
    EXPECT_EQ(format(
        "#ifdef A\n"
        "if x then\n"
        "#elseif B\n"
        "if y then\n"
        "#endif\n"
    ),
        "#ifdef A\n"
        "    if x then\n"
        "#elseif B\n"
        "    if y then\n"
        "#endif\n"
    );
}

// ---------------------------------------------------------------------------
// PP — multiple PP blocks inside code
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, MultiplePPBlocksInsideCode) {
    EXPECT_EQ(format(
        "Sub Main\n"
        "#ifdef A\n"
        "print 1\n"
        "#endif\n"
        "#ifdef B\n"
        "print 2\n"
        "#endif\n"
        "End Sub\n"
    ),
        "Sub Main\n"
        "    #ifdef A\n"
        "        print 1\n"
        "    #endif\n"
        "    #ifdef B\n"
        "        print 2\n"
        "    #endif\n"
        "End Sub\n"
    );
}

// ---------------------------------------------------------------------------
// PP — empty PP blocks
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, EmptyPPBlock) {
    EXPECT_EQ(format(
        "#ifdef DEBUG\n"
        "#endif\n"
    ),
        "#ifdef DEBUG\n"
        "#endif\n"
    );
}

// ---------------------------------------------------------------------------
// Declare — various forms don't open blocks
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, DeclareFunction) {
    EXPECT_EQ(format(
        "Declare Function Add(a As Integer, b As Integer) As Integer\n"
        "Dim x = 1\n"
    ),
        "Declare Function Add(a As Integer, b As Integer) As Integer\n"
        "Dim x = 1\n"
    );
}

// ---------------------------------------------------------------------------
// Function = value (property setter)
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, ExitSubNotBlockOpener) {
    EXPECT_EQ(format(
        "If True Then\n"
        "Exit Sub\n"
        "End If\n"
    ),
        "If True Then\n"
        "    Exit Sub\n"
        "End If\n"
    );
}

TEST_F(FormatRendererTests, ExitForNotBlockOpener) {
    EXPECT_EQ(format(
        "For i = 1 To 10\n"
        "Exit For\n"
        "Next\n"
    ),
        "For i = 1 To 10\n"
        "    Exit For\n"
        "Next\n"
    );
}

TEST_F(FormatRendererTests, FunctionPropertySetter) {
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
// Deeply nested blocks
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, DeeplyNestedBlocks) {
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
// Colon splitting with nested blocks
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, ColonSplitNestedBlock) {
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
// Malformed input — extra closers at top level
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, ExtraClosersAtTopLevel) {
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

// ---------------------------------------------------------------------------
// Mixed code and PP with proper nesting
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, PPNestedWithCodeAndBranches) {
    EXPECT_EQ(format(
        "#ifdef A\n"
        "#ifdef B\n"
        "print x\n"
        "#endif\n"
        "print y\n"
        "#else\n"
        "print z\n"
        "#endif\n"
    ),
        "#ifdef A\n"
        "    #ifdef B\n"
        "        print x\n"
        "    #endif\n"
        "    print y\n"
        "#else\n"
        "    print z\n"
        "#endif\n"
    );
}

// ---------------------------------------------------------------------------
// Comment preservation
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, CommentAfterBlockOpener) {
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

TEST_F(FormatRendererTests, StandaloneComment) {
    EXPECT_EQ(format(
        "' this is a comment\n"
        "Print x\n"
    ),
        "' this is a comment\n"
        "Print x\n"
    );
}

// ---------------------------------------------------------------------------
// Anchored hash mode
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, AnchoredHashSimple) {
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

TEST_F(FormatRendererTests, AnchoredHashNested) {
    EXPECT_EQ(format(
        "#if 1\n"
        "#if 1\n"
        "print \"hello\"\n"
        "#endif\n"
        "#endif\n",
        true
    ),
        "#if 1\n"
        "#   if 1\n"
        "        print \"hello\"\n"
        "#   endif\n"
        "#endif\n"
    );
}

TEST_F(FormatRendererTests, AnchoredHashInsideCodeBlock) {
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

// ---------------------------------------------------------------------------
// reIndent = false — preserve original leading whitespace
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, PreserveIndent_SimpleSubBody) {
    // Body has no indent in source — preserved as-is.
    EXPECT_EQ(formatWith(
        "Sub Main\n"
        "Print \"hello\"\n"
        "End Sub\n",
        { .tabSize = tabSize, .reIndent = false }
    ),
        "Sub Main\n"
        "Print \"hello\"\n"
        "End Sub\n"
    );
}

TEST_F(FormatRendererTests, PreserveIndent_NonStandardIndent) {
    // Source uses 2-space indent; preserved (formatter would otherwise use 4).
    EXPECT_EQ(formatWith(
        "Sub Main\n"
        "  Print \"a\"\n"
        "  Print \"b\"\n"
        "End Sub\n",
        { .tabSize = tabSize, .reIndent = false }
    ),
        "Sub Main\n"
        "  Print \"a\"\n"
        "  Print \"b\"\n"
        "End Sub\n"
    );
}

TEST_F(FormatRendererTests, PreserveIndent_MixedTabsAndSpaces) {
    // Tab followed by two spaces — echoed verbatim.
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

TEST_F(FormatRendererTests, PreserveIndent_OverIndented) {
    // Source over-indents for readability; formatter respects it.
    EXPECT_EQ(formatWith(
        "If x Then\n"
        "            Print x\n"
        "End If\n",
        { .tabSize = tabSize, .reIndent = false }
    ),
        "If x Then\n"
        "            Print x\n"
        "End If\n"
    );
}

TEST_F(FormatRendererTests, PreserveIndent_InterTokenStillNormalized) {
    // reIndent=false but reFormat=true — inter-token spacing still normalized.
    EXPECT_EQ(formatWith(
        "    x=1+2\n",
        { .tabSize = tabSize, .reIndent = false }
    ),
        "    x = 1 + 2\n"
    );
}

// ---------------------------------------------------------------------------
// reFormat = false — preserve inter-token whitespace
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, PreserveFormat_QuirkySpacing) {
    // Inter-token spacing echoed verbatim; indent still rebuilt.
    EXPECT_EQ(formatWith(
        "x=  1  +  2\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "x=  1  +  2\n"
    );
}

TEST_F(FormatRendererTests, PreserveFormat_NoSpaceAroundEquals) {
    EXPECT_EQ(formatWith(
        "x=1\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "x=1\n"
    );
}

TEST_F(FormatRendererTests, PreserveFormat_IndentRebuiltInSubBody) {
    // reFormat=false but reIndent=true — body lines re-indented, but the
    // existing source already uses 4-space indent so output matches.
    EXPECT_EQ(formatWith(
        "Sub Main\n"
        "    x=1\n"
        "End Sub\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "Sub Main\n"
        "    x=1\n"
        "End Sub\n"
    );
}

TEST_F(FormatRendererTests, PreserveFormat_ContinuationPreserved) {
    // Continuation newline and following indent echoed verbatim.
    EXPECT_EQ(formatWith(
        "Dim x = 1 _\n"
        "        + 2\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "Dim x = 1 _\n"
        "        + 2\n"
    );
}

TEST_F(FormatRendererTests, PreserveFormat_ColonSeparated) {
    // With reFormat=true, these split to two lines. With reFormat=false,
    // the colon stays inline.
    EXPECT_EQ(formatWith(
        "x = 1 : y = 2\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "x = 1 : y = 2\n"
    );
}

TEST_F(FormatRendererTests, PreserveFormat_ForNextOnOneLine) {
    // `For i = 1 To 10 : Print i : Next` is self-contained — no phantom block.
    EXPECT_EQ(formatWith(
        "For i = 1 To 10 : Print i : Next\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "For i = 1 To 10 : Print i : Next\n"
    );
}

TEST_F(FormatRendererTests, PreserveFormat_IfThenEndIfOnOneLine) {
    EXPECT_EQ(formatWith(
        "If x Then : y = 1 : End If\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "If x Then : y = 1 : End If\n"
    );
}

TEST_F(FormatRendererTests, PreserveFormat_BlankLineRunsPreserved) {
    // Three blank lines between statements preserved under reFormat=false.
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

TEST_F(FormatRendererTests, PreserveFormat_SingleBlankLinePreserved) {
    EXPECT_EQ(formatWith(
        "x = 1\n"
        "\n"
        "y = 2\n",
        { .tabSize = tabSize, .reFormat = false }
    ),
        "x = 1\n"
        "\n"
        "y = 2\n"
    );
}

TEST_F(FormatRendererTests, PreserveIndent_AnchoredPPBypassed) {
    // anchoredPP is a reindent-time override; with reIndent=false, the
    // source's own leading whitespace wins, so no anchor rewrite happens.
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

TEST_F(FormatRendererTests, PreserveFormat_NoAutoBlankBetweenDefs) {
    // reFormat=true would insert a blank line between the two Subs;
    // reFormat=false preserves the user's (lack of) blank line.
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

TEST_F(FormatRendererTests, PreserveFormatAndIndent_Passthrough) {
    // Both flags off → near-identity for well-formed input.
    const char* source =
        "Sub Main\n"
        "  x=1+2\n"
        "  Print x\n"
        "End Sub\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}

// ---------------------------------------------------------------------------
// Round-trip acceptance: both flags off preserves source byte-for-byte.
// ---------------------------------------------------------------------------

TEST_F(FormatRendererTests, RoundTrip_MixedTabsAndSpaces) {
    const char* source =
        "Sub Main\n"
        "\tx = 1\n"
        "\t  y = 2\n"
        "End Sub\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}

TEST_F(FormatRendererTests, RoundTrip_ContinuationAndColons) {
    const char* source =
        "Dim total = 1 _\n"
        "          + 2 _\n"
        "          + 3\n"
        "x = 1 : y = 2 : z = 3\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}

TEST_F(FormatRendererTests, RoundTrip_PPDirectives) {
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

TEST_F(FormatRendererTests, RoundTrip_BlankLineRuns) {
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

TEST_F(FormatRendererTests, RoundTrip_QuirkySpacing) {
    const char* source =
        "x=1+2*3\n"
        "y = foo(a,b ,c)\n"
        "z =   42\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}

TEST_F(FormatRendererTests, RoundTrip_OddIndentNested) {
    const char* source =
        "If x Then\n"
        "   For i = 1 To 10\n"
        "      Print i\n"
        "   Next\n"
        "End If\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}
