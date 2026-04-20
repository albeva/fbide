//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "TestHelpers.hpp"
#include <gtest/gtest.h>

using namespace fbide;
using lexer::KeywordKind;
using lexer::Lexer;
using lexer::Token;
using lexer::TokenKind;

class LexerTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;

    void SetUp() override {
        const auto groups = tests::loadKeywordGroups(testDataPath + "fbfull.lng");
        m_lexer = std::make_unique<Lexer>(groups);
    }

    auto tokenise(const char* source) -> std::vector<Token> {
        return m_lexer->tokenise(source);
    }

    // Helper: return only non-whitespace, non-newline tokens
    auto significant(const char* source) -> std::vector<Token> {
        auto tokens = tokenise(source);
        std::erase_if(tokens, [](const Token& tok) {
            return tok.kind == TokenKind::Whitespace || tok.kind == TokenKind::Newline;
        });
        return tokens;
    }

    std::unique_ptr<Lexer> m_lexer;
};

// ---------------------------------------------------------------------------
// Empty and whitespace
// ---------------------------------------------------------------------------

TEST_F(LexerTests, EmptySource) {
    const auto tokens = tokenise("");
    EXPECT_TRUE(tokens.empty());
}

TEST_F(LexerTests, WhitespaceOnly) {
    const auto tokens = tokenise("   \t  ");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Whitespace);
    EXPECT_EQ(tokens[0].text, "   \t  ");
}

// ---------------------------------------------------------------------------
// Newlines
// ---------------------------------------------------------------------------

TEST_F(LexerTests, NewlineLF) {
    const auto tokens = tokenise("\n");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Newline);
    EXPECT_EQ(tokens[0].text, "\n");
}

TEST_F(LexerTests, NewlineCRLF) {
    const auto tokens = tokenise("\r\n");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Newline);
    EXPECT_EQ(tokens[0].text, "\r\n");
}

TEST_F(LexerTests, NewlineCR) {
    const auto tokens = tokenise("\r");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Newline);
}

// ---------------------------------------------------------------------------
// Comments
// ---------------------------------------------------------------------------

TEST_F(LexerTests, SingleLineComment) {
    const auto tokens = significant("' this is a comment");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Comment);
    EXPECT_EQ(tokens[0].text, "' this is a comment");
}

TEST_F(LexerTests, RemComment) {
    const auto tokens = significant("REM this is a comment");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Comment);
    EXPECT_EQ(tokens[0].text, "REM this is a comment");
}

TEST_F(LexerTests, RemCaseInsensitive) {
    const auto tokens = significant("rem a comment");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Comment);
}

TEST_F(LexerTests, BlockComment) {
    const auto tokens = significant("/' block comment '/");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::CommentBlock);
    EXPECT_EQ(tokens[0].text, "/' block comment '/");
}

TEST_F(LexerTests, NestedBlockComment) {
    const auto tokens = significant("/' outer /' inner '/ outer '/");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::CommentBlock);
    EXPECT_EQ(tokens[0].text, "/' outer /' inner '/ outer '/");
}

TEST_F(LexerTests, UnterminatedBlockComment) {
    const auto tokens = significant("/' no closing");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::CommentBlock);
    EXPECT_EQ(tokens[0].text, "/' no closing");
}

TEST_F(LexerTests, CommentStopsAtNewline) {
    const auto tokens = tokenise("' comment\ncode");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].kind, TokenKind::Comment);
    EXPECT_EQ(tokens[0].text, "' comment");
    EXPECT_EQ(tokens[1].kind, TokenKind::Newline);
    EXPECT_EQ(tokens[2].kind, TokenKind::Identifier);
}

// ---------------------------------------------------------------------------
// Strings — normal
// ---------------------------------------------------------------------------

TEST_F(LexerTests, SimpleString) {
    const auto tokens = significant("\"hello\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
    EXPECT_EQ(tokens[0].text, "\"hello\"");
}

TEST_F(LexerTests, DoubledQuotesInString) {
    const auto tokens = significant("\"hello \"\"world\"\"\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
    EXPECT_EQ(tokens[0].text, "\"hello \"\"world\"\"\"");
}

TEST_F(LexerTests, UnterminatedString) {
    const auto tokens = significant("\"unterminated\nDim x");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].kind, TokenKind::UnterminatedString);
    EXPECT_EQ(tokens[0].text, "\"unterminated");
    EXPECT_EQ(tokens[1].kind, TokenKind::Keyword1); // Dim
    EXPECT_EQ(tokens[2].kind, TokenKind::Identifier); // x
}

TEST_F(LexerTests, UnterminatedStringAtEOF) {
    const auto tokens = significant("\"no close");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::UnterminatedString);
    EXPECT_EQ(tokens[0].text, "\"no close");
}

TEST_F(LexerTests, InvalidCharacter) {
    const auto tokens = significant("\x01");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Invalid);
}

TEST_F(LexerTests, EmptyString) {
    const auto tokens = significant("\"\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
    EXPECT_EQ(tokens[0].text, "\"\"");
}

// ---------------------------------------------------------------------------
// Strings — escaped (!"...")
// ---------------------------------------------------------------------------

TEST_F(LexerTests, EscapedString) {
    const auto tokens = significant("!\"hello\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
    EXPECT_EQ(tokens[0].text, "!\"hello\"");
}

TEST_F(LexerTests, EscapedStringWithBackslash) {
    const auto tokens = significant("!\"hello \\\"world\\\"\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
    EXPECT_EQ(tokens[0].text, "!\"hello \\\"world\\\"\"");
}

TEST_F(LexerTests, EscapedStringNewlineEscape) {
    const auto tokens = significant("!\"line1\\nline2\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
    EXPECT_EQ(tokens[0].text, "!\"line1\\nline2\"");
}

TEST_F(LexerTests, EscapedStringBackslashAtEnd) {
    // !"abc\ at EOF — backslash with nothing after, unterminated
    const auto tokens = significant("!\"abc\\");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::UnterminatedString);
}

TEST_F(LexerTests, EscapedStringDoubledQuotes) {
    // "" works in escaped strings too
    const auto tokens = significant("!\"hello \"\"world\"\"\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
    EXPECT_EQ(tokens[0].text, "!\"hello \"\"world\"\"\"");
}

// ---------------------------------------------------------------------------
// Strings — dollar ($"...")
// ---------------------------------------------------------------------------

TEST_F(LexerTests, DollarString) {
    const auto tokens = significant("$\"hello\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
    EXPECT_EQ(tokens[0].text, "$\"hello\"");
}

TEST_F(LexerTests, DollarStringNoEscapes) {
    // backslash is literal in $"..."
    const auto tokens = significant("$\"C:\\path\\file\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
    EXPECT_EQ(tokens[0].text, "$\"C:\\path\\file\"");
}

TEST_F(LexerTests, DollarStringDoubledQuotes) {
    const auto tokens = significant("$\"say \"\"hi\"\"\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
    EXPECT_EQ(tokens[0].text, "$\"say \"\"hi\"\"\"");
}

TEST_F(LexerTests, DollarAloneIsIdentifier) {
    // '$' not followed by '"' is a word char
    const auto tokens = significant("x$");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[0].text, "x$");
}

// ---------------------------------------------------------------------------
// Numbers — integers
// ---------------------------------------------------------------------------

TEST_F(LexerTests, IntegerDecimal) {
    const auto tokens = significant("42");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "42");
}

TEST_F(LexerTests, IntegerHex) {
    const auto tokens = significant("&HFF");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "&HFF");
}

TEST_F(LexerTests, IntegerOctal) {
    const auto tokens = significant("&O77");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "&O77");
}

TEST_F(LexerTests, IntegerBinary) {
    const auto tokens = significant("&B1010");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "&B1010");
}

TEST_F(LexerTests, AmpersandAloneIsOperator) {
    // '&' not followed by H/O/B is operator
    const auto tokens = significant("&");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Operator);
}

TEST_F(LexerTests, NumberWithTypeSuffix) {
    const auto tokens = significant("100UL");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "100UL");
}

// ---------------------------------------------------------------------------
// Numbers — floating point
// ---------------------------------------------------------------------------

TEST_F(LexerTests, FloatWithDecimal) {
    const auto tokens = significant("3.14");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "3.14");
}

TEST_F(LexerTests, FloatLeadingDot) {
    const auto tokens = significant(".5");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, ".5");
}

TEST_F(LexerTests, FloatExponentPlus) {
    const auto tokens = significant("743.1e+13");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "743.1e+13");
}

TEST_F(LexerTests, FloatExponentMinus) {
    const auto tokens = significant("1.5e-3");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "1.5e-3");
}

TEST_F(LexerTests, FloatExponentD) {
    const auto tokens = significant("2.0d+10");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "2.0d+10");
}

TEST_F(LexerTests, FloatExponentNoSign) {
    const auto tokens = significant("1.5e10");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "1.5e10");
}

TEST_F(LexerTests, DotAloneIsOperator) {
    // '.' not followed by digit is operator
    const auto tokens = significant("x.y");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[1].kind, TokenKind::Operator);
    EXPECT_EQ(tokens[1].text, ".");
    EXPECT_EQ(tokens[2].kind, TokenKind::Identifier);
}

// ---------------------------------------------------------------------------
// Numbers — no exponent sign without decimal
// ---------------------------------------------------------------------------

TEST_F(LexerTests, IntegerPlusIsNotExponent) {
    // 2+3 must be number + operator + number, not a single number
    const auto tokens = significant("2+3");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "2");
    EXPECT_EQ(tokens[1].kind, TokenKind::Operator);
    EXPECT_EQ(tokens[1].text, "+");
    EXPECT_EQ(tokens[2].kind, TokenKind::Number);
    EXPECT_EQ(tokens[2].text, "3");
}

TEST_F(LexerTests, HexDPlusIsNotExponent) {
    // &HFFd+3 — 'd' is a hex digit, '+' must not be consumed
    const auto tokens = significant("&HFFd+3");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].kind, TokenKind::Number);
    EXPECT_EQ(tokens[0].text, "&HFFd");
    EXPECT_EQ(tokens[1].kind, TokenKind::Operator);
    EXPECT_EQ(tokens[2].kind, TokenKind::Number);
}

// ---------------------------------------------------------------------------
// Keywords
// ---------------------------------------------------------------------------

TEST_F(LexerTests, KeywordCaseInsensitive) {
    const auto t1 = significant("DIM");
    const auto t2 = significant("dim");
    const auto t3 = significant("Dim");
    ASSERT_EQ(t1.size(), 1);
    ASSERT_EQ(t2.size(), 1);
    ASSERT_EQ(t3.size(), 1);
    EXPECT_EQ(t1[0].kind, TokenKind::Keyword1);
    EXPECT_EQ(t2[0].kind, TokenKind::Keyword1);
    EXPECT_EQ(t3[0].kind, TokenKind::Keyword1);
}

TEST_F(LexerTests, StructuralKeywordKind) {
    const auto tokens = significant("Sub");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Keyword1);
    EXPECT_EQ(tokens[0].keywordKind, KeywordKind::Sub);
}

TEST_F(LexerTests, NonStructuralKeyword) {
    const auto tokens = significant("Dim");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Keyword1);
    EXPECT_EQ(tokens[0].keywordKind, KeywordKind::Other);
}

TEST_F(LexerTests, IdentifierNotKeyword) {
    const auto tokens = significant("myVariable");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[0].keywordKind, KeywordKind::None);
}

// ---------------------------------------------------------------------------
// Operators
// ---------------------------------------------------------------------------

using lexer::OperatorKind;

TEST_F(LexerTests, Operators) {
    const auto tokens = significant("()+*");
    ASSERT_EQ(tokens.size(), 4);
    for (const auto& tok : tokens) {
        EXPECT_EQ(tok.kind, TokenKind::Operator);
    }
}

TEST_F(LexerTests, ExclamationAloneIsOperator) {
    // '!' not followed by '"' is operator
    const auto tokens = significant("!x");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].kind, TokenKind::Operator);
    EXPECT_EQ(tokens[0].text, "!");
}

// ---------------------------------------------------------------------------
// Compound operators
// ---------------------------------------------------------------------------

TEST_F(LexerTests, CompoundAddAssign) {
    const auto tokens = significant("x += 1");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].kind, TokenKind::Operator);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::AddAssign);
    EXPECT_EQ(tokens[1].text, "+=");
}

TEST_F(LexerTests, CompoundSubAssign) {
    const auto tokens = significant("x -= 1");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::SubAssign);
    EXPECT_EQ(tokens[1].text, "-=");
}

TEST_F(LexerTests, CompoundMulAssign) {
    const auto tokens = significant("x *= 2");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::MulAssign);
    EXPECT_EQ(tokens[1].text, "*=");
}

TEST_F(LexerTests, CompoundDivAssign) {
    const auto tokens = significant("x /= 2");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::DivAssign);
    EXPECT_EQ(tokens[1].text, "/=");
}

TEST_F(LexerTests, CompoundIntDivAssign) {
    const auto tokens = significant("x \\= 2");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::IntDivAssign);
    EXPECT_EQ(tokens[1].text, "\\=");
}

TEST_F(LexerTests, CompoundExpAssign) {
    const auto tokens = significant("x ^= 2");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::ExpAssign);
    EXPECT_EQ(tokens[1].text, "^=");
}

TEST_F(LexerTests, CompoundConcatAssign) {
    const auto tokens = significant("x &= y");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::ConcatAssign);
    EXPECT_EQ(tokens[1].text, "&=");
}

TEST_F(LexerTests, CompoundNotEqual) {
    const auto tokens = significant("x <> y");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::NotEqual);
    EXPECT_EQ(tokens[1].text, "<>");
}

TEST_F(LexerTests, CompoundLessEqual) {
    const auto tokens = significant("x <= y");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::LessEqual);
    EXPECT_EQ(tokens[1].text, "<=");
}

TEST_F(LexerTests, CompoundGreaterEqual) {
    const auto tokens = significant("x >= y");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::GreaterEqual);
    EXPECT_EQ(tokens[1].text, ">=");
}

TEST_F(LexerTests, CompoundShiftLeft) {
    const auto tokens = significant("x << 2");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::ShiftLeft);
    EXPECT_EQ(tokens[1].text, "<<");
}

TEST_F(LexerTests, CompoundShiftRight) {
    const auto tokens = significant("x >> 2");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::ShiftRight);
    EXPECT_EQ(tokens[1].text, ">>");
}

TEST_F(LexerTests, CompoundShlAssign) {
    const auto tokens = significant("x <<= 2");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::ShlAssign);
    EXPECT_EQ(tokens[1].text, "<<=");
}

TEST_F(LexerTests, CompoundShrAssign) {
    const auto tokens = significant("x >>= 2");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::ShrAssign);
    EXPECT_EQ(tokens[1].text, ">>=");
}

TEST_F(LexerTests, CompoundArrow) {
    const auto tokens = significant("p->x");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::Arrow);
    EXPECT_EQ(tokens[1].text, "->");
}

// ---------------------------------------------------------------------------
// Operator kind classification
// ---------------------------------------------------------------------------

TEST_F(LexerTests, OperatorKindParens) {
    const auto tokens = significant("()[]{}");
    ASSERT_EQ(tokens.size(), 6);
    EXPECT_EQ(tokens[0].operatorKind, OperatorKind::ParenOpen);
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::ParenClose);
    EXPECT_EQ(tokens[2].operatorKind, OperatorKind::BracketOpen);
    EXPECT_EQ(tokens[3].operatorKind, OperatorKind::BracketClose);
    EXPECT_EQ(tokens[4].operatorKind, OperatorKind::BraceOpen);
    EXPECT_EQ(tokens[5].operatorKind, OperatorKind::BraceClose);
}

TEST_F(LexerTests, OperatorKindPunctuation) {
    const auto tokens = significant("a , ; : ?");
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::Comma);
    EXPECT_EQ(tokens[2].operatorKind, OperatorKind::Semicolon);
    EXPECT_EQ(tokens[3].operatorKind, OperatorKind::Colon);
    EXPECT_EQ(tokens[4].operatorKind, OperatorKind::Question);
}

TEST_F(LexerTests, OperatorKindDot) {
    const auto tokens = significant("x.y");
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::Dot);
}

TEST_F(LexerTests, OperatorKindEllipsis) {
    const auto tokens = significant("a..b");
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::Ellipsis2);
    const auto tokens2 = significant("a...");
    EXPECT_EQ(tokens2[1].operatorKind, OperatorKind::Ellipsis3);
}

// ---------------------------------------------------------------------------
// Binary vs unary operator detection
// ---------------------------------------------------------------------------

TEST_F(LexerTests, BinaryMinusAfterIdentifier) {
    const auto tokens = significant("a - b");
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::Subtract);
}

TEST_F(LexerTests, UnaryMinusAfterOperator) {
    const auto tokens = significant("= -3");
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::Negate);
}

TEST_F(LexerTests, UnaryMinusAfterOpenParen) {
    const auto tokens = significant("(-3)");
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::Negate);
}

TEST_F(LexerTests, UnaryMinusAfterComma) {
    const auto tokens = significant("f(a, -b)");
    EXPECT_EQ(tokens[4].operatorKind, OperatorKind::Negate);
}

TEST_F(LexerTests, BinaryMinusAfterCloseParen) {
    const auto tokens = significant("(a) - b");
    ASSERT_EQ(tokens.size(), 5);
    EXPECT_EQ(tokens[3].operatorKind, OperatorKind::Subtract);
}

TEST_F(LexerTests, BinaryMinusAfterNumber) {
    const auto tokens = significant("3 - 1");
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::Subtract);
}

TEST_F(LexerTests, UnaryPlusAfterAssign) {
    const auto tokens = significant("x = +3");
    EXPECT_EQ(tokens[2].operatorKind, OperatorKind::UnaryPlus);
}

TEST_F(LexerTests, BinaryPlusAfterIdentifier) {
    const auto tokens = significant("a + b");
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::Add);
}

TEST_F(LexerTests, DerefAfterAssign) {
    const auto tokens = significant("x = *ptr");
    EXPECT_EQ(tokens[2].operatorKind, OperatorKind::Dereference);
}

TEST_F(LexerTests, BinaryMultiplyAfterIdentifier) {
    const auto tokens = significant("a * b");
    EXPECT_EQ(tokens[1].operatorKind, OperatorKind::Multiply);
}

TEST_F(LexerTests, AddressOfAlwaysUnary) {
    const auto tokens = significant("@x");
    EXPECT_EQ(tokens[0].operatorKind, OperatorKind::AddressOf);
}

TEST_F(LexerTests, UnaryAtStartOfLine) {
    const auto tokens = significant("-x");
    EXPECT_EQ(tokens[0].operatorKind, OperatorKind::Negate);
}

// ---------------------------------------------------------------------------
// Preprocessor
// ---------------------------------------------------------------------------

TEST_F(LexerTests, PreprocessorDirective) {
    const auto tokens = significant("#include \"file.bi\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Preprocessor);
    EXPECT_EQ(tokens[0].text, "#include \"file.bi\"");
}

TEST_F(LexerTests, HashNotAtLineStartIsOperator) {
    const auto tokens = significant("x #y");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[1].kind, TokenKind::Operator);
    EXPECT_EQ(tokens[1].text, "#");
    EXPECT_EQ(tokens[2].kind, TokenKind::Identifier);
}

// ---------------------------------------------------------------------------
// Mixed expressions
// ---------------------------------------------------------------------------

TEST_F(LexerTests, SimpleAssignment) {
    const auto tokens = significant("Dim x = .3");
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0].kind, TokenKind::Keyword1);
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[2].kind, TokenKind::Operator);
    EXPECT_EQ(tokens[3].kind, TokenKind::Number);
    EXPECT_EQ(tokens[3].text, ".3");
}

TEST_F(LexerTests, ArithmeticExpression) {
    const auto tokens = significant("x = 2 + 3.5e+2");
    ASSERT_EQ(tokens.size(), 5);
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[1].kind, TokenKind::Operator);
    EXPECT_EQ(tokens[2].kind, TokenKind::Number);
    EXPECT_EQ(tokens[2].text, "2");
    EXPECT_EQ(tokens[3].kind, TokenKind::Operator);
    EXPECT_EQ(tokens[3].text, "+");
    EXPECT_EQ(tokens[4].kind, TokenKind::Number);
    EXPECT_EQ(tokens[4].text, "3.5e+2");
}

// ---------------------------------------------------------------------------
// UTF-8 in comments
// ---------------------------------------------------------------------------

TEST_F(LexerTests, Utf8InComment2Byte) {
    // 2-byte: é = C3 A9
    const auto tokens = significant("' café");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Comment);
    EXPECT_EQ(tokens[0].text, "' caf\xC3\xA9");
}

TEST_F(LexerTests, Utf8InComment3Byte) {
    // 3-byte: € = E2 82 AC
    const auto tokens = significant("' price: 10\xE2\x82\xAC");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Comment);
}

TEST_F(LexerTests, Utf8InComment4Byte) {
    // 4-byte: 😀 = F0 9F 98 80
    const auto tokens = significant("' emoji \xF0\x9F\x98\x80 here");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Comment);
}

TEST_F(LexerTests, Utf8InBlockComment) {
    const auto tokens = significant("/\' \xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80 \'/");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::CommentBlock);
}

// ---------------------------------------------------------------------------
// UTF-8 in string literals
// ---------------------------------------------------------------------------

TEST_F(LexerTests, Utf8InString2Byte) {
    // "café"
    const auto tokens = significant("\"caf\xC3\xA9\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
    EXPECT_EQ(tokens[0].text, "\"caf\xC3\xA9\"");
}

TEST_F(LexerTests, Utf8InString3Byte) {
    // "10€"
    const auto tokens = significant("\"10\xE2\x82\xAC\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
}

TEST_F(LexerTests, Utf8InString4Byte) {
    // "😀"
    const auto tokens = significant("\"\xF0\x9F\x98\x80\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
}

TEST_F(LexerTests, Utf8InEscapedString) {
    // !"héllo"
    const auto tokens = significant("!\"h\xC3\xA9llo\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
}

TEST_F(LexerTests, Utf8InDollarString) {
    // $"日本語"  (3-byte chars)
    const auto tokens = significant("$\"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\"");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::String);
}

// ---------------------------------------------------------------------------
// Non-ASCII outside comments/strings is invalid
// ---------------------------------------------------------------------------

TEST_F(LexerTests, NonAsciiOutsideCommentOrStringIsInvalid) {
    // é (C3 A9) at top level — each byte is invalid
    const auto tokens = significant("\xC3\xA9");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].kind, TokenKind::Invalid);
    EXPECT_EQ(tokens[1].kind, TokenKind::Invalid);
}

TEST_F(LexerTests, NonAsciiMixedWithCode) {
    // "Dim x" then a non-ASCII byte, then more code
    const auto tokens = significant("Dim x\xC3\xA9= 1");
    // Dim, x, invalid, invalid, =, 1
    ASSERT_EQ(tokens.size(), 6);
    EXPECT_EQ(tokens[0].kind, TokenKind::Keyword1); // Dim
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier); // x
    EXPECT_EQ(tokens[2].kind, TokenKind::Invalid); // C3
    EXPECT_EQ(tokens[3].kind, TokenKind::Invalid); // A9
    EXPECT_EQ(tokens[4].kind, TokenKind::Operator); // =
    EXPECT_EQ(tokens[5].kind, TokenKind::Number); // 1
}
