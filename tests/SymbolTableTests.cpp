//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "TestHelpers.hpp"
#include "../src/lib/analyses/symbols/SymbolTable.hpp"
#include "../src/lib/format/transformers/reformat/ReFormatter.hpp"
#include <gtest/gtest.h>

using namespace fbide;
using namespace fbide::reformat;

class SymbolTableTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;

    void SetUp() override {
        m_lexer = tests::createFbLexer(testDataPath + "fbfull.lng");
    }

    void TearDown() override {
        m_lexer->Release();
        m_lexer = nullptr;
    }

    auto extract(const char* source) -> SymbolTable {
        const auto tokens = tests::tokenise(*m_lexer, source);
        ReFormatter parser({ .lean = true });
        return SymbolTable { parser.buildTree(tokens) };
    }

    Scintilla::ILexer5* m_lexer { nullptr };
};

TEST_F(SymbolTableTests, IncludeBasic) {
    const auto table = extract(
        "#include \"foo.bi\"\n"
        "#include once \"bar.bi\"\n"
        "Sub S\n"
        "End Sub\n"
    );
    ASSERT_EQ(table.getIncludes().size(), 2U);
    EXPECT_EQ(table.getIncludes()[0].path, "foo.bi");
    EXPECT_EQ(table.getIncludes()[0].line, 0);
    EXPECT_EQ(table.getIncludes()[1].path, "bar.bi");
    EXPECT_EQ(table.getIncludes()[1].line, 1);
}

TEST_F(SymbolTableTests, EmptySource) {
    const auto table = extract("");
    EXPECT_TRUE(table.getSubs().empty());
    EXPECT_TRUE(table.getFunctions().empty());
    EXPECT_TRUE(table.getTypes().empty());
    EXPECT_TRUE(table.getUnions().empty());
    EXPECT_TRUE(table.getEnums().empty());
}

TEST_F(SymbolTableTests, SubFunctionType) {
    const auto table = extract(
        "Sub Foo\n"
        "End Sub\n"
        "Function Bar() As Integer\n"
        "End Function\n"
        "Type T\n"
        "    x As Integer\n"
        "End Type\n"
    );
    ASSERT_EQ(table.getSubs().size(), 1U);
    EXPECT_EQ(table.getSubs()[0].name, "Foo");
    EXPECT_EQ(table.getSubs()[0].line, 0);

    ASSERT_EQ(table.getFunctions().size(), 1U);
    EXPECT_EQ(table.getFunctions()[0].name, "Bar");
    EXPECT_EQ(table.getFunctions()[0].line, 2);

    ASSERT_EQ(table.getTypes().size(), 1U);
    EXPECT_EQ(table.getTypes()[0].name, "T");
    EXPECT_EQ(table.getTypes()[0].line, 4);
}

TEST_F(SymbolTableTests, EnumAndUnion) {
    const auto table = extract(
        "Enum Color\n"
        "    Red\n"
        "    Green\n"
        "End Enum\n"
        "Union U\n"
        "    a As Integer\n"
        "    b As Single\n"
        "End Union\n"
    );
    ASSERT_EQ(table.getEnums().size(), 1U);
    EXPECT_EQ(table.getEnums()[0].name, "Color");
    EXPECT_EQ(table.getEnums()[0].line, 0);

    ASSERT_EQ(table.getUnions().size(), 1U);
    EXPECT_EQ(table.getUnions()[0].name, "U");
    EXPECT_EQ(table.getUnions()[0].line, 4);
}

TEST_F(SymbolTableTests, NamespaceFlattens) {
    const auto table = extract(
        "Namespace Outer\n"
        "    Sub A\n"
        "    End Sub\n"
        "    Type T\n"
        "        x As Integer\n"
        "    End Type\n"
        "End Namespace\n"
    );
    ASSERT_EQ(table.getSubs().size(), 1U);
    EXPECT_EQ(table.getSubs()[0].name, "A");
    ASSERT_EQ(table.getTypes().size(), 1U);
    EXPECT_EQ(table.getTypes()[0].name, "T");
}

TEST_F(SymbolTableTests, NestedNamespacesRecurse) {
    const auto table = extract(
        "Namespace Outer\n"
        "    Namespace Inner\n"
        "        Sub Deep\n"
        "        End Sub\n"
        "    End Namespace\n"
        "End Namespace\n"
    );
    ASSERT_EQ(table.getSubs().size(), 1U);
    EXPECT_EQ(table.getSubs()[0].name, "Deep");
}

TEST_F(SymbolTableTests, DeclareSubIsNotCaptured) {
    const auto table = extract("Declare Sub Foo()\n");
    EXPECT_TRUE(table.getSubs().empty());
    EXPECT_TRUE(table.getFunctions().empty());
}

TEST_F(SymbolTableTests, TypeAliasIsNotCaptured) {
    const auto table = extract("Type AliasName As Integer\n");
    EXPECT_TRUE(table.getTypes().empty());
}

TEST_F(SymbolTableTests, ControlFlowBlocksIgnored) {
    const auto table = extract(
        "If x = 1 Then\n"
        "    Print x\n"
        "End If\n"
        "For i = 1 To 10\n"
        "Next\n"
    );
    EXPECT_TRUE(table.getSubs().empty());
    EXPECT_TRUE(table.getFunctions().empty());
    EXPECT_TRUE(table.getTypes().empty());
}

TEST_F(SymbolTableTests, AnonymousTypeSkipped) {
    // Anonymous Type / Union (no name token) — walker drops it.
    const auto table = extract(
        "Type\n"
        "    x As Integer\n"
        "End Type\n"
    );
    EXPECT_TRUE(table.getTypes().empty());
}

TEST_F(SymbolTableTests, HashChangesOnContent) {
    const auto a = extract(
        "Sub Foo\n"
        "End Sub\n"
    );
    const auto b = extract(
        "Sub Bar\n"
        "End Sub\n"
    );
    EXPECT_NE(a.getHash(), b.getHash());
}

TEST_F(SymbolTableTests, HashStableForSameInput) {
    const auto a = extract(
        "Sub Foo\n"
        "End Sub\n"
        "Function Bar() As Integer\n"
        "End Function\n"
    );
    const auto b = extract(
        "Sub Foo\n"
        "End Sub\n"
        "Function Bar() As Integer\n"
        "End Function\n"
    );
    EXPECT_EQ(a.getHash(), b.getHash());
}

TEST_F(SymbolTableTests, HashStableOnLineShift) {
    // Same symbol set, just shifted to a different line — hash is invariant
    // because it covers (kind, name) only, not line numbers. This lets the
    // sub/function browser skip rebuilds when typing whitespace.
    const auto a = extract(
        "Sub Foo\n"
        "End Sub\n"
    );
    const auto b = extract(
        "\n"
        "Sub Foo\n"
        "End Sub\n"
    );
    EXPECT_EQ(a.getHash(), b.getHash());
}
