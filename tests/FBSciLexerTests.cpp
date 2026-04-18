//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "editor/lexilla/FBSciLexer.hpp"
#include "TestDocument.h"
#include <gtest/gtest.h>

using namespace fbide;
using S = FBSciLexerState;

class FBSciLexerTests : public testing::Test {
protected:
    void SetUp() override {
        m_lexer = FBSciLexer::Create();
        // Set up keyword lists matching fbfull.lng groups
        m_lexer->WordListSet(0, "dim as if then else end sub function type");
        m_lexer->WordListSet(1, "integer string single double long byte");
        m_lexer->WordListSet(2, "and or not mod xor");
        m_lexer->WordListSet(3, "__fb_version__");
        m_lexer->WordListSet(4, "true false nothing");
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
    ///   ' ' = Default    'C' = Comment       'M' = MultilineComment
    ///   'N' = Number     'S' = String         'O' = StringOpen
    ///   'I' = Identifier '1' = Keyword1       '2' = Keyword2
    ///   '3' = Keyword3   '4' = Keyword4       '5' = Keyword5
    ///   'P' = Operator   'L' = Label          'V' = Constant
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
            table['5'] = S::Keyword5;
            table['P'] = S::Operator;
            table['L'] = S::Label;
            table['V'] = S::Constant;
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
            table[+S::Keyword5] = '5';
            table[+S::Operator] = 'P';
            table[+S::Label] = 'L';
            table[+S::Constant] = 'V';
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
