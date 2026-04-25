//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
#include <gtest/gtest.h>

using namespace fbide;
using namespace fbide::lexer;

class StyleLexerTests : public testing::Test {
protected:
    void SetUp() override {
        m_lexer = FBSciLexer::Create();
        // Mirror FBSciLexerTests fixture: realistic wordlists.
        m_lexer->WordListSet(0, "dim as if then else end sub function type asm");
        m_lexer->WordListSet(1, "integer string single double long byte");
        m_lexer->WordListSet(2, "and or not mod xor");
        m_lexer->WordListSet(3, "__fb_version__");
        m_lexer->WordListSet(4, "");
        m_lexer->WordListSet(5, "");
        m_lexer->WordListSet(6, "if ifdef ifndef else elseif endif macro endmacro");
        m_lexer->WordListSet(7, "mov push pop ret jmp");
        m_lexer->WordListSet(8, "eax ebx ecx edx");
    }

    void TearDown() override {
        m_lexer->Release();
        m_lexer = nullptr;
    }

    auto lex(const std::string& src) -> std::vector<Token> {
        m_doc.Set(src);
        m_lexer->Lex(0, m_doc.Length(), +ThemeCategory::Default, &m_doc);
        MemoryDocStyledSource source(m_doc);
        StyleLexer adapter(source);
        return adapter.tokenise();
    }

    /// Drop Whitespace and Newline tokens to simplify assertions.
    static auto strip(std::vector<Token> tokens) -> std::vector<Token> {
        std::erase_if(tokens, [](const Token& t) {
            return t.kind == TokenKind::Whitespace || t.kind == TokenKind::Newline;
        });
        return tokens;
    }

    Scintilla::ILexer5* m_lexer { nullptr };
    MemoryDocument m_doc;
};

// region ---------- Basic shape ----------

TEST_F(StyleLexerTests, EmptySource) {
    EXPECT_TRUE(lex("").empty());
}

TEST_F(StyleLexerTests, WhitespaceOnly) {
    auto t = lex("   \t  ");
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Whitespace);
    EXPECT_EQ(t[0].text, "   \t  ");
}

TEST_F(StyleLexerTests, NewlineLF) {
    auto t = lex("\n");
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Newline);
    EXPECT_EQ(t[0].text, "\n");
}

TEST_F(StyleLexerTests, NewlineCRLF) {
    auto t = lex("\r\n");
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Newline);
    EXPECT_EQ(t[0].text, "\r\n");
}

TEST_F(StyleLexerTests, NewlineCR) {
    auto t = lex("\r");
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Newline);
    EXPECT_EQ(t[0].text, "\r");
}

// endregion

// region ---------- Identifiers + keywords ----------

TEST_F(StyleLexerTests, PlainIdentifier) {
    auto t = strip(lex("hello"));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Identifier);
    EXPECT_EQ(t[0].keywordKind, KeywordKind::None);
    EXPECT_EQ(t[0].style, ThemeCategory::Identifier);
    EXPECT_EQ(t[0].text, "hello");
}

TEST_F(StyleLexerTests, Keyword1ClassifiedAsIf) {
    auto t = strip(lex("if"));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Keywords);
    EXPECT_EQ(t[0].keywordKind, KeywordKind::If);
    EXPECT_EQ(t[0].style, ThemeCategory::Keywords);
}

TEST_F(StyleLexerTests, Keyword2NonStructural) {
    auto t = strip(lex("integer"));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::KeywordTypes);
    EXPECT_EQ(t[0].keywordKind, KeywordKind::Other); // not a structural keyword
}

TEST_F(StyleLexerTests, FieldAccessIntegerIsIdentifier) {
    // The motivating bug: `obj.integer` — `integer` after `.` must be
    // styled as Identifier by FBSciLexer, so StyleLexer emits Identifier,
    // not Keyword2. Trailing whitespace forces the lexer to commit the
    // word boundary (matches FBSciLexerTests.FieldAccessSuppressesKeyword).
    auto t = strip(lex("obj.integer "));
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[0].kind, TokenKind::Identifier);
    EXPECT_EQ(t[0].text, "obj");
    EXPECT_EQ(t[1].kind, TokenKind::Operator);
    EXPECT_EQ(t[1].operatorKind, OperatorKind::Dot);
    EXPECT_EQ(t[2].kind, TokenKind::Identifier);
    EXPECT_EQ(t[2].keywordKind, KeywordKind::None);
    EXPECT_EQ(t[2].text, "integer");
}

TEST_F(StyleLexerTests, FieldAccessAcrossLineContinuation) {
    // `this _\n . _\n integer()` — FBSciLexer carries field-access state
    // through `_` continuation. `integer` must remain Identifier.
    auto t = strip(lex("this _\n . _\n integer()"));
    // Find the `integer` token.
    bool found = false;
    for (const auto& tok : t) {
        if (tok.text == "integer") {
            EXPECT_EQ(tok.kind, TokenKind::Identifier);
            EXPECT_EQ(tok.keywordKind, KeywordKind::None);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(StyleLexerTests, ArrowMemberAccess) {
    auto t = strip(lex("obj->integer "));
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[1].operatorKind, OperatorKind::Arrow);
    EXPECT_EQ(t[2].kind, TokenKind::Identifier); // not Keyword2
}

// endregion

// region ---------- Operators ----------

TEST_F(StyleLexerTests, OperatorDispatchTable) {
    auto t = strip(lex("(),;:.?[]{}=+-*@"));
    std::vector<OperatorKind> kinds;
    for (const auto& tok : t) {
        kinds.push_back(tok.operatorKind);
    }
    EXPECT_EQ(kinds, (std::vector<OperatorKind> {
        OperatorKind::ParenOpen,
        OperatorKind::ParenClose,
        OperatorKind::Comma,
        OperatorKind::Semicolon,
        OperatorKind::Colon,
        OperatorKind::Dot,
        OperatorKind::Question,
        OperatorKind::BracketOpen,
        OperatorKind::BracketClose,
        OperatorKind::BraceOpen,
        OperatorKind::BraceClose,
        OperatorKind::Assign,
        OperatorKind::UnaryPlus,    // line-start → unary
        OperatorKind::Negate,       // after UnaryPlus → unary
        OperatorKind::Dereference,  // after Negate → unary
        OperatorKind::AddressOf,    // always unary
    }));
}

TEST_F(StyleLexerTests, OperatorOtherFallback) {
    auto t = strip(lex("a < b"));
    // 3 tokens: Identifier, Operator(Other), Identifier
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[1].kind, TokenKind::Operator);
    EXPECT_EQ(t[1].operatorKind, OperatorKind::Other);
    EXPECT_EQ(t[1].text, "<");
}

TEST_F(StyleLexerTests, UnaryAfterIdentifier) {
    auto t = strip(lex("a + b"));
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[1].operatorKind, OperatorKind::Add); // binary, after Identifier
}

TEST_F(StyleLexerTests, EllipsisOperator) {
    auto t = strip(lex("a ... b"));
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[1].operatorKind, OperatorKind::Ellipsis3);
}

// endregion

// region ---------- Numbers / strings / comments ----------

TEST_F(StyleLexerTests, Number) {
    auto t = strip(lex("123"));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Number);
    EXPECT_EQ(t[0].text, "123");
}

TEST_F(StyleLexerTests, String) {
    auto t = strip(lex("\"hi\""));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::String);
    EXPECT_EQ(t[0].text, "\"hi\"");
}

TEST_F(StyleLexerTests, UnterminatedString) {
    auto t = strip(lex("\"hi\n"));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::UnterminatedString);
}

TEST_F(StyleLexerTests, SingleLineComment) {
    auto t = strip(lex("' hello\n"));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Comment);
    EXPECT_EQ(t[0].text, "' hello");
}

TEST_F(StyleLexerTests, BlockComment) {
    auto t = strip(lex("/' a '/"));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::CommentBlock);
}

// endregion

// region ---------- Preprocessor ----------

TEST_F(StyleLexerTests, PreprocessorIfDef) {
    auto t = strip(lex("#ifdef FOO"));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Preprocessor);
    EXPECT_EQ(t[0].keywordKind, KeywordKind::PpIfDef);
    EXPECT_EQ(t[0].style, ThemeCategory::KeywordPP);
    // PP token coalesces the directive + body.
    EXPECT_NE(t[0].text.find("ifdef"), std::string::npos);
    EXPECT_NE(t[0].text.find("FOO"), std::string::npos);
}

TEST_F(StyleLexerTests, PreprocessorOnePerLine) {
    auto t = strip(lex("#ifdef A\n#endif"));
    // Two PP tokens separated by Newline.
    std::size_t ppCount = 0;
    for (const auto& tok : t) {
        if (tok.kind == TokenKind::Preprocessor) ppCount++;
    }
    EXPECT_EQ(ppCount, 2u);
}

// endregion

// region ---------- Edge cases ----------

TEST_F(StyleLexerTests, LabelKeepsTrailingColon) {
    // FBSciLexer styles `name:` as one Label run including the colon — emit
    // a single Identifier token covering the whole thing. Labels are atomic.
    auto t = strip(lex("myLabel:\n"));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Identifier);
    EXPECT_EQ(t[0].text, "myLabel:");
}

TEST_F(StyleLexerTests, NumberUnderscoreSeparatorNotSupported) {
    // The legacy `analyses/lexer/Lexer` over-permissively accepted `_`
    // inside numeric literals. FBSciLexer does not — `1_000` styles as
    // Error. StyleLexer just reflects what FBSciLexer produces. The editor
    // already shows this as an error today; no behaviour change for users.
    auto t = strip(lex("1_000 "));
    EXPECT_FALSE(t.empty());
    EXPECT_EQ(t[0].kind, TokenKind::Invalid);
}

TEST_F(StyleLexerTests, AsmBlockEndAsm) {
    auto t = strip(lex("asm\nmov eax, 1\nend asm\n"));
    // Expect: Keyword1(asm), KeywordAsm1(mov), KeywordAsm2(eax), Operator(,), Number(1),
    //         Keyword1(end), Keyword1(asm)
    bool sawAsmKeyword1 = false, sawAsmAsm1Mov = false, sawAsmAsm2Eax = false;
    for (const auto& tok : t) {
        if (tok.kind == TokenKind::Keywords && tok.text == "asm") sawAsmKeyword1 = true;
        if (tok.kind == TokenKind::KeywordAsm1 && tok.text == "mov") sawAsmAsm1Mov = true;
        if (tok.kind == TokenKind::KeywordAsm2 && tok.text == "eax") sawAsmAsm2Eax = true;
    }
    EXPECT_TRUE(sawAsmKeyword1);
    EXPECT_TRUE(sawAsmAsm1Mov);
    EXPECT_TRUE(sawAsmAsm2Eax);
}

TEST_F(StyleLexerTests, RemAsComment) {
    auto t = strip(lex("rem hello\n"));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Comment);
}

TEST_F(StyleLexerTests, NestedMultilineComment) {
    auto t = strip(lex("/' a /' b '/ c '/"));
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::CommentBlock);
}

TEST_F(StyleLexerTests, CRLFLineEndings) {
    // \r\n must stay one Newline token, not two.
    auto t = lex("a\r\nb\r\n");
    std::size_t newlineCount = 0;
    for (const auto& tok : t) {
        if (tok.kind == TokenKind::Newline) {
            EXPECT_EQ(tok.text, "\r\n");
            newlineCount++;
        }
    }
    EXPECT_EQ(newlineCount, 2u);
}

// endregion

// region ---------- Verbatim annotation ----------

TEST_F(StyleLexerTests, FormatOffMarksTokensVerbatim) {
    auto t = lex("'format off\nhello\n'format on\n");
    bool sawVerbatim = false;
    for (const auto& tok : t) {
        if (tok.kind == TokenKind::Identifier && tok.text == "hello") {
            sawVerbatim = tok.verbatim;
        }
    }
    EXPECT_TRUE(sawVerbatim);
}

// endregion
