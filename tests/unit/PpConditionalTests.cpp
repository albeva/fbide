//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "TestHelpers.hpp"
#include "analyses/symbols/PpConditional.hpp"

using namespace fbide;

class PpConditionalTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;

    void SetUp() override { m_lexer = tests::createFbLexer(testDataPath + "fbfull.lng"); }
    void TearDown() override {
        m_lexer->Release();
        m_lexer = nullptr;
    }

    auto eval(const char* line, std::unordered_set<std::string> defines) -> PpEval {
        const auto tokens = tests::tokenise(*m_lexer, line);
        return evaluatePpCondition(tokens, defines);
    }

    Scintilla::ILexer5* m_lexer { nullptr };
};

TEST_F(PpConditionalTests, DefinedSymbolIsTrue) {
    EXPECT_EQ(eval("#ifdef __FB_UNIX__\n", { "__fb_unix__" }), PpEval::True);
    EXPECT_EQ(eval("#ifdef Foo\n", { "foo" }), PpEval::True);              // symbols case-insensitive
    EXPECT_EQ(eval("#if __FB_UNIX__\n", { "__fb_unix__" }), PpEval::True); // bare = defined-check
}

TEST_F(PpConditionalTests, UndefinedBuiltinIsFalse) {
    // The __FB_* namespace is fully probed, so an absent one is definitively undefined.
    EXPECT_EQ(eval("#ifdef __FB_WIN32__\n", { "__fb_unix__" }), PpEval::False);
    EXPECT_EQ(eval("#if __FB_WIN32__\n", { "__fb_unix__" }), PpEval::False);
    EXPECT_EQ(eval("#ifndef __FB_WIN32__\n", { "__fb_unix__" }), PpEval::True); // keep the non-win branch
    EXPECT_EQ(eval("#ifndef __FB_UNIX__\n", { "__fb_unix__" }), PpEval::False);
}

TEST_F(PpConditionalTests, UndefinedUserSymbolIsFalse) {
    // A non-builtin symbol absent from the set is undefined — code `#define`s are
    // not tracked, so it resolves false (the -d + built-in define model).
    EXPECT_EQ(eval("#ifdef MY_FLAG\n", {}), PpEval::False);
    EXPECT_EQ(eval("#if defined(MY_FLAG)\n", {}), PpEval::False);
    // An include guard's #ifndef therefore stays true → guarded content kept.
    EXPECT_EQ(eval("#ifndef __FOO_BI__\n", {}), PpEval::True);
    // ...unless the symbol was given on the command line (-d).
    EXPECT_EQ(eval("#ifdef MY_FLAG\n", { "my_flag" }), PpEval::True);
}

TEST_F(PpConditionalTests, BooleanOperators) {
    EXPECT_EQ(eval("#if defined(__FB_UNIX__) or defined(__FB_MACOS__)\n", { "__fb_unix__" }), PpEval::True);
    EXPECT_EQ(eval("#if defined(__FB_WIN32__) and defined(__FB_64BIT__)\n", { "__fb_64bit__" }), PpEval::False);
    EXPECT_EQ(eval("#if defined(__FB_UNIX__) and defined(__FB_64BIT__)\n", { "__fb_unix__", "__fb_64bit__" }), PpEval::True);
    EXPECT_EQ(eval("#if not defined(__FB_WIN32__)\n", { "__fb_unix__" }), PpEval::True);
    EXPECT_EQ(eval("#if defined __FB_UNIX__\n", { "__fb_unix__" }), PpEval::True); // no parens
    EXPECT_EQ(eval("#if (defined(__FB_DARWIN__) or defined(__FB_LINUX__)) and not defined(__FB_WIN32__)\n",
                  { "__fb_darwin__", "__fb_unix__" }),
        PpEval::True);
}

TEST_F(PpConditionalTests, TrueShortCircuitsUnknownOperand) {
    // A definite True via OR wins even when the other operand is a user symbol.
    EXPECT_EQ(eval("#if defined(__FB_UNIX__) or defined(MY_FLAG)\n", { "__fb_unix__" }), PpEval::True);
    // A definite False via AND wins likewise.
    EXPECT_EQ(eval("#if defined(__FB_WIN32__) and defined(MY_FLAG)\n", { "__fb_unix__" }), PpEval::False);
}

TEST_F(PpConditionalTests, ValueChecksAreUnknown) {
    EXPECT_EQ(eval("#if __FB_VERSION__ >= 1\n", { "__fb_version__" }), PpEval::Unknown);
    EXPECT_EQ(eval("#if FOO = 0\n", { "foo" }), PpEval::Unknown);
    EXPECT_EQ(eval("#if 1 + 1\n", {}), PpEval::Unknown); // arithmetic is not evaluated
}

TEST_F(PpConditionalTests, BooleanAndNumericLiterals) {
    EXPECT_EQ(eval("#if 1\n", {}), PpEval::True);
    EXPECT_EQ(eval("#if 0\n", {}), PpEval::False);
    EXPECT_EQ(eval("#if 42\n", {}), PpEval::True);
    EXPECT_EQ(eval("#if true\n", {}), PpEval::True);
    EXPECT_EQ(eval("#if false\n", {}), PpEval::False);
    EXPECT_EQ(eval("#if True\n", {}), PpEval::True);  // case-insensitive
    EXPECT_EQ(eval("#if not 0\n", {}), PpEval::True); // composes with operators
    EXPECT_EQ(eval("#if defined(__FB_UNIX__) and false\n", { "__fb_unix__" }), PpEval::False);
}

TEST_F(PpConditionalTests, NoProbeKeepsBuiltinBranches) {
    // Without a successful compiler probe (no built-in presence macro in the set)
    // the closed-world rule is suspended: an absent __FB_* stays Unknown so we
    // never wrongly drop OS-specific code when fbc is unavailable.
    EXPECT_EQ(eval("#ifdef __FB_WIN32__\n", {}), PpEval::Unknown);
    EXPECT_EQ(eval("#ifdef __FB_WIN32__\n", { "myflag" }), PpEval::Unknown); // -d only, no probe
    EXPECT_EQ(eval("#ifndef __FB_WIN32__\n", {}), PpEval::Unknown);
    // With a probe present, the closed world is back in force.
    EXPECT_EQ(eval("#ifdef __FB_WIN32__\n", { "__fb_unix__" }), PpEval::False);
}
