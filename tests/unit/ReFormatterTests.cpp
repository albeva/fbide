//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "../src/lib/format/transformers/case/CaseTransform.hpp"
#include "../src/lib/format/transformers/reformat/ReFormatter.hpp"
#include "TestHelpers.hpp"

using namespace fbide;
using namespace fbide::reformat;

class ReFormatterTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;
    static constexpr std::size_t tabSize = 4;

    void SetUp() override {
        m_lexer = tests::createFbLexer(testDataPath + "fbfull.lng");
    }

    void TearDown() override {
        m_lexer->Release();
        m_lexer = nullptr;
    }

    auto format(const char* source, const bool anchoredPP = false) -> std::string {
        return formatWith(source, { .tabSize = tabSize, .anchoredPP = anchoredPP });
    }

    auto formatWith(const char* source, const FormatOptions& options) -> std::string {
        const auto tokens = tests::tokenise(*m_lexer, source);
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

    Scintilla::ILexer5* m_lexer { nullptr };
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
        "End Sub\n");
}

TEST_F(ReFormatterTests, AsmBlock) {
    // Multi-line `asm … end asm` — opens a block, body is indented.
    EXPECT_EQ(format(
                  "Sub Foo\n"
                  "Asm\n"
                  "mov eax, 0\n"
                  "End Asm\n"
                  "End Sub\n"
              ),
        "Sub Foo\n"
        "    Asm\n"
        "        mov eax, 0\n"
        "    End Asm\n"
        "End Sub\n");
}

TEST_F(ReFormatterTests, SingleLineAsmDoesNotOpenBlock) {
    // FB single-line `asm <stmt>` is one statement — must not be treated
    // as a compound-statement opener; the next line's indent is unchanged.
    EXPECT_EQ(format(
                  "Sub Foo\n"
                  "Asm mov eax, 10\n"
                  "Print x\n"
                  "End Sub\n"
              ),
        "Sub Foo\n"
        "    Asm mov eax, 10\n"
        "    Print x\n"
        "End Sub\n");
}

TEST_F(ReFormatterTests, FunctionBlock) {
    EXPECT_EQ(format(
                  "Function Add(a As Integer, b As Integer) As Integer\n"
                  "Return a + b\n"
                  "End Function\n"
              ),
        "Function Add(a As Integer, b As Integer) As Integer\n"
        "    Return a + b\n"
        "End Function\n");
}

TEST_F(ReFormatterTests, ConstructorBlock) {
    EXPECT_EQ(format(
                  "Constructor MyType()\n"
                  "x = 1\n"
                  "End Constructor\n"
              ),
        "Constructor MyType()\n"
        "    x = 1\n"
        "End Constructor\n");
}

TEST_F(ReFormatterTests, DestructorBlock) {
    EXPECT_EQ(format(
                  "Destructor MyType()\n"
                  "x = 0\n"
                  "End Destructor\n"
              ),
        "Destructor MyType()\n"
        "    x = 0\n"
        "End Destructor\n");
}

TEST_F(ReFormatterTests, OperatorBlock) {
    EXPECT_EQ(format(
                  "Operator MyType.Cast() As String\n"
                  "Return \"hello\"\n"
                  "End Operator\n"
              ),
        "Operator MyType.Cast() As String\n"
        "    Return \"hello\"\n"
        "End Operator\n");
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
        "Dim x = 1\n");
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
        "End Function\n");
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
        "End If\n");
}

TEST_F(ReFormatterTests, ExitLoopKeywordsDoNotOpenBlock) {
    EXPECT_EQ(format(
                  "If true Then\n"
                  "Exit Do, Do\n"
                  "End If\n"
              ),
        "If true Then\n"
        "    Exit Do, Do\n"
        "End If\n");

    EXPECT_EQ(format(
                  "Do\n"
                  "Exit Do\n"
                  "Loop\n"
              ),
        "Do\n"
        "    Exit Do\n"
        "Loop\n");

    EXPECT_EQ(format(
                  "For i = 1 To 10\n"
                  "Continue For\n"
                  "Next\n"
              ),
        "For i = 1 To 10\n"
        "    Continue For\n"
        "Next\n");
}

// ---------------------------------------------------------------------------
// Keyword reuse — only the FIRST word-like token of a statement decides
// whether a block opens. Subsequent reused keywords (e.g. `For` inside
// `Open ... For Input`) must not push a block onto the stack.
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, OpenForInputDoesNotOpenForBlock) {
    EXPECT_EQ(format(
                  "Open \"somefile\" For Input As #f\n"
                  "Print \"this is not a FOR loop and should not indent\"\n"
              ),
        "Open \"somefile\" For Input As # f\n"
        "Print \"this is not a FOR loop and should not indent\"\n");
}

// ---------------------------------------------------------------------------
// Access modifiers (Private/Public/Protected) are transparent prefixes —
// the keyword that follows decides the block.
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, PrivateSubOpensBlock) {
    EXPECT_EQ(format(
                  "Private Sub Foo()\n"
                  "Print \"hi\"\n"
                  "End Sub\n"
              ),
        "Private Sub Foo()\n"
        "    Print \"hi\"\n"
        "End Sub\n");
}

TEST_F(ReFormatterTests, PublicTypeOpensBlock) {
    EXPECT_EQ(format(
                  "Public Type Foo\n"
                  "x As Integer\n"
                  "End Type\n"
              ),
        "Public Type Foo\n"
        "    x As Integer\n"
        "End Type\n");
}

TEST_F(ReFormatterTests, PublicTypeAliasDoesNotOpenBlock) {
    // `Public Type X As Integer` is an alias — modifier is transparent but
    // the Type-As form must still resolve to a statement.
    EXPECT_EQ(format(
                  "Public Type X As Integer\n"
                  "Print x\n"
              ),
        "Public Type X As Integer\n"
        "Print x\n");
}

TEST_F(ReFormatterTests, PublicLabelInsideTypeBody) {
    // `public:` is a C++-style visibility label inside a Type body — does
    // not open a block, body content stays at Type's indent level.
    EXPECT_EQ(format(
                  "Type myudt\n"
                  "public:\n"
                  "Declare Sub some_public()\n"
                  "End Type\n"
              ),
        "Type myudt\n"
        "    public:\n"
        "    Declare Sub some_public()\n"
        "End Type\n");
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
        "End Sub\n");
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
        "End Sub\n");
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
        "End If\n");
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
        "Print \"done\"\n");
}

TEST_F(ReFormatterTests, EndIfSingleWordClosesIfBlock) {
    // `EndIf` (one word) is the combined form of `End If` — closes the
    // if-block the same way.
    EXPECT_EQ(format(
                  "If x > 0 Then\n"
                  "Print x\n"
                  "EndIf\n"
              ),
        "If x > 0 Then\n"
        "    Print x\n"
        "EndIf\n");
}

TEST_F(ReFormatterTests, EndIfClosesNestedIfInsideSub) {
    EXPECT_EQ(format(
                  "Sub Main\n"
                  "If x Then\n"
                  "Print x\n"
                  "endif\n"
                  "End Sub\n"
              ),
        "Sub Main\n"
        "    If x Then\n"
        "        Print x\n"
        "    endif\n"
        "End Sub\n");
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
        "End If\n");
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
        "Next\n");
}

TEST_F(ReFormatterTests, DoLoop) {
    EXPECT_EQ(format(
                  "Do\n"
                  "x += 1\n"
                  "Loop\n"
              ),
        "Do\n"
        "    x += 1\n"
        "Loop\n");
}

TEST_F(ReFormatterTests, WhileWend) {
    EXPECT_EQ(format(
                  "While x > 0\n"
                  "x -= 1\n"
                  "Wend\n"
              ),
        "While x > 0\n"
        "    x -= 1\n"
        "Wend\n");
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
        "End Select\n");
}

TEST_F(ReFormatterTests, ScopeBlock) {
    EXPECT_EQ(format(
                  "Scope\n"
                  "Dim x = 1\n"
                  "End Scope\n"
              ),
        "Scope\n"
        "    Dim x = 1\n"
        "End Scope\n");
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
        "End Type\n");
}

TEST_F(ReFormatterTests, TypeAsAlias) {
    // `Type As <T> <name>` is an alias declaration, not a block.
    EXPECT_EQ(format(
                  "Type As Integer MyInt\n"
                  "Dim x As MyInt\n"
              ),
        "Type As Integer MyInt\n"
        "Dim x As MyInt\n");
}

// ---------------------------------------------------------------------------
// Colon splitting (reFormat=true default)
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, ColonSplitsStatements) {
    EXPECT_EQ(format("x = 1 : y = 2\n"),
        "x = 1\n"
        "y = 2\n");
}

TEST_F(ReFormatterTests, ColonSplitsIntoBlock) {
    EXPECT_EQ(format("If x Then : Print x : End If\n"),
        "If x Then\n"
        "    Print x\n"
        "End If\n");
}

TEST_F(ReFormatterTests, ColonSplitNestedBlock) {
    EXPECT_EQ(format(
                  "Sub Main : If x Then : Print x : End If : End Sub\n"
              ),
        "Sub Main\n"
        "    If x Then\n"
        "        Print x\n"
        "    End If\n"
        "End Sub\n");
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
        "Print x\n");
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
        "End Sub\n");
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
        "#endif\n");
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
        "#endif\n");
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
        "End Sub\n");
}

TEST_F(ReFormatterTests, PPMacroBlock) {
    EXPECT_EQ(format(
                  "#macro MyMacro(x)\n"
                  "Print x\n"
                  "#endmacro\n"
              ),
        "#macro MyMacro(x)\n"
        "    Print x\n"
        "#endmacro\n");
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
        "#endif\n");
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
        "End Sub\n");
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
        "#endif\n");
}

TEST_F(ReFormatterTests, PPEmptyBlock) {
    EXPECT_EQ(format(
                  "#ifdef DEBUG\n"
                  "#endif\n"
              ),
        "#ifdef DEBUG\n"
        "#endif\n");
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
        "Next\n");
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
        "end if\n");
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
        "y = 2\n");
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
        "y = 2\n");
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
        "End Sub\n");
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
        "End Sub\n");
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
        "End Sub\n");
}

// ---------------------------------------------------------------------------
// Spacing — transformative inputs exercising needsSpaceBefore rules
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, SpacingNormalizesBinaryOperators) {
    // Arithmetic / comparison / compound-assign all get single-space padding.
    // FB shift uses keyword form (Shl/Shr), not C-style << / >>.
    EXPECT_EQ(format(
                  "x=a+b*c-d\n"
                  "y=x>=0\n"
                  "z=y Shl 2\n"
                  "x+=1\n"
              ),
        "x = a + b * c - d\n"
        "y = x >= 0\n"
        "z = y Shl 2\n"
        "x += 1\n");
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
        "p = @x\n");
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
        "z = (a + b) * c\n");
}

TEST_F(ReFormatterTests, SpacingMemberAccessAndBraces) {
    EXPECT_EQ(format(
                  "x = foo.bar\n"
                  "y = ptr->field\n"
                  "z = { 1, 2, 3 }\n"
              ),
        "x = foo.bar\n"
        "y = ptr->field\n"
        "z = { 1, 2, 3 }\n");
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
        "#endif\n");
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
        "End Sub\n");
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
        "End Sub\n");
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
        "End Type\n");
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
        "End Sub\n");
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
        "End Sub\n");
}

TEST_F(ReFormatterTests, PreserveIndent_InterTokenStillNormalized) {
    // reIndent=false but reFormat=true — indent kept, inter-token spacing still normalized.
    EXPECT_EQ(formatWith(
                  "    x=1+2\n",
                  { .tabSize = tabSize, .reIndent = false }
              ),
        "    x = 1 + 2\n");
}

// ---------------------------------------------------------------------------
// reFormat = false — preserve inter-token whitespace and original layout
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, PreserveFormat_QuirkySpacing) {
    EXPECT_EQ(formatWith(
                  "x=  1  +  2\n",
                  { .tabSize = tabSize, .reFormat = false }
              ),
        "x=  1  +  2\n");
}

TEST_F(ReFormatterTests, ReFormat_ContinuationStaysMultiLine) {
    // reFormat=true rebuilds spacing, but `_` line continuations must still
    // produce multi-physical-line output (squashing them into one line would
    // produce invalid FB).
    EXPECT_EQ(formatWith(
                  "this _\n"
                  "    . _\n"
                  "    foo()\n",
                  { .tabSize = tabSize, .reIndent = true, .reFormat = true }
              ),
        "this _\n"
        "    . _\n"
        "    foo()\n");
}

TEST_F(ReFormatterTests, PreserveFormat_ContinuationPreserved) {
    EXPECT_EQ(formatWith(
                  "Dim x = 1 _\n"
                  "        + 2\n",
                  { .tabSize = tabSize, .reFormat = false }
              ),
        "Dim x = 1 _\n"
        "        + 2\n");
}

TEST_F(ReFormatterTests, PreserveFormat_ColonSeparated) {
    // Colon stays inline (default behavior would split onto multiple lines).
    EXPECT_EQ(formatWith(
                  "x = 1 : y = 2\n",
                  { .tabSize = tabSize, .reFormat = false }
              ),
        "x = 1 : y = 2\n");
}

TEST_F(ReFormatterTests, PreserveFormat_ForNextOnOneLine) {
    // Self-contained `For ... Next` on one line must not open a phantom block.
    EXPECT_EQ(formatWith(
                  "For i = 1 To 10 : Print i : Next\n",
                  { .tabSize = tabSize, .reFormat = false }
              ),
        "For i = 1 To 10 : Print i : Next\n");
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
        "y = 2\n");
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
        "End Sub\n");
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
        "End Sub\n");
}

TEST_F(ReFormatterTests, PassthroughWithBothFlagsOff) {
    const char* source = "Sub Main\n"
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
    const char* source = "Dim total = 1 _\n"
                         "          + 2 _\n"
                         "          + 3\n"
                         "x = 1 : y = 2 : z = 3\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}

TEST_F(ReFormatterTests, RoundTrip_PPDirectives) {
    const char* source = "Sub Main\n"
                         "  #ifdef DEBUG\n"
                         "    Print \"dbg\"\n"
                         "  #else\n"
                         "    Print \"rel\"\n"
                         "  #endif\n"
                         "End Sub\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}

TEST_F(ReFormatterTests, RoundTrip_BlankLineRuns) {
    const char* source = "Sub A\n"
                         "End Sub\n"
                         "\n"
                         "\n"
                         "\n"
                         "Sub B\n"
                         "End Sub\n";
    EXPECT_EQ(formatWith(source, { .tabSize = tabSize, .reIndent = false, .reFormat = false }), source);
}

// ---------------------------------------------------------------------------
// Format pragma regions (' format off / ' format on)
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, FormatOff_PreservesContentInsideRegion) {
    // Everything between `' format off` and `' format on` survives verbatim —
    // odd spacing, lack of indent, original case, all of it.
    EXPECT_EQ(format(
                  "Sub Main\n"
                  "' format off\n"
                  "x=  1+   2\n"
                  "y=x*3\n"
                  "' format on\n"
                  "z = x + y\n"
                  "End Sub\n"
              ),
        "Sub Main\n"
        "' format off\n"
        "x=  1+   2\n"
        "y=x*3\n"
        "' format on\n"
        "    z = x + y\n"
        "End Sub\n");
}

TEST_F(ReFormatterTests, FormatOff_IndentResumesAfterRegion) {
    // After the region ends, code returns to the correct indent for its block.
    EXPECT_EQ(format(
                  "If x Then\n"
                  "' format off\n"
                  "nope\n"
                  "' format on\n"
                  "yes = 1\n"
                  "End If\n"
              ),
        "If x Then\n"
        "' format off\n"
        "nope\n"
        "' format on\n"
        "    yes = 1\n"
        "End If\n");
}

TEST_F(ReFormatterTests, FormatOff_PreservesBlankLineRuns) {
    EXPECT_EQ(format(
                  "' format off\n"
                  "a\n"
                  "\n"
                  "\n"
                  "b\n"
                  "' format on\n"
              ),
        "' format off\n"
        "a\n"
        "\n"
        "\n"
        "b\n"
        "' format on\n");
}

TEST_F(ReFormatterTests, FormatOff_NestedPragmasRoundTrip) {
    // Inner off/on just bump the counter; the whole region is preserved.
    EXPECT_EQ(format(
                  "' format off\n"
                  "outer\n"
                  "' format off\n"
                  "inner\n"
                  "' format on\n"
                  "tail\n"
                  "' format on\n"
              ),
        "' format off\n"
        "outer\n"
        "' format off\n"
        "inner\n"
        "' format on\n"
        "tail\n"
        "' format on\n");
}

TEST_F(ReFormatterTests, FormatOff_UnbalancedOffRunsToEof) {
    EXPECT_EQ(format(
                  "Dim a = 1\n"
                  "' format off\n"
                  "messy  code  here\n"
                  "more = weird\n"
              ),
        "Dim a = 1\n"
        "' format off\n"
        "messy  code  here\n"
        "more = weird\n");
}

TEST_F(ReFormatterTests, FormatOff_OutsideRegionStillNormalises) {
    // Spacing and indentation outside the region are still normalised.
    EXPECT_EQ(format(
                  "Sub   Main\n"
                  "x=1+2\n"
                  "' format off\n"
                  "y   =   3\n"
                  "' format on\n"
                  "z=4\n"
                  "End Sub\n"
              ),
        "Sub Main\n"
        "    x = 1 + 2\n"
        "' format off\n"
        "y   =   3\n"
        "' format on\n"
        "    z = 4\n"
        "End Sub\n");
}

TEST_F(ReFormatterTests, FormatOff_RoundTripStable) {
    // Idempotence: format(format(src)) == format(src).
    const char* source = "Sub Main\n"
                         "' format off\n"
                         "ugly   = 1\n"
                         "' format on\n"
                         "clean = 2\n"
                         "End Sub\n";
    const auto once = format(source);
    const auto twice = format(once.c_str());
    EXPECT_EQ(once, twice);
}

// ---------------------------------------------------------------------------
// CaseTransform interaction with verbatim regions
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, FormatOff_CaseTransformSkipsVerbatimKeywords) {
    // Tokens inside a verbatim region keep their original casing even under
    // Upper case conversion.
    auto tokens = tests::tokenise(*m_lexer,
        "dim x = 1\n"
        "' format off\n"
        "dim y = 2\n"
        "' format on\n"
        "dim z = 3\n");

    std::array<CaseMode, kThemeKeywordGroupsCount> cases {};
    cases.fill(CaseMode::Upper);
    CaseTransform upper { cases };
    const auto transformed = upper.apply(tokens);

    // Find the three `dim` identifier/keyword tokens. First and third should
    // be upper-cased; the middle one (inside the region) stays lowercase.
    std::vector<std::string> dimTexts;
    for (const auto& tok : transformed) {
        if (tok.kind == lexer::TokenKind::Keywords
            && (tok.text == "dim" || tok.text == "DIM")) {
            dimTexts.push_back(tok.text);
        }
    }
    ASSERT_EQ(dimTexts.size(), 3);
    EXPECT_EQ(dimTexts[0], "DIM");
    EXPECT_EQ(dimTexts[1], "dim"); // inside region — untouched
    EXPECT_EQ(dimTexts[2], "DIM");
}

// ---------------------------------------------------------------------------
// Lean mode — strips Whitespace/Comment/CommentBlock tokens and collapses
// runs of Newlines. Used by non-rendering consumers (sub/function browser).
// ---------------------------------------------------------------------------

TEST_F(ReFormatterTests, LeanFilterDropsLayoutAndCollapsesBlankLines) {
    const auto tokens = tests::tokenise(*m_lexer,
        "  Sub Foo  ' inline comment\n"
        "\n"
        "\n"
        "    /' multi\n"
        "       line '/\n"
        "    Print 1\n"
        "End Sub\n");
    ReFormatter formatter({ .tabSize = tabSize, .lean = true });
    const auto tree = formatter.buildTree(tokens);

    // Walk every token in the tree and verify no layout/comment tokens remain.
    const auto countByKind = [&](lexer::TokenKind kind) {
        int count = 0;
        const auto countInTokens = [&](const std::vector<lexer::Token>& tokens) {
            for (const auto& tkn : tokens) {
                if (tkn.kind == kind) {
                    count++;
                }
            }
        };
        const std::function<void(const Node&)> walk = [&](const Node& node) {
            if (const auto* st = std::get_if<StatementNode>(&node)) {
                countInTokens(st->tokens);
            } else if (const auto* block = std::get_if<std::unique_ptr<BlockNode>>(&node)) {
                if ((*block)->opener) {
                    countInTokens((*block)->opener->tokens);
                }
                for (const auto& child : (*block)->body) {
                    walk(child);
                }
                if ((*block)->closer) {
                    countInTokens((*block)->closer->tokens);
                }
            }
        };
        for (const auto& node : tree.nodes) {
            walk(node);
        }
        return count;
    };

    EXPECT_EQ(countByKind(lexer::TokenKind::Whitespace), 0);
    EXPECT_EQ(countByKind(lexer::TokenKind::Comment), 0);
    EXPECT_EQ(countByKind(lexer::TokenKind::CommentBlock), 0);
}

TEST_F(ReFormatterTests, LeanModeProducesSubBlock) {
    const auto tokens = tests::tokenise(*m_lexer,
        "Sub Foo\n"
        "Print 1\n"
        "End Sub\n");
    ReFormatter formatter({ .tabSize = tabSize, .lean = true });
    const auto tree = formatter.buildTree(tokens);

    ASSERT_EQ(tree.nodes.size(), 1u);
    const auto* block = std::get_if<std::unique_ptr<BlockNode>>(&tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    ASSERT_TRUE((*block)->opener.has_value());
    ASSERT_TRUE((*block)->closer.has_value());

    // First significant token in opener is the Sub keyword.
    const auto& openerTokens = (*block)->opener->tokens;
    ASSERT_FALSE(openerTokens.empty());
    EXPECT_EQ(openerTokens[0].keywordKind, lexer::KeywordKind::Sub);
}

TEST_F(ReFormatterTests, LeanModePreservesVerbatimRegion) {
    const auto tokens = tests::tokenise(*m_lexer,
        "' format off\n"
        "  X   =   1\n"
        "' format on\n"
        "Sub Foo\n"
        "End Sub\n");
    ReFormatter formatter({ .tabSize = tabSize, .lean = true });
    const auto tree = formatter.buildTree(tokens);

    bool sawVerbatim = false;
    for (const auto& node : tree.nodes) {
        if (std::holds_alternative<VerbatimNode>(node)) {
            sawVerbatim = true;
            break;
        }
    }
    EXPECT_TRUE(sawVerbatim);
}
