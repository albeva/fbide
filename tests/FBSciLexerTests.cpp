//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "editor/lexilla/FBSciLexer.hpp"
#include "Scintilla.h"
#include "TestDocument.h"
#include <gtest/gtest.h>

using namespace fbide;
using S = ThemeCategory;

class FBSciLexerTests : public testing::Test {
protected:
    void SetUp() override {
        m_lexer = FBSciLexer::Create();
        // Set up keyword lists matching fbfull.lng groups
        // Indices map 1:1 to DEFINE_THEME_KEYWORD_GROUPS order.
        m_lexer->WordListSet(0, "dim as if then else end sub function type asm"); // Keyword1
        m_lexer->WordListSet(1, "integer string single double long byte");        // Keyword2
        m_lexer->WordListSet(2, "and or not mod xor");                            // Keyword3
        m_lexer->WordListSet(3, "__fb_version__");                                // Keyword4
        m_lexer->WordListSet(4, "");                                              // KeywordCustom1
        m_lexer->WordListSet(5, "");                                              // KeywordCustom2
        m_lexer->WordListSet(6, "if ifdef ifndef else elseif endif macro endmacro"); // KeywordPP
        m_lexer->WordListSet(7, "mov push pop ret jmp");                          // KeywordAsm1
        m_lexer->WordListSet(8, "eax ebx ecx edx");                               // KeywordAsm2
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
    ///   'I' = Identifier '1'..'4' = Keyword1..4  '5' = KeywordCustom1
    ///   '6' = KeywordCustom2 '7' = KeywordAsm1  '8' = KeywordAsm2
    ///   'P' = Operator   'L' = Label             'V' = Constant
    ///   '#' = Preprocessor  'E' = Error
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
            table['1'] = S::Keyword1;
            table['2'] = S::Keyword2;
            table['3'] = S::Keyword3;
            table['4'] = S::Keyword4;
            table['5'] = S::KeywordCustom1;
            table['6'] = S::KeywordCustom2;
            table['7'] = S::KeywordAsm1;
            table['8'] = S::KeywordAsm2;
            table['P'] = S::Operator;
            table['L'] = S::Label;
            table['#'] = S::Preprocessor;
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
            table[+S::Keyword1] = '1';
            table[+S::Keyword2] = '2';
            table[+S::Keyword3] = '3';
            table[+S::Keyword4] = '4';
            table[+S::KeywordCustom1] = '5';
            table[+S::KeywordCustom2] = '6';
            table[+S::KeywordAsm1] = '7';
            table[+S::KeywordAsm2] = '8';
            table[+S::Operator] = 'P';
            table[+S::Label] = 'L';
            table[+S::Preprocessor] = '#';
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
    TestDocument m_doc;
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
    // Regular keyword lists (Keyword1-4, KeywordCustom1-2) are suppressed
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
        "########### "
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

TEST_F(FBSciLexerTests, FoldBlankLineInheritsAndPermitsHeader) {
    // Blank line gets WHITEFLAG; header detection looks past it to line 2.
    EXPECT_EQ(
        fold("sub foo()\n\n    dim x\nend sub\n"),
        "0H|0W|4|0"
    );
}

TEST_F(FBSciLexerTests, FoldTabIndentRoundsToEight) {
    // Single leading tab rounds to column 8.
    EXPECT_EQ(
        fold("sub foo()\n\tdim x\nend sub\n"),
        "0H|8|0"
    );
}

TEST_F(FBSciLexerTests, FoldMultilineCommentBlockFoldsWhenPure) {
    // Opener on its own line, body all comment, closer alone -> one fold.
    EXPECT_EQ(
        fold("/' comment\n   inside\n   '/\ndim x\n"),
        "0H|1|1|0"
    );
}

TEST_F(FBSciLexerTests, FoldMultilineCommentOpenerAfterCodeDoesNotFold) {
    // Opener line has code before `/'` so the block must not fold.
    EXPECT_EQ(
        fold("x = 1 /' comment\ninside '/ dim y\n"),
        "0|0"
    );
}

TEST_F(FBSciLexerTests, FoldSingleLineMultilineCommentNoFold) {
    // `/' ... '/` on a single line: pure ML but no continuation, so no fold.
    EXPECT_EQ(
        fold("/' inline '/\ndim x\n"),
        "0|0"
    );
}

TEST_F(FBSciLexerTests, FoldIndentedMultilineCommentBlock) {
    // Opener indented; continuation line has zero indent. Fold level for
    // continuation must still exceed the opener so the fold is valid.
    EXPECT_EQ(
        fold("    /' start\ncontinuation\n    '/\n"),
        "4H|5|5"
    );
}

// endregion
