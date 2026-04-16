//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "lib/analyses/lexer/Lexer.hpp"
#include "lib/config/Keywords.hpp"
#include "lib/format/formatters/Formatter.hpp"
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

    /// Tokenise, format, and return the resulting text.
    auto reindent(const char* source, const bool anchoredPP = false) -> std::string {
        const auto tokens = m_lexer->tokenise(source);
        format::Formatter formatter({ .tabSize = static_cast<std::size_t>(tabSize), .anchoredPP = anchoredPP });
        return formatter.format(tokens);
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

TEST_F(ReindentTests, DeclareSubNoIndent) {
    const auto result = reindent(
        "Declare Sub Main()\n"
        "Dim x = 1\n"
    );
    EXPECT_EQ(result,
        "Declare Sub Main()\n"
        "Dim x = 1\n"
    );
}

TEST_F(ReindentTests, DeclareFunctionNoIndent) {
    const auto result = reindent(
        "Declare Function Add(a As Integer, b As Integer) As Integer\n"
        "Dim x = 1\n"
    );
    EXPECT_EQ(result,
        "Declare Function Add(a As Integer, b As Integer) As Integer\n"
        "Dim x = 1\n"
    );
}

TEST_F(ReindentTests, FunctionReturnValueNoIndent) {
    // "Function = value" inside a function body — must not indent
    const auto result = reindent(
        "Function Foo() As Integer\n"
        "Function = 10\n"
        "End Function\n"
    );
    EXPECT_EQ(result,
        "Function Foo() As Integer\n"
        "    Function = 10\n"
        "End Function\n"
    );
}

TEST_F(ReindentTests, ConstructorBlock) {
    const auto result = reindent(
        "Constructor MyType()\n"
        "x = 1\n"
        "End Constructor\n"
    );
    EXPECT_EQ(result,
        "Constructor MyType()\n"
        "    x = 1\n"
        "End Constructor\n"
    );
}

TEST_F(ReindentTests, DestructorBlock) {
    const auto result = reindent(
        "Destructor MyType()\n"
        "x = 0\n"
        "End Destructor\n"
    );
    EXPECT_EQ(result,
        "Destructor MyType()\n"
        "    x = 0\n"
        "End Destructor\n"
    );
}

TEST_F(ReindentTests, OperatorBlock) {
    const auto result = reindent(
        "Operator MyType.Cast() As String\n"
        "Return \"hello\"\n"
        "End Operator\n"
    );
    EXPECT_EQ(result,
        "Operator MyType.Cast() As String\n"
        "    Return \"hello\"\n"
        "End Operator\n"
    );
}

TEST_F(ReindentTests, DeclareConstructorNoIndent) {
    const auto result = reindent(
        "Declare Constructor()\n"
        "Dim x = 1\n"
    );
    EXPECT_EQ(result,
        "Declare Constructor()\n"
        "Dim x = 1\n"
    );
}

TEST_F(ReindentTests, DeclareDestructorNoIndent) {
    const auto result = reindent(
        "Declare Destructor()\n"
        "Dim x = 1\n"
    );
    EXPECT_EQ(result,
        "Declare Destructor()\n"
        "Dim x = 1\n"
    );
}

TEST_F(ReindentTests, DeclareOperatorNoIndent) {
    const auto result = reindent(
        "Declare Operator Cast() As String\n"
        "Dim x = 1\n"
    );
    EXPECT_EQ(result,
        "Declare Operator Cast() As String\n"
        "Dim x = 1\n"
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
    // Colons are split into separate lines with proper indentation
    const auto result = reindent(
        "If x Then : Print x : End If\n"
        "Print \"done\"\n"
    );
    EXPECT_EQ(result,
        "If x Then\n"
        "    Print x\n"
        "End If\n"
        "Print \"done\"\n"
    );
}

TEST_F(ReindentTests, ColonSeparatedForNext) {
    // Colons are split into separate lines with proper indentation
    const auto result = reindent(
        "For i = 1 To 10 : Print i : Next\n"
        "Print \"done\"\n"
    );
    EXPECT_EQ(result,
        "For i = 1 To 10\n"
        "    Print i\n"
        "Next\n"
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
// Preprocessor — non-block directives indent with surrounding code
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, PreprocessorIncludeIndented) {
    const auto result = reindent(
        "Sub Main\n"
        "#include \"file.bi\"\n"
        "Print x\n"
        "End Sub\n"
    );
    EXPECT_EQ(result,
        "Sub Main\n"
        "    #include \"file.bi\"\n"
        "    Print x\n"
        "End Sub\n"
    );
}

// ---------------------------------------------------------------------------
// Preprocessor — #ifdef / #endif blocks
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, PreprocessorIfdefEndif) {
    const auto result = reindent(
        "#ifdef DEBUG\n"
        "#define LOG(x) Print x\n"
        "#endif\n"
    );
    EXPECT_EQ(result,
        "#ifdef DEBUG\n"
        "    #define LOG(x) Print x\n"
        "#endif\n"
    );
}

TEST_F(ReindentTests, PreprocessorIfdefElseEndif) {
    const auto result = reindent(
        "#ifdef DEBUG\n"
        "#define LOG(x) Print x\n"
        "#else\n"
        "#define LOG(x)\n"
        "#endif\n"
    );
    EXPECT_EQ(result,
        "#ifdef DEBUG\n"
        "    #define LOG(x) Print x\n"
        "#else\n"
        "    #define LOG(x)\n"
        "#endif\n"
    );
}

TEST_F(ReindentTests, PreprocessorNested) {
    const auto result = reindent(
        "#ifdef A\n"
        "#ifdef B\n"
        "#define X 1\n"
        "#endif\n"
        "#endif\n"
    );
    EXPECT_EQ(result,
        "#ifdef A\n"
        "    #ifdef B\n"
        "        #define X 1\n"
        "    #endif\n"
        "#endif\n"
    );
}

TEST_F(ReindentTests, PreprocessorIfEndif) {
    const auto result = reindent(
        "#if FOO = 1\n"
        "#define BAR 2\n"
        "#endif\n"
    );
    EXPECT_EQ(result,
        "#if FOO = 1\n"
        "    #define BAR 2\n"
        "#endif\n"
    );
}

TEST_F(ReindentTests, PreprocessorIfndefEndif) {
    const auto result = reindent(
        "#ifndef GUARD\n"
        "#define GUARD\n"
        "#endif\n"
    );
    EXPECT_EQ(result,
        "#ifndef GUARD\n"
        "    #define GUARD\n"
        "#endif\n"
    );
}

// ---------------------------------------------------------------------------
// Preprocessor — #macro / #endmacro
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, PreprocessorMacroBlock) {
    const auto result = reindent(
        "#macro MyMacro(x)\n"
        "Print x\n"
        "#endmacro\n"
    );
    EXPECT_EQ(result,
        "#macro MyMacro(x)\n"
        "    Print x\n"
        "#endmacro\n"
    );
}

// ---------------------------------------------------------------------------
// Preprocessor — mixed with code blocks
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, PreprocessorInsideCodeBlock) {
    const auto result = reindent(
        "Sub Main\n"
        "#ifdef DEBUG\n"
        "Print \"debug\"\n"
        "#endif\n"
        "End Sub\n"
    );
    EXPECT_EQ(result,
        "Sub Main\n"
        "    #ifdef DEBUG\n"
        "        Print \"debug\"\n"
        "    #endif\n"
        "End Sub\n"
    );
}

TEST_F(ReindentTests, PreprocessorElseIfVariants) {
    const auto result = reindent(
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
    );
    EXPECT_EQ(result,
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

// ---------------------------------------------------------------------------
// Preprocessor / code indent independence
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, PpCodeIndentResetAtElse) {
    const auto result = reindent(
        "#ifdef DEBUG\n"
        "if x then\n"
        "print x\n"
        "end if\n"
        "#else\n"
        "if y then\n"
        "print y\n"
        "end if\n"
        "#endif\n"
    );
    EXPECT_EQ(result,
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

TEST_F(ReindentTests, PpInsideCodeBlock) {
    const auto result = reindent(
        "Sub Main\n"
        "#ifdef DEBUG\n"
        "print \"debug\"\n"
        "#endif\n"
        "print \"hello\"\n"
        "End Sub\n"
    );
    EXPECT_EQ(result,
        "Sub Main\n"
        "    #ifdef DEBUG\n"
        "        print \"debug\"\n"
        "    #endif\n"
        "    print \"hello\"\n"
        "End Sub\n"
    );
}

TEST_F(ReindentTests, PpNestedWithCode) {
    const auto result = reindent(
        "#ifdef A\n"
        "#ifdef B\n"
        "print x\n"
        "#endif\n"
        "print y\n"
        "#endif\n"
    );
    EXPECT_EQ(result,
        "#ifdef A\n"
        "    #ifdef B\n"
        "        print x\n"
        "    #endif\n"
        "    print y\n"
        "#endif\n"
    );
}

TEST_F(ReindentTests, PpElseIfChainResetsCode) {
    const auto result = reindent(
        "#ifdef A\n"
        "if x then\n"
        "print 1\n"
        "end if\n"
        "#elseif B\n"
        "if y then\n"
        "print 2\n"
        "end if\n"
        "#else\n"
        "print 3\n"
        "#endif\n"
    );
    EXPECT_EQ(result,
        "#ifdef A\n"
        "    if x then\n"
        "        print 1\n"
        "    end if\n"
        "#elseif B\n"
        "    if y then\n"
        "        print 2\n"
        "    end if\n"
        "#else\n"
        "    print 3\n"
        "#endif\n"
    );
}

// ---------------------------------------------------------------------------
// Anchored # mode
// ---------------------------------------------------------------------------

TEST_F(ReindentTests, AnchoredHashSimple) {
    const auto result = reindent(
        "#ifdef DEBUG\n"
        "#define X 1\n"
        "#endif\n",
        true
    );
    EXPECT_EQ(result,
        "#ifdef DEBUG\n"
        "#   define X 1\n"
        "#endif\n"
    );
}

TEST_F(ReindentTests, AnchoredHashNested) {
    const auto result = reindent(
        "#if 1\n"
        "#if 1\n"
        "print \"hello\"\n"
        "#endif\n"
        "#endif\n",
        true
    );
    EXPECT_EQ(result,
        "#if 1\n"
        "#   if 1\n"
        "        print \"hello\"\n"
        "#   endif\n"
        "#endif\n"
    );
}

TEST_F(ReindentTests, AnchoredHashInsideCodeBlock) {
    const auto result = reindent(
        "Sub Main\n"
        "#ifdef DEBUG\n"
        "print x\n"
        "#endif\n"
        "End Sub\n",
        true
    );
    EXPECT_EQ(result,
        "Sub Main\n"
        "#   ifdef DEBUG\n"
        "        print x\n"
        "#   endif\n"
        "End Sub\n"
    );
}
