//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "TestHelpers.hpp"
#include "format/FormatCheck.hpp"
#include "format/transformers/case/CaseTransform.hpp"
#include "format/transformers/reformat/ReFormatter.hpp"

using namespace fbide;

// ---------------------------------------------------------------------------
// isSafeFormatResult — rejects anything that is not a pure layout/case change
// ---------------------------------------------------------------------------

TEST(FormatCheckTests, RejectsEmptyResultForNonEmptySource) {
    EXPECT_FALSE(isSafeFormatResult("Dim x As Integer\n", ""));
}

TEST(FormatCheckTests, AcceptsEmptyResultForBlankSource) {
    EXPECT_TRUE(isSafeFormatResult("", ""));
    EXPECT_TRUE(isSafeFormatResult("  \n\t\n", ""));
}

TEST(FormatCheckTests, RejectsInvalidUtf8) {
    // A multi-byte character split by an inserted space.
    EXPECT_FALSE(isSafeFormatResult("x = \xE4\xB8\xAD\n", "x = \xE4 \xB8\xAD\n"));
}

TEST(FormatCheckTests, RejectsDroppedCharacter) {
    EXPECT_FALSE(isSafeFormatResult("#define A (a.b)\n", "#define A (ab)\n"));
}

TEST(FormatCheckTests, RejectsChangedIdentifier) {
    EXPECT_FALSE(isSafeFormatResult("x = 1\n", "y = 1\n"));
    EXPECT_FALSE(isSafeFormatResult("x = 1\n", "x = 1\nz\n"));
}

TEST(FormatCheckTests, AcceptsLayoutChanges) {
    EXPECT_TRUE(isSafeFormatResult("Sub Main\nPrint  x\nEnd Sub\n", "Sub Main\n    Print x\nEnd Sub\n"));
    EXPECT_TRUE(isSafeFormatResult("x=1\r\n", "x = 1\n"));
}

TEST(FormatCheckTests, AcceptsAsciiCaseChanges) {
    EXPECT_TRUE(isSafeFormatResult("dim x as integer\n", "DIM x AS INTEGER\n"));
}

TEST(FormatCheckTests, AcceptsColonSeparatorsRemovedBySplitting) {
    EXPECT_TRUE(isSafeFormatResult("If x Then : Print x : End If\n", "If x Then\n    Print x\nEnd If\n"));
    // A colon the output keeps must still match.
    EXPECT_TRUE(isSafeFormatResult("Public:\n", "Public:\n"));
}

TEST(FormatCheckTests, RejectsAddedColon) {
    EXPECT_FALSE(isSafeFormatResult("x = 1\n", "x = 1 :\n"));
}

// ---------------------------------------------------------------------------
// The real transform chain must pass the check — no false "Formatting failed"
// ---------------------------------------------------------------------------

class FormatCheckChainTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;

    void SetUp() override {
        m_lexer = tests::createFbLexer(testDataPath + "resources/ide/keywords.ini");
    }

    void TearDown() override {
        m_lexer->Release();
        m_lexer = nullptr;
    }

    /// Run case conversion + reformat with every option on, return the text.
    auto formatAll(const std::string& source, const bool anchoredPP) -> std::string {
        const auto tokens = tests::tokenise(*m_lexer, source);
        std::array<CaseMode, kThemeKeywordGroupsCount> cases {};
        cases.fill(CaseMode::Upper);
        const auto cased = CaseTransform { cases }.apply(tokens);
        reformat::ReFormatter formatter({ .tabSize = 4, .anchoredPP = anchoredPP });
        std::string out;
        for (const auto& tok : formatter.apply(cased)) {
            out += tok.text;
        }
        return out;
    }

    Scintilla::ILexer5* m_lexer { nullptr };
};

TEST_F(FormatCheckChainTests, SampleProgramPassesCheck) {
    wxFFile file(testDataPath + "flappyfb.bas", "rb");
    ASSERT_TRUE(file.IsOpened());
    std::string raw(static_cast<std::size_t>(file.Length()), '\0');
    ASSERT_EQ(file.Read(raw.data(), raw.size()), raw.size());
    // The sample is Latin-1; the editor formats UTF-8 text, so decode first.
    const std::string source { wxString(raw.data(), wxConvISO8859_1, raw.size()).utf8_str().data() };
    ASSERT_FALSE(source.empty());
    for (const bool anchored : { false, true }) {
        const auto formatted = formatAll(source, anchored);
        EXPECT_FALSE(formatted.empty());
        EXPECT_TRUE(isSafeFormatResult(source, formatted)) << "anchoredPP=" << anchored;
    }
}

TEST_F(FormatCheckChainTests, ForumSnippetsPassCheck) {
    for (const char* source : {
             "Dim \xE5\x8F\x98\xE9\x87\x8F As Integer\n",
             "#define X \xE4\xB8\xAD\n",
             "x = 1 \xEF\xBC\x9A y = 2\n",
             "x\xE5\x8F\x98\xE9\x87\x8F = 1\n",
             "a &=> \"-\"\n",
             "If x Then : Print x : End If\n",
             "If t2 < t1 Then t1 -= 1 : t3 -= 1\n",
             "#Define LEFT1 (sprite1.x)\n",
         }) {
        EXPECT_TRUE(isSafeFormatResult(source, formatAll(source, false))) << source;
    }
}
