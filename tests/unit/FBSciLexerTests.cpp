//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "Scintilla.h"
#include "analyses/lexer/MemoryDocument.hpp"
#include "editor/lexilla/FBSciLexer.hpp"

using namespace fbide;
using S = ThemeCategory;

class FBSciLexerTests : public testing::Test {
protected:
    void SetUp() override {
        m_lexer = FBSciLexer::Create();
        // Set up keyword lists matching fbfull.lng groups
        // Indices map 1:1 to DEFINE_THEME_KEYWORD_GROUPS order.
        m_lexer->WordListSet(0, "dim as if then else end sub function type asm");                   // Keyword1
        m_lexer->WordListSet(1, "integer string single double long byte");                          // Keyword2
        m_lexer->WordListSet(2, "and or not mod xor");                                              // Keyword3
        m_lexer->WordListSet(3, "__fb_version__");                                                  // Keyword4
        m_lexer->WordListSet(4, "");                                                                // KeywordCustom
        m_lexer->WordListSet(5, "");                                                                // KeywordCustom
        m_lexer->WordListSet(6, "if ifdef ifndef else elseif endif macro endmacro define include"); // KeywordPP
        m_lexer->WordListSet(7, "mov push pop ret jmp");                                            // KeywordAsm1
        m_lexer->WordListSet(8, "eax ebx ecx edx");                                                 // KeywordAsm2
        m_lexer->PropertySet("fold", "1");
    }

    void TearDown() override {
        m_lexer->Release();
        m_lexer = nullptr;
    }

    /// Lex the source and return the style for each character
    auto lex(const std::string& source) -> std::vector<S> {
        m_doc.Set(source);
        m_lexer->Lex(0, m_doc.Length(), +S::Default, &m_doc);
        std::vector<S> styles;
        styles.reserve(source.size());
        for (Sci_Position i = 0; i < m_doc.Length(); i++) {
            styles.push_back(static_cast<S>(m_doc.StyleAt(i)));
        }
        return styles;
    }

    /// Build expected styles from a shorthand string.
    /// Each char maps to a state, repeated for the length of the span.
    /// Legend:
    ///   ' ' = Default    'C' = Comment          'M' = MultilineComment
    ///   'N' = Number     'S' = String            'O' = StringOpen
    ///   'I' = Identifier '1'..'4' = Keyword1..4  '5' = KeywordCustom
    ///   '6' = KeywordCustom '7' = KeywordAsm1  '8' = KeywordAsm2
    ///   'P' = Operator   'L' = Label             'V' = Constant
    ///   '#' = Preprocessor  'E' = Error  'k' = KeywordPP
    ///   'n' = NumberPP   's' = StringPP   'p' = OperatorPP   'i' = IdentifierPP
    static auto expect(const std::string& pattern) -> std::vector<S> {
        static constexpr auto map = [] consteval {
            std::array<S, 128> table {};
            table[' '] = S::Default;
            table['C'] = S::Comment;
            table['M'] = S::MultilineComment;
            table['N'] = S::Number;
            table['S'] = S::String;
            table['O'] = S::StringOpen;
            table['I'] = S::Identifier;
            table['1'] = S::Keywords;
            table['2'] = S::KeywordTypes;
            table['3'] = S::KeywordOperators;
            table['4'] = S::KeywordConstants;
            table['5'] = S::KeywordLibrary;
            table['6'] = S::KeywordCustom;
            table['7'] = S::KeywordAsm1;
            table['8'] = S::KeywordAsm2;
            table['P'] = S::Operator;
            table['L'] = S::Label;
            table['#'] = S::Preprocessor;
            table['k'] = S::KeywordPP;
            table['n'] = S::NumberPP;
            table['s'] = S::StringPP;
            table['p'] = S::OperatorPP;
            table['i'] = S::IdentifierPP;
            table['E'] = S::Error;
            return table;
        }();

        std::vector<S> result;
        result.reserve(pattern.size());
        for (const char c : pattern) {
            result.push_back(map[static_cast<unsigned char>(c)]);
        }
        return result;
    }

    /// Format styles as a readable string for diagnostics
    static auto format(const std::vector<S>& styles) -> std::string {
        static constexpr auto map = [] consteval {
            std::array<char, 32> table {};
            table[+S::Default] = ' ';
            table[+S::Comment] = 'C';
            table[+S::MultilineComment] = 'M';
            table[+S::Number] = 'N';
            table[+S::String] = 'S';
            table[+S::StringOpen] = 'O';
            table[+S::Identifier] = 'I';
            table[+S::Keywords] = '1';
            table[+S::KeywordTypes] = '2';
            table[+S::KeywordOperators] = '3';
            table[+S::KeywordConstants] = '4';
            table[+S::KeywordLibrary] = '5';
            table[+S::KeywordCustom] = '6';
            table[+S::KeywordAsm1] = '7';
            table[+S::KeywordAsm2] = '8';
            table[+S::Operator] = 'P';
            table[+S::Label] = 'L';
            table[+S::Preprocessor] = '#';
            table[+S::KeywordPP] = 'k';
            table[+S::NumberPP] = 'n';
            table[+S::StringPP] = 's';
            table[+S::OperatorPP] = 'p';
            table[+S::IdentifierPP] = 'i';
            table[+S::Error] = 'E';
            return table;
        }();

        std::string result;
        result.reserve(styles.size());
        for (const auto s : styles) {
            result += map[+s];
        }
        return result;
    }

    /// Assert that the source lexes to the expected style pattern
    void expectStyles(const std::string& source, const std::string& pattern) {
        ASSERT_EQ(source.size(), pattern.size())
            << "Source and pattern must have the same length.\n"
            << "Source:  \"" << source << "\" (" << source.size() << ")\n"
            << "Pattern: \"" << pattern << "\" (" << pattern.size() << ")";

        auto actual = lex(source);
        auto expected = expect(pattern);
        EXPECT_EQ(format(actual), pattern)
            << "Source: \"" << source << "\"";
    }

    /// Lex and fold the source, returning a per-line fold summary joined by '|'.
    /// Each line is rendered as "<level>[H][W]" where level is indent columns
    /// relative to SC_FOLDLEVELBASE, H = SC_FOLDLEVELHEADERFLAG, W = SC_FOLDLEVELWHITEFLAG.
    auto fold(const std::string& source) -> std::string {
        m_doc.Set(source);
        m_lexer->Lex(0, m_doc.Length(), +S::Default, &m_doc);
        m_lexer->Fold(0, m_doc.Length(), +S::Default, &m_doc);

        std::string result;
        for (Sci_Position line = 0; line < m_doc.MaxLine(); line++) {
            const int level = m_doc.GetLevel(line);
            if (!result.empty()) {
                result += '|';
            }
            result += std::to_string((level & SC_FOLDLEVELNUMBERMASK) - SC_FOLDLEVELBASE);
            if (level & SC_FOLDLEVELHEADERFLAG) {
                result += 'H';
            }
            if (level & SC_FOLDLEVELWHITEFLAG) {
                result += 'W';
            }
        }
        return result;
    }

    Scintilla::ILexer5* m_lexer = nullptr;
    MemoryDocument m_doc;
};

// region ---------- Comments ----------

TEST_F(FBSciLexerTests, SingleLineComment) {
    expectStyles(
        "' this is a comment\n",
        "CCCCCCCCCCCCCCCCCCC "
    );
}

TEST_F(FBSciLexerTests, CommentAfterCode) {
    expectStyles(
        "dim x ' comment\n",
        "111 I CCCCCCCCC "
    );
}

TEST_F(FBSciLexerTests, MultilineComment) {
    expectStyles(
        "/' hello '/",
        "MMMMMMMMMMM"
    );
}

TEST_F(FBSciLexerTests, NestedMultilineComment) {
    expectStyles(
        "/' outer /' inner '/ still '/",
        "MMMMMMMMMMMMMMMMMMMMMMMMMMMMM"
    );
}

TEST_F(FBSciLexerTests, NestedMultilineCommentWithLineBreaks) {
    expectStyles(
        "/' outer\n /'\n inner '/\nstill '/",
        "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"
    );
}

TEST_F(FBSciLexerTests, RemAtLineEndDoesNotContinueToNextLine) {
    // `rem` at the very end of a line is a comment for that line only — the
    // next line must lex normally. Regression for #28.
    expectStyles(
        "foobar rem\nfoobar",
        "IIIIII CCC IIIIII"
    );
}

TEST_F(FBSciLexerTests, PreprocessorRemAtLineEndDoesNotContinueToNextLine) {
    // Same rule applies inside a preprocessor line: `REM` at the end of the
    // `#define` line is a trailing comment, not a state that bleeds into the
    // following identifier. Regression for #28.
    expectStyles(
        "#define foo 1 REM\nfoobar",
        "#kkkkkk#iii#n#CCC IIIIII"
    );
}

// endregion

// region ---------- Numbers ----------

TEST_F(FBSciLexerTests, DecimalNumber) {
    expectStyles("123", "NNN");
}

TEST_F(FBSciLexerTests, FloatingPoint) {
    expectStyles("1.5", "NNN");
}

TEST_F(FBSciLexerTests, DotFraction) {
    expectStyles(".5", "NN");
}

TEST_F(FBSciLexerTests, Exponent) {
    expectStyles("1.5E10", "NNNNNN");
}

TEST_F(FBSciLexerTests, ExponentWithSign) {
    expectStyles("1.5E-3", "NNNNNN");
}

TEST_F(FBSciLexerTests, HexNumber) {
    expectStyles("&hFF", "NNNN");
}

TEST_F(FBSciLexerTests, OctalNumber) {
    expectStyles("&o77", "NNNN");
}

TEST_F(FBSciLexerTests, BinaryNumber) {
    expectStyles("&b1010", "NNNNNN");
}

TEST_F(FBSciLexerTests, IntegerSuffix) {
    expectStyles("123ULL", "NNNNNN");
}

TEST_F(FBSciLexerTests, FPSuffix) {
    expectStyles("1.5!", "NNNN");
}

TEST_F(FBSciLexerTests, InvalidNumberTerminator) {
    expectStyles("123abc", "EEEEEE");
}

// endregion

// region ---------- Strings ----------

TEST_F(FBSciLexerTests, StringLiteral) {
    expectStyles(
        "\"hello\"",
        "SSSSSSS"
    );
}

TEST_F(FBSciLexerTests, EscapeString) {
    expectStyles(
        "!\"he\\\"llo\"",
        "SSSSSSSSSS"
    );
}

TEST_F(FBSciLexerTests, UnclosedString) {
    expectStyles(
        "\"hello\n",
        "OOOOOO "
    );
}

// endregion

// region ---------- Keywords ----------

TEST_F(FBSciLexerTests, Keyword1) {
    expectStyles("dim", "111");
}

TEST_F(FBSciLexerTests, Keyword2) {
    expectStyles("integer", "2222222");
}

TEST_F(FBSciLexerTests, Keyword3) {
    expectStyles("and", "333");
}

TEST_F(FBSciLexerTests, NonKeywordIdentifier) {
    expectStyles("foo", "III");
}

TEST_F(FBSciLexerTests, DimStatement) {
    expectStyles(
        "dim x as integer",
        "111 I 11 2222222"
    );
}

// endregion

// region ---------- Asm blocks ----------

// Newlines in source take the Default style, which format() renders as
// space — so expected patterns use a space at every line-break position.
TEST_F(FBSciLexerTests, AsmBlockEnterExit) {
    // Regular keyword lists (Keyword1-4, KeywordCustom-2) are suppressed
    // inside asm blocks; asm-only lists (KeywordAsm1='7', KeywordAsm2='8')
    // kick in instead. "end asm" closes the block and restores normal
    // keyword lookup.
    expectStyles(
        "asm\nmov eax\nend asm\ndim\n",
        "111 777 888 111 111 111 "
    );
}

TEST_F(FBSciLexerTests, AsmBlockSuppressesRegularKeywords) {
    // "dim" is a Keyword1 outside asm but must not highlight inside.
    expectStyles(
        "asm\ndim\nend asm\n",
        "111 III 111 111 "
    );
}

TEST_F(FBSciLexerTests, AsmBlockBareEndStaysInBlock) {
    // A lone "end" (no asm follower) must not exit the asm block.
    // Next "mov" still lexes as an asm mnemonic (KeywordAsm1).
    expectStyles(
        "asm\nend\nmov\nend asm\n",
        "111 III 777 111 111 "
    );
}

TEST_F(FBSciLexerTests, AsmBlockEndAsmExtraWhitespace) {
    // Tabs and multiple spaces between "end" and "asm" should still match.
    expectStyles(
        "asm\nend\t asm\n",
        "111 111  111 "
    );
}

TEST_F(FBSciLexerTests, AsmBlockEndIdentifierDoesNotExit) {
    // "end" followed by an identifier starting with "asm..." must not
    // match — the look-ahead requires a word break after "asm".
    expectStyles(
        "asm\nend asmfoo\nend asm\n",
        "111 III IIIIII 111 111 "
    );
}

TEST_F(FBSciLexerTests, AsmRegistersInKeywordAsm2) {
    expectStyles(
        "asm\nmov eax\nend asm\n",
        "111 777 888 111 111 "
    );
}

// Single-line asm: `asm <stmt>` on one line. No `end asm` required.
// Following statements must lex with normal (non-asm) classification.
TEST_F(FBSciLexerTests, AsmSingleLineNoContinuation) {
    expectStyles(
        "asm mov eax\ndim x\n",
        "111 777 888 111 I "
    );
}

// Single-line asm spanning physical lines via `_` continuation.
// Both halves classify with asm wordlists; next logical line is normal.
TEST_F(FBSciLexerTests, AsmSingleLineWithContinuation) {
    expectStyles(
        "asm _\n   mov eax\ndim x\n",
        "111 C    777 888 111 I "
    );
}

// `asm` followed by a multi-line block comment then `_` — still a
// single-liner, content arrives on the continued physical line.
// Following logical line lexes with normal classification.
TEST_F(FBSciLexerTests, AsmSingleLineMLCommentThenContinuation) {
    expectStyles(
        "asm /' x '/ _\n   mov eax\ndim x\n",
        "111 MMMMMMM C    777 888 111 I "
    );
}

// `asm` followed by a block comment but NO `_` — logical line ends
// without significant content, so it opens a multi-line asm block.
// Body lexes with asm wordlists; `end asm` closes.
TEST_F(FBSciLexerTests, AsmBlockOpenerWithMLCommentNoContinuation) {
    expectStyles(
        "asm /' x '/\nmov eax\nend asm\n",
        "111 MMMMMMM 777 888 111 111 "
    );
}

// `asm` followed by a single-line `'` comment is treated as opening a
// block (no significant content on the logical line).
TEST_F(FBSciLexerTests, AsmBlockOpenerWithLineCommentOnly) {
    expectStyles(
        "asm ' note\nmov\nend asm\n",
        "111 CCCCCC 777 111 111 "
    );
}

// endregion

// region ---------- Operators ----------

TEST_F(FBSciLexerTests, Operators) {
    expectStyles("(+)", "PPP");
}

TEST_F(FBSciLexerTests, DotDotOperator) {
    expectStyles("..", "PP");
}

TEST_F(FBSciLexerTests, DotDotDotOperator) {
    expectStyles("...", "PPP");
}

TEST_F(FBSciLexerTests, DotDotDotDotOperator) {
    expectStyles("....", "EEEE");
}

// endregion

// region ---------- Labels ----------

TEST_F(FBSciLexerTests, Label) {
    expectStyles("myLabel: ", "LLLLLLLL ");
}

// endregion

// region ---------- Preprocessor ----------

TEST_F(FBSciLexerTests, Preprocessor) {
    expectStyles(
        "#define FOO\n",
        "#kkkkkk#iii "
    );
}

TEST_F(FBSciLexerTests, PreprocessorIncludeWithStringPath) {
    // #include "path" — quoted path body styles as StringPP, returning to
    // Preprocessor at end of line. Whitespace stays Preprocessor.
    expectStyles(
        "#include \"foo.bi\"\n",
        "#kkkkkkk#ssssssss "
    );
}

TEST_F(FBSciLexerTests, PreprocessorIncludeOnceWithStringPath) {
    // Only the directive (`include`) is KeywordPP. `once` is a body
    // identifier — IdentifierPP — even though it's part of the include
    // syntax. Quoted path is StringPP.
    expectStyles(
        "#include once \"foo.bi\"\n",
        "#kkkkkkk#iiii#ssssssss "
    );
}

TEST_F(FBSciLexerTests, PreprocessorDefineWithNumber) {
    // #define X 42 — `42` styles as NumberPP.
    expectStyles(
        "#define X 42\n",
        "#kkkkkk#i#nn "
    );
}

TEST_F(FBSciLexerTests, PreprocessorDefineWithOperatorAndNumber) {
    // Operator inside PP body styles as OperatorPP; subsequent number
    // returns to NumberPP, then back to Preprocessor at line end.
    expectStyles(
        "#define X = 42\n",
        "#kkkkkk#i#p#nn "
    );
}

TEST_F(FBSciLexerTests, PreprocessorIfWithExpression) {
    // #if A and B — only `if` is the directive (KeywordPP); body identifiers
    // (including `and`, even though it's a wordlist match outside PP body)
    // are IdentifierPP.
    expectStyles(
        "#if A and B\n",
        "#kk#i#iii#i "
    );
}

TEST_F(FBSciLexerTests, PreprocessorBodyKeywordIsIdentifierPP) {
    // `define` is a directive when first; here it's a body identifier after
    // the `#include` directive — must paint IdentifierPP, not KeywordPP.
    expectStyles(
        "#include define\n",
        "#kkkkkkk#iiiiii "
    );
}

TEST_F(FBSciLexerTests, PreprocessorDirectiveSurvivesContinuationAndComment) {
    // The first-identifier-is-directive rule survives `_` continuation and
    // multi-line block comments. `include` here is the FIRST identifier
    // after `#`, even though it's separated by continuation + comment.
    expectStyles(
        "# _\n/' c '/ _\ninclude \"x\"\n",
        "##C MMMMMMM#C kkkkkkk#sss "
    );
}

TEST_F(FBSciLexerTests, PreprocessorStringWithLiteral) {
    // Standard string rules apply inside PP — closing quote ends the literal.
    expectStyles(
        "#define X \"hi\"\n",
        "#kkkkkk#i#ssss "
    );
}

TEST_F(FBSciLexerTests, PreprocessorHexNumber) {
    expectStyles(
        "#define X &HFF\n",
        "#kkkkkk#i#nnnn "
    );
}

TEST_F(FBSciLexerTests, PreprocessorDoesNotBleedToNextLine) {
    // After a directive line ends, the next line must lex as regular code.
    // `dim` is in the Keywords wordlist — without proper PP-state reset
    // it would lex as IdentifierPP and the user's keyword highlighting
    // would silently disappear on every line after a `#if`.
    expectStyles(
        "#if\n    dim\n",
        "#kk     111 "
    );
}

TEST_F(FBSciLexerTests, PreprocessorCrlfDoesNotBleed) {
    // Regression: with CRLF line endings the `\r` styles as Preprocessor
    // (whitespace inside PP body) but `\n` MUST flip state back to Default
    // so the next line lexes as regular code. Without that flip, every line
    // after a `#` directive keeps lexing in PP mode and main-wordlist
    // keywords (Dim/Sub/Declare/...) silently mis-style as IdentifierPP.
    expectStyles(
        "#if x\r\n\tdim\r\n#endif\r\n",
        "#kk#i#  111  #kkkkk# "
    );
}

TEST_F(FBSciLexerTests, PreprocessorBlockBodyKeywordsHighlight) {
    // Code inside a `#if`/`#endif` block must lex normally — the directive
    // line is over and the body is regular code, not a continuation.
    // Reproduces user-reported regression: "#IF\n    declare\n#ENDIF\n"
    // shows `declare` as Identifier, not Keywords.
    expectStyles(
        "#if\n    dim\n#endif\n",
        "#kk     111 #kkkkk "
    );
}

// endregion

// region ---------- Field Access ----------

TEST_F(FBSciLexerTests, FieldAccessSuppressesKeyword) {
    expectStyles(
        "foo.dim ",
        "IIIPIII "
    );
}

TEST_F(FBSciLexerTests, ArrowAccessSuppressesKeyword) {
    expectStyles(
        "foo->dim ",
        "IIIPPIII "
    );
}

// endregion

// region ---------- Line Continuation ----------

TEST_F(FBSciLexerTests, LineContinuation) {
    expectStyles(
        "dim _\nx as integer ",
        "111 C I 11 2222222 "
    );
}

// endregion

// region ---------- Folding ----------

TEST_F(FBSciLexerTests, FoldFlatNoHeader) {
    EXPECT_EQ(fold("dim x\ndim y\n"), "0|0");
}

TEST_F(FBSciLexerTests, FoldIndentOpensFold) {
    EXPECT_EQ(
        fold("sub foo()\n    dim x\nend sub\n"),
        "0H|4|0"
    );
}

TEST_F(FBSciLexerTests, FoldNestedBlocks) {
    EXPECT_EQ(
        fold("sub foo()\n    if x then\n        dim y\n    end if\nend sub\n"),
        "0H|4H|8|4|0"
    );
}

TEST_F(FBSciLexerTests, FoldSubWithBlankFirstBodyLineOpensFold) {
    // Bug report: a SUB whose first body line is blank should still fold.
    // The header flag must land on the SUB despite the blank gap, AND
    // the blank line itself must inherit the body's indent (not the
    // header's) so Scintilla never sees a non-WHITE-blocking sibling at
    // the header level. Look-ahead, not look-back, decides the blank
    // line's level.
    EXPECT_EQ(
        fold(
            "SUB hello\n"
            "\n"
            "    print \"world\"\n"
            "END SUB\n"
        ),
        "0H|4W|4|0"
    );
}

TEST_F(FBSciLexerTests, FoldBlankLineInheritsAndPermitsHeader) {
    // Blank line gets WHITEFLAG at the BODY's indent (look-ahead), not the
    // header's — so Scintilla's fold-marker rendering never sees a
    // sibling at the header level. Header detection still looks past the
    // blank to line 2 to decide the SUB is foldable.
    EXPECT_EQ(
        fold("sub foo()\n\n    dim x\nend sub\n"),
        "0H|4W|4|0"
    );
}

TEST_F(FBSciLexerTests, FoldTabIndentRoundsToEight) {
    // Single leading tab rounds to column 8.
    EXPECT_EQ(
        fold("sub foo()\n\tdim x\nend sub\n"),
        "0H|8|0"
    );
}

TEST_F(FBSciLexerTests, FoldCommentContentNotSpecialCased) {
    // Folding is purely indentation-based — comment content is not treated
    // specially, so flat comment lines produce no fold.
    EXPECT_EQ(
        fold("x = 1 /' comment\ninside '/ dim y\n"),
        "0|0"
    );
    EXPECT_EQ(
        fold("/' inline '/\ndim x\n"),
        "0|0"
    );
}

TEST_F(FBSciLexerTests, FoldWorkedExample) {
    // Spec worked example: an outer block with a nested block inside, and a
    // blank line (line 3) that must not open, close, or break a block.
    EXPECT_EQ(
        fold(
            "start block\n"
            "    indented line\n"
            "\n"
            "    indented line\n"
            "    nested block\n"
            "        sub indented line\n"
            "    end nested block\n"
            "end of block\n"
        ),
        "0H|4|4W|4|4H|8|4|0"
    );
}

TEST_F(FBSciLexerTests, FoldIndentedRunWithoutOpenerDoesNotFold) {
    // Indented lines with no opener above them are not foldable — no header.
    // The leading blank inherits the body's indent via look-ahead.
    EXPECT_EQ(
        fold("\n    non-foldable line\n    non-foldable line\n\n"),
        "4W|4|4|4W"
    );
}

TEST_F(FBSciLexerTests, FoldNonFoldableLineMayHaveChildBlock) {
    // A non-foldable indented line is not a header, but a deeper block that
    // follows it still folds. Leading blank inherits the body's indent.
    EXPECT_EQ(
        fold(
            "\n"
            "    non-foldable line\n"
            "    block start\n"
            "        nested block\n"
            "        nested ends\n"
            "    non-foldable line\n"
        ),
        "4W|4|4H|8|8|4"
    );
}

TEST_F(FBSciLexerTests, FoldIncrementalSubrangeMatchesFull) {
    // Folding only the range of a single edited line must reproduce the same
    // levels as a full-document fold (back-up + look-ahead behaviour).
    const std::string src = "sub foo()\n    if x then\n        dim y\n    end if\nend sub\n";
    m_doc.Set(src);
    m_lexer->Lex(0, m_doc.Length(), +S::Default, &m_doc);
    m_lexer->Fold(0, m_doc.Length(), +S::Default, &m_doc);

    std::vector<int> full;
    for (Sci_Position line = 0; line < m_doc.MaxLine(); line++) {
        full.push_back(m_doc.GetLevel(line));
    }

    // Re-fold only the range covering line 2.
    const Sci_Position rangeStart = m_doc.LineStart(2);
    const Sci_Position rangeLen = m_doc.LineStart(3) - rangeStart;
    m_lexer->Fold(rangeStart, rangeLen, +S::Default, &m_doc);

    for (Sci_Position line = 0; line < m_doc.MaxLine(); line++) {
        EXPECT_EQ(m_doc.GetLevel(line), full[static_cast<std::size_t>(line)])
            << "line " << line;
    }
}

TEST_F(FBSciLexerTests, FoldMultilineCommentBlock) {
    // `/'` alone on its line opens a fold; the matching `'/` line is the
    // closer and stays visible.
    EXPECT_EQ(
        fold("/'\nsome comment\nmore comment\n'/print \"hello\"\n"),
        "0H|1|1|0"
    );
}

TEST_F(FBSciLexerTests, FoldNestedMultilineComment) {
    // A nested `/'` does not open a new fold; the whole comment is one region.
    // Blank line inside the comment is swallowed, not treated as a break.
    EXPECT_EQ(
        fold(
            "/'\n"
            "  comment\n"
            "\n"
            "  /'\n"
            "     nested comment\n"
            "  '/\n"
            "end'/\n"
            "print \"hello\"\n"
        ),
        "0H|1|1|1|1|1|0|0"
    );
}

TEST_F(FBSciLexerTests, FoldMlCommentAfterCodeDoesNotFold) {
    // `/'` preceded by non-whitespace content does not open a foldable region.
    EXPECT_EQ(
        fold("print \"x\" /'\ninside\n'/ print \"y\"\n"),
        "0|0|0"
    );
}

TEST_F(FBSciLexerTests, FoldMlCommentInsideIndentBlock) {
    // A comment region nested inside an indentation block: the block folds via
    // indentation, the comment folds as its own region one level deeper.
    EXPECT_EQ(
        fold("sub foo()\n    /'\n    comment\n    '/\nend sub\n"),
        "0H|4H|5|4|0"
    );
}

TEST_F(FBSciLexerTests, FoldMlCommentIncrementalMatchesFull) {
    // Folding a range inside a comment region must reproduce the full-document
    // levels — the incremental back-up walks out of the comment to its opener.
    const std::string src = "/'\n  comment\n\n  /'\n  nested\n  '/\nend'/\nx\n";
    m_doc.Set(src);
    m_lexer->Lex(0, m_doc.Length(), +S::Default, &m_doc);
    m_lexer->Fold(0, m_doc.Length(), +S::Default, &m_doc);

    std::vector<int> full;
    for (Sci_Position line = 0; line < m_doc.MaxLine(); line++) {
        full.push_back(m_doc.GetLevel(line));
    }

    // Re-fold only the range covering line 4 (inside the nested comment).
    const Sci_Position rangeStart = m_doc.LineStart(4);
    const Sci_Position rangeLen = m_doc.LineStart(5) - rangeStart;
    m_lexer->Fold(rangeStart, rangeLen, +S::Default, &m_doc);

    for (Sci_Position line = 0; line < m_doc.MaxLine(); line++) {
        EXPECT_EQ(m_doc.GetLevel(line), full[static_cast<std::size_t>(line)])
            << "line " << line;
    }
}

// endregion
