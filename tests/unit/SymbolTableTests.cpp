//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "analyses/parser/TreeParser.hpp"
#include "analyses/symbols/SymbolTable.hpp"
#include "TestHelpers.hpp"

using namespace fbide;
using namespace fbide::parser;

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
        TreeParser parser({ .lean = true });
        return SymbolTable { parser.parse(tokens) };
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
    EXPECT_TRUE(table.getMacros().empty());
}

TEST_F(SymbolTableTests, MacroBasic) {
    const auto table = extract(
        "#macro DOUBLE(x)\n"
        "    (x) * 2\n"
        "#endmacro\n"
        "#macro NO_ARGS\n"
        "    Print \"hi\"\n"
        "#endmacro\n"
    );
    ASSERT_EQ(table.getMacros().size(), 2U);
    EXPECT_EQ(table.getMacros()[0].name, "DOUBLE");
    EXPECT_EQ(table.getMacros()[0].line, 0);
    EXPECT_EQ(table.getMacros()[1].name, "NO_ARGS");
    EXPECT_EQ(table.getMacros()[1].line, 3);
}

TEST_F(SymbolTableTests, MacroAlongsideSubs) {
    const auto table = extract(
        "#macro M(a)\n"
        "    a + a\n"
        "#endmacro\n"
        "Sub Foo\n"
        "End Sub\n"
    );
    ASSERT_EQ(table.getMacros().size(), 1U);
    EXPECT_EQ(table.getMacros()[0].name, "M");
    ASSERT_EQ(table.getSubs().size(), 1U);
    EXPECT_EQ(table.getSubs()[0].name, "Foo");
}

TEST_F(SymbolTableTests, AnonymousMacroSkipped) {
    // `#macro` with no identifier is invalid input — the walker drops it.
    const auto table = extract(
        "#macro\n"
        "#endmacro\n"
    );
    EXPECT_TRUE(table.getMacros().empty());
}

TEST_F(SymbolTableTests, MacroParticipatesInHash) {
    const auto a = extract(
        "#macro A\n"
        "#endmacro\n"
    );
    const auto b = extract(
        "#macro B\n"
        "#endmacro\n"
    );
    EXPECT_NE(a.getHash(), b.getHash());
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

TEST_F(SymbolTableTests, AccessModifiersOnSubFunction) {
    // `Private` / `Public` / `Protected` are transparent prefixes — the
    // declaration is still captured under its real kind and name.
    const auto table = extract(
        "Private Sub Foo\n"
        "End Sub\n"
        "Public Function Bar() As Integer\n"
        "End Function\n"
        "Protected Sub Baz\n"
        "End Sub\n"
    );
    ASSERT_EQ(table.getSubs().size(), 2U);
    EXPECT_EQ(table.getSubs()[0].name, "Foo");
    EXPECT_EQ(table.getSubs()[0].line, 0);
    EXPECT_EQ(table.getSubs()[1].name, "Baz");
    EXPECT_EQ(table.getSubs()[1].line, 4);

    ASSERT_EQ(table.getFunctions().size(), 1U);
    EXPECT_EQ(table.getFunctions()[0].name, "Bar");
    EXPECT_EQ(table.getFunctions()[0].line, 2);
}

TEST_F(SymbolTableTests, AccessModifierOnType) {
    const auto table = extract(
        "Public Type T\n"
        "    x As Integer\n"
        "End Type\n"
    );
    ASSERT_EQ(table.getTypes().size(), 1U);
    EXPECT_EQ(table.getTypes()[0].name, "T");
}

TEST_F(SymbolTableTests, MethodKeepsQualifiedName) {
    // FB OO syntax — `Sub TypeName.MethodName` defines a method body. The
    // browser shows the fully qualified name.
    const auto table = extract(
        "Sub Vec.Reset\n"
        "End Sub\n"
        "Function Vec.Length() As Double\n"
        "End Function\n"
    );
    ASSERT_EQ(table.getSubs().size(), 1U);
    EXPECT_EQ(table.getSubs()[0].name, "Vec.Reset");

    ASSERT_EQ(table.getFunctions().size(), 1U);
    EXPECT_EQ(table.getFunctions()[0].name, "Vec.Length");
}

TEST_F(SymbolTableTests, MethodWithAccessModifier) {
    const auto table = extract(
        "Private Sub Vec.Hide\n"
        "End Sub\n"
    );
    ASSERT_EQ(table.getSubs().size(), 1U);
    EXPECT_EQ(table.getSubs()[0].name, "Vec.Hide");
}

TEST_F(SymbolTableTests, ConstructorAndDestructor) {
    const auto table = extract(
        "Constructor Vec()\n"
        "End Constructor\n"
        "Destructor Vec()\n"
        "End Destructor\n"
    );
    ASSERT_EQ(table.getConstructors().size(), 1U);
    EXPECT_EQ(table.getConstructors()[0].name, "Vec");
    EXPECT_EQ(table.getConstructors()[0].line, 0);

    ASSERT_EQ(table.getDestructors().size(), 1U);
    EXPECT_EQ(table.getDestructors()[0].name, "Vec");
    EXPECT_EQ(table.getDestructors()[0].line, 2);
}

TEST_F(SymbolTableTests, MemberOperator) {
    // UDT member operators carry the `TypeName.` qualifier and the operator
    // token(s) — symbolic, keyword, or bracketed.
    const auto table = extract(
        "Operator Vec.+ (rhs As Vec) As Vec\n"
        "End Operator\n"
        "Operator Vec.Cast() As String\n"
        "End Operator\n"
        "Operator Vec.[] (i As Integer) As Integer\n"
        "End Operator\n"
    );
    ASSERT_EQ(table.getOperators().size(), 3U);
    EXPECT_EQ(table.getOperators()[0].name, "Vec.+");
    EXPECT_EQ(table.getOperators()[1].name, "Vec.Cast");
    EXPECT_EQ(table.getOperators()[2].name, "Vec.[]");
}

TEST_F(SymbolTableTests, FreeStandingOperator) {
    const auto table = extract(
        "Operator + (lhs As Vec, rhs As Vec) As Vec\n"
        "End Operator\n"
    );
    ASSERT_EQ(table.getOperators().size(), 1U);
    EXPECT_EQ(table.getOperators()[0].name, "+");
}

TEST_F(SymbolTableTests, Property) {
    const auto table = extract(
        "Property Vec.X() As Integer\n"
        "End Property\n"
        "Property Vec.X(value As Integer)\n"
        "End Property\n"
    );
    ASSERT_EQ(table.getProperties().size(), 2U);
    EXPECT_EQ(table.getProperties()[0].name, "Vec.X");
    EXPECT_EQ(table.getProperties()[1].name, "Vec.X");
}

TEST_F(SymbolTableTests, OperatorNameForms) {
    // Operators come in many shapes — keyword names (`Cast`, `Let`, `New`,
    // `Delete`), bracketed allocation forms (`New[]`, `Delete[]`), sigils
    // (`@`), and compound-assignment symbols (`+=`).
    const auto table = extract(
        "Operator Vec.Let (rhs As Vec)\n"
        "End Operator\n"
        "Operator Vec.+= (rhs As Vec)\n"
        "End Operator\n"
        "Operator Vec.@ () As Integer Ptr\n"
        "End Operator\n"
        "Operator Vec.New (size As UInteger) As Any Ptr\n"
        "End Operator\n"
        "Operator Vec.New[] (size As UInteger) As Any Ptr\n"
        "End Operator\n"
        "Operator Vec.Delete (buf As Any Ptr)\n"
        "End Operator\n"
        "Operator Vec.Delete[] (buf As Any Ptr)\n"
        "End Operator\n"
    );
    ASSERT_EQ(table.getOperators().size(), 7U);
    EXPECT_EQ(table.getOperators()[0].name, "Vec.Let");
    EXPECT_EQ(table.getOperators()[1].name, "Vec.+=");
    EXPECT_EQ(table.getOperators()[2].name, "Vec.@");
    EXPECT_EQ(table.getOperators()[3].name, "Vec.New");
    EXPECT_EQ(table.getOperators()[4].name, "Vec.New[]");
    EXPECT_EQ(table.getOperators()[5].name, "Vec.Delete");
    EXPECT_EQ(table.getOperators()[6].name, "Vec.Delete[]");
}

TEST_F(SymbolTableTests, AccessModifiersOnAllCallables) {
    // `Private` / `Public` / `Protected` are transparent prefixes on every
    // callable kind, not just Sub / Function.
    const auto table = extract(
        "Public Constructor Vec()\n"
        "End Constructor\n"
        "Private Destructor Vec()\n"
        "End Destructor\n"
        "Protected Operator Vec.+ (rhs As Vec) As Vec\n"
        "End Operator\n"
        "Public Property Vec.X() As Integer\n"
        "End Property\n"
    );
    ASSERT_EQ(table.getConstructors().size(), 1U);
    EXPECT_EQ(table.getConstructors()[0].name, "Vec");
    ASSERT_EQ(table.getDestructors().size(), 1U);
    EXPECT_EQ(table.getDestructors()[0].name, "Vec");
    ASSERT_EQ(table.getOperators().size(), 1U);
    EXPECT_EQ(table.getOperators()[0].name, "Vec.+");
    ASSERT_EQ(table.getProperties().size(), 1U);
    EXPECT_EQ(table.getProperties()[0].name, "Vec.X");
}

TEST_F(SymbolTableTests, CallablesInsideNamespaceRecurse) {
    // Every callable kind is found when nested inside a Namespace body.
    const auto table = extract(
        "Namespace Geom\n"
        "    Constructor Vec()\n"
        "    End Constructor\n"
        "    Destructor Vec()\n"
        "    End Destructor\n"
        "    Operator Vec.+ (rhs As Vec) As Vec\n"
        "    End Operator\n"
        "    Property Vec.X() As Integer\n"
        "    End Property\n"
        "End Namespace\n"
    );
    ASSERT_EQ(table.getConstructors().size(), 1U);
    EXPECT_EQ(table.getConstructors()[0].name, "Vec");
    ASSERT_EQ(table.getDestructors().size(), 1U);
    EXPECT_EQ(table.getDestructors()[0].name, "Vec");
    ASSERT_EQ(table.getOperators().size(), 1U);
    EXPECT_EQ(table.getOperators()[0].name, "Vec.+");
    ASSERT_EQ(table.getProperties().size(), 1U);
    EXPECT_EQ(table.getProperties()[0].name, "Vec.X");
}

TEST_F(SymbolTableTests, AllSymbolKindsTogether) {
    // One source exercising every captured kind — guards against a kind being
    // dropped by the walker dispatch.
    const auto table = extract(
        "#include \"foo.bi\"\n"
        "#macro M(a)\n"
        "    a\n"
        "#endmacro\n"
        "Sub S\n"
        "End Sub\n"
        "Function F() As Integer\n"
        "End Function\n"
        "Type C\n"
        "    x As Integer\n"
        "End Type\n"
        "Constructor C()\n"
        "End Constructor\n"
        "Destructor C()\n"
        "End Destructor\n"
        "Operator C.+ (rhs As C) As C\n"
        "End Operator\n"
        "Property C.P() As Integer\n"
        "End Property\n"
        "Type T\n"
        "    x As Integer\n"
        "End Type\n"
        "Union U\n"
        "    a As Integer\n"
        "End Union\n"
        "Enum E\n"
        "    Red\n"
        "End Enum\n"
    );
    EXPECT_EQ(table.getSubs().size(), 1U);
    EXPECT_EQ(table.getFunctions().size(), 1U);
    EXPECT_EQ(table.getConstructors().size(), 1U);
    EXPECT_EQ(table.getDestructors().size(), 1U);
    EXPECT_EQ(table.getOperators().size(), 1U);
    EXPECT_EQ(table.getProperties().size(), 1U);
    EXPECT_EQ(table.getTypes().size(), 2U); // C and T, both declared
    EXPECT_EQ(table.getUnions().size(), 1U);
    EXPECT_EQ(table.getEnums().size(), 1U);
    EXPECT_EQ(table.getMacros().size(), 1U);
    EXPECT_EQ(table.getIncludes().size(), 1U);
}

TEST_F(SymbolTableTests, NewKindsParticipateInHash) {
    // Hash covers (kind, name) for the OO kinds too — renaming a constructor
    // changes the hash so the browser rebuilds.
    const auto a = extract(
        "Constructor Foo()\n"
        "End Constructor\n"
    );
    const auto b = extract(
        "Constructor Bar()\n"
        "End Constructor\n"
    );
    EXPECT_NE(a.getHash(), b.getHash());
}

TEST_F(SymbolTableTests, UndeclaredMethodOwnerSynthesisesType) {
    // `Sub Vec.Foo` with no `Type Vec` — a synthetic group-only Type is added
    // so the browser can nest the method. Synthetic types carry a negative
    // line and so are not navigable.
    const auto table = extract(
        "Sub Vec.Foo\n"
        "End Sub\n"
    );
    ASSERT_EQ(table.getTypes().size(), 1U);
    EXPECT_EQ(table.getTypes()[0].name, "Vec");
    EXPECT_LT(table.getTypes()[0].line, 0);
    EXPECT_EQ(symbolOwner(table.getSubs()[0]), "Vec");
}

TEST_F(SymbolTableTests, ConstructorSynthesisesOwnerType) {
    const auto table = extract(
        "Constructor Vec()\n"
        "End Constructor\n"
    );
    ASSERT_EQ(table.getTypes().size(), 1U);
    EXPECT_EQ(table.getTypes()[0].name, "Vec");
    EXPECT_LT(table.getTypes()[0].line, 0);
}

TEST_F(SymbolTableTests, DeclaredOwnerHasNoSyntheticDuplicate) {
    // The owner is already a declared type — no synthetic entry, and the
    // declared one keeps its real (non-negative) line.
    const auto table = extract(
        "Type Vec\n"
        "    x As Integer\n"
        "End Type\n"
        "Sub Vec.Foo\n"
        "End Sub\n"
        "Constructor Vec()\n"
        "End Constructor\n"
    );
    ASSERT_EQ(table.getTypes().size(), 1U);
    EXPECT_EQ(table.getTypes()[0].name, "Vec");
    EXPECT_GE(table.getTypes()[0].line, 0);
}

TEST_F(SymbolTableTests, ManyMembersShareOneSyntheticType) {
    // Several members of the same undeclared owner collapse to one Type entry.
    const auto table = extract(
        "Sub Vec.A\n"
        "End Sub\n"
        "Function Vec.B() As Integer\n"
        "End Function\n"
        "Constructor Vec()\n"
        "End Constructor\n"
        "Property Vec.C() As Integer\n"
        "End Property\n"
    );
    ASSERT_EQ(table.getTypes().size(), 1U);
    EXPECT_EQ(table.getTypes()[0].name, "Vec");
}

TEST_F(SymbolTableTests, FreeStandingSymbolsDoNotSynthesiseTypes) {
    const auto table = extract(
        "Sub Foo\n"
        "End Sub\n"
        "Function Bar() As Integer\n"
        "End Function\n"
        "Operator + (lhs As Integer, rhs As Integer) As Integer\n"
        "End Operator\n"
    );
    EXPECT_TRUE(table.getTypes().empty());
    EXPECT_TRUE(symbolOwner(table.getSubs()[0]).empty());
    EXPECT_TRUE(symbolOwner(table.getOperators()[0]).empty());
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

// ---------------------------------------------------------------------------
// Preprocessor conditional blocks — symbols guarded by `#if` / `#ifdef` /
// `#ifndef` are collected (the parser does not evaluate conditions, so both
// `#if` and `#else` branches contribute). `#macro` bodies are not recursed.
// ---------------------------------------------------------------------------

TEST_F(SymbolTableTests, SymbolsInsidePpIfCaptured) {
    const auto table = extract(
        "#if defined(FOO)\n"
        "    Sub Guarded\n"
        "    End Sub\n"
        "    Type Gizmo\n"
        "        x As Integer\n"
        "    End Type\n"
        "#endif\n"
    );
    ASSERT_EQ(table.getSubs().size(), 1U);
    EXPECT_EQ(table.getSubs()[0].name, "Guarded");
    ASSERT_EQ(table.getTypes().size(), 1U);
    EXPECT_EQ(table.getTypes()[0].name, "Gizmo");
}

TEST_F(SymbolTableTests, SymbolsInPpIfdefAndIfndef) {
    const auto table = extract(
        "#ifdef FOO\n"
        "    Sub A\n"
        "    End Sub\n"
        "#endif\n"
        "#ifndef BAR\n"
        "    Sub B\n"
        "    End Sub\n"
        "#endif\n"
    );
    ASSERT_EQ(table.getSubs().size(), 2U);
    EXPECT_EQ(table.getSubs()[0].name, "A");
    EXPECT_EQ(table.getSubs()[1].name, "B");
}

TEST_F(SymbolTableTests, PpIfElseCollectsBothBranches) {
    // Conditions are not evaluated — symbols from every branch are listed.
    const auto table = extract(
        "#if defined(FOO)\n"
        "    Sub WhenFoo\n"
        "    End Sub\n"
        "#else\n"
        "    Sub WhenNotFoo\n"
        "    End Sub\n"
        "#endif\n"
    );
    ASSERT_EQ(table.getSubs().size(), 2U);
    EXPECT_EQ(table.getSubs()[0].name, "WhenFoo");
    EXPECT_EQ(table.getSubs()[1].name, "WhenNotFoo");
}

TEST_F(SymbolTableTests, NestedPpIfRecurses) {
    const auto table = extract(
        "#if defined(FOO)\n"
        "    #if defined(BAR)\n"
        "        Sub Deep\n"
        "        End Sub\n"
        "    #endif\n"
        "#endif\n"
    );
    ASSERT_EQ(table.getSubs().size(), 1U);
    EXPECT_EQ(table.getSubs()[0].name, "Deep");
}

TEST_F(SymbolTableTests, PpMacroBodyNotRecursed) {
    // A `#macro` body is a template, not a scope — declarations inside it
    // are not real symbols. Only the macro name itself is captured.
    const auto table = extract(
        "#macro DEFINE_SUB\n"
        "    Sub Generated\n"
        "    End Sub\n"
        "#endmacro\n"
    );
    EXPECT_TRUE(table.getSubs().empty());
    ASSERT_EQ(table.getMacros().size(), 1U);
    EXPECT_EQ(table.getMacros()[0].name, "DEFINE_SUB");
}

TEST_F(SymbolTableTests, RetainsScopeTreeWithParents) {
    const auto table = extract(
        "Sub Foo\n"
        "For i = 1 To 10\n"
        "Next\n"
        "End Sub\n");
    ASSERT_EQ(table.tree().nodes.size(), 1u);
    const auto* sub = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ((*sub)->parent, nullptr);
    ASSERT_TRUE((*sub)->opener.has_value());
    EXPECT_GE((*sub)->opener->tokens[0].pos, 0); // tokens carry byte offsets
}

TEST_F(SymbolTableTests, TakeTreeEmptiesRetainedTree) {
    SymbolTable table = extract("Sub Foo\nEnd Sub\n");
    ASSERT_EQ(table.tree().nodes.size(), 1u);
    const auto taken = table.takeTree();
    EXPECT_EQ(taken.nodes.size(), 1u);
    EXPECT_TRUE(table.tree().nodes.empty()); // moved-out leaves it empty
}

TEST_F(SymbolTableTests, BlockAtFindsInnermostScope) {
    const auto table = extract(
        "Sub Foo\n"
        "For i = 1 To 10\n"
        "Print i\n"
        "Next\n"
        "End Sub\n");
    ASSERT_EQ(table.tree().nodes.size(), 1u);
    const auto* sub = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(sub, nullptr);

    const BlockNode* forBlock = nullptr;
    for (const auto& child : (*sub)->body) {
        if (const auto* nested = std::get_if<std::unique_ptr<BlockNode>>(&child)) {
            forBlock = nested->get();
        }
    }
    ASSERT_NE(forBlock, nullptr);
    ASSERT_TRUE((*sub)->closer.has_value());
    ASSERT_TRUE(forBlock->closer.has_value());

    // On the Sub opener keyword -> the Sub block.
    EXPECT_EQ(table.blockAt((*sub)->opener->tokens[0].pos), sub->get());
    // On the For opener keyword -> the innermost (For), not the enclosing Sub.
    EXPECT_EQ(table.blockAt(forBlock->opener->tokens[0].pos), forBlock);
    // On the For closer (Next) -> still the For block.
    EXPECT_EQ(table.blockAt(forBlock->closer->tokens[0].pos), forBlock);
    // On the Sub closer (End Sub) -> the Sub block (For has ended by now).
    EXPECT_EQ(table.blockAt((*sub)->closer->tokens[0].pos), sub->get());
    // Past the end of the document -> no scope.
    EXPECT_EQ(table.blockAt(100000), nullptr);
}

TEST_F(SymbolTableTests, BlockAtReturnsNullBetweenTopLevelBlocks) {
    const auto table = extract(
        "Sub A\n"
        "End Sub\n"
        "\n"
        "Sub B\n"
        "End Sub\n");
    ASSERT_EQ(table.tree().nodes.size(), 2u);
    const auto* subA = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    const auto* subB = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[1]);
    ASSERT_NE(subA, nullptr);
    ASSERT_NE(subB, nullptr);
    ASSERT_TRUE((*subA)->closer.has_value());

    // Just past "End Sub" of A, before "Sub B" -> in no block.
    const auto& endA = (*subA)->closer->tokens.back();
    const int gap = endA.pos + static_cast<int>(endA.text.size());
    EXPECT_EQ(table.blockAt(gap), nullptr);
    // Inside B resolves to B.
    EXPECT_EQ(table.blockAt((*subB)->opener->tokens[0].pos), subB->get());
}

TEST_F(SymbolTableTests, MatchBlockForNext) {
    const auto table = extract(
        "For i = 1 To 10\n"
        "Next\n");
    ASSERT_EQ(table.tree().nodes.size(), 1u);
    const auto* forBlk = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(forBlk, nullptr);
    const int forPos = (*forBlk)->opener->tokens[0].pos;
    const int nextPos = (*forBlk)->closer->tokens[0].pos;

    const auto onFor = table.matchBlockAt(forPos);
    ASSERT_EQ(onFor.size(), 2u);
    EXPECT_EQ(onFor[0].first, forPos);
    EXPECT_EQ(onFor[1].first, nextPos);

    const auto onNext = table.matchBlockAt(nextPos);
    ASSERT_EQ(onNext.size(), 2u); // same pair, reached from the closer
    EXPECT_EQ(onNext[0].first, forPos);
    EXPECT_EQ(onNext[1].first, nextPos);
}

TEST_F(SymbolTableTests, MatchBlockSubEndSubSpansBothCloserTokens) {
    const auto table = extract("Sub Foo\nEnd Sub\n");
    const auto* sub = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(sub, nullptr);
    const int subPos = (*sub)->opener->tokens[0].pos;
    const auto spans = table.matchBlockAt(subPos);
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0].first, subPos);
    // closer span covers the whole "End Sub" (End .. Sub end).
    const auto& closer = (*sub)->closer->tokens;
    EXPECT_EQ(spans[1].first, closer.front().pos);
    EXPECT_EQ(spans[1].second, closer.back().pos + static_cast<int>(closer.back().text.size()));
}

TEST_F(SymbolTableTests, MatchBlockEmptyOffKeyword) {
    const auto table = extract("Sub Foo\nPrint 1\nEnd Sub\n");
    const auto* sub = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(sub, nullptr);
    int printPos = -1;
    for (const auto& child : (*sub)->body) {
        if (const auto* st = std::get_if<StatementNode>(&child)) {
            printPos = st->tokens[0].pos;
            break;
        }
    }
    ASSERT_GE(printPos, 0);
    EXPECT_TRUE(table.matchBlockAt(printPos).empty()); // on a statement, not a block keyword
}

TEST_F(SymbolTableTests, MatchProcedureFromReturn) {
    const auto table = extract(
        "Function F() As Integer\n"
        "Return 1\n"
        "End Function\n");
    const auto* fn = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(fn, nullptr);
    int returnPos = -1;
    for (const auto& child : (*fn)->body) {
        if (const auto* st = std::get_if<StatementNode>(&child)) {
            returnPos = st->tokens[0].pos;
            break;
        }
    }
    ASSERT_GE(returnPos, 0);
    const auto spans = table.matchProcedureAt(returnPos);
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0].first, (*fn)->opener->tokens[0].pos);          // Function
    EXPECT_EQ(spans[1].first, (*fn)->closer->tokens.front().pos);     // End Function
}

TEST_F(SymbolTableTests, MatchProcedureFromReturnInsideNestedBlock) {
    const auto table = extract(
        "Sub S\n"
        "For i = 1 To 2\n"
        "Return\n"
        "Next\n"
        "End Sub\n");
    const auto* sub = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(sub, nullptr);
    // find the Return statement nested inside the For
    int returnPos = -1;
    for (const auto& child : (*sub)->body) {
        if (const auto* forBlk = std::get_if<std::unique_ptr<BlockNode>>(&child)) {
            for (const auto& inner : (*forBlk)->body) {
                if (const auto* st = std::get_if<StatementNode>(&inner)) {
                    returnPos = st->tokens[0].pos;
                    break;
                }
            }
        }
    }
    ASSERT_GE(returnPos, 0);
    const auto spans = table.matchProcedureAt(returnPos);
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0].first, (*sub)->opener->tokens[0].pos); // walks past the For to the Sub
}

TEST_F(SymbolTableTests, MatchSelectGroupFromCase) {
    const auto table = extract(
        "Select Case x\n"
        "Case 1\n"
        "Print 1\n"
        "Case 2\n"
        "Print 2\n"
        "End Select\n");
    ASSERT_EQ(table.tree().nodes.size(), 1u);
    const auto* sel = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(sel, nullptr);
    std::vector<const BlockNode*> cases;
    for (const auto& child : (*sel)->body) {
        if (const auto* b = std::get_if<std::unique_ptr<BlockNode>>(&child)) {
            cases.push_back(b->get());
        }
    }
    ASSERT_EQ(cases.size(), 2u);
    const int casePos = cases[0]->opener->tokens[0].pos;

    const auto spans = table.matchBlockAt(casePos);
    // On a single Case: Select header + that one Case + End Select.
    ASSERT_EQ(spans.size(), 3u);
    EXPECT_EQ(spans.front().first, (*sel)->opener->tokens[0].pos);    // Select Case
    EXPECT_EQ(spans[1].first, cases[0]->opener->tokens[0].pos);       // only Case 1
    EXPECT_EQ(spans.back().first, (*sel)->closer->tokens.front().pos); // End Select
    for (const auto& span : spans) {
        EXPECT_NE(span.first, cases[1]->opener->tokens[0].pos);       // Case 2 excluded
    }
}

TEST_F(SymbolTableTests, MatchSelectGroupFromSelectHighlightsAllCases) {
    const auto table = extract(
        "Select Case x\n"
        "Case 1\n"
        "Case 2\n"
        "End Select\n");
    const auto* sel = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(sel, nullptr);
    const auto spans = table.matchBlockAt((*sel)->opener->tokens[0].pos);
    EXPECT_EQ(spans.size(), 4u); // Select + Case 1 + Case 2 + End Select
}

TEST_F(SymbolTableTests, MatchSelectGroupSameFromSelectAndEnd) {
    const auto table = extract(
        "Select Case x\n"
        "Case 1\n"
        "End Select\n");
    const auto* sel = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(sel, nullptr);
    const int selectPos = (*sel)->opener->tokens[0].pos;
    const int endPos = (*sel)->closer->tokens.front().pos;

    const auto fromSelect = table.matchBlockAt(selectPos);
    const auto fromEnd = table.matchBlockAt(endPos);
    ASSERT_EQ(fromSelect.size(), 3u);
    EXPECT_EQ(fromSelect, fromEnd);

    const auto& op = (*sel)->opener->tokens;
    EXPECT_EQ(fromSelect.front().first, op[0].pos);
    EXPECT_EQ(fromSelect.front().second, op[1].pos + static_cast<int>(op[1].text.size()));
}

TEST_F(SymbolTableTests, MatchIfGroupFromIfAndEnd) {
    const auto table = extract(
        "If a Then\n"
        "Print 1\n"
        "ElseIf b Then\n"
        "Print 2\n"
        "Else\n"
        "Print 3\n"
        "End If\n");
    ASSERT_EQ(table.tree().nodes.size(), 1u);
    const auto* iff = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(iff, nullptr);
    ASSERT_TRUE((*iff)->closer.has_value());
    std::vector<const BlockNode*> branches;
    for (const auto& child : (*iff)->body) {
        if (const auto* b = std::get_if<std::unique_ptr<BlockNode>>(&child)) {
            branches.push_back(b->get());
        }
    }
    ASSERT_EQ(branches.size(), 2u); // ElseIf + Else

    const int ifPos = (*iff)->opener->tokens[0].pos;
    const int endPos = (*iff)->closer->tokens.front().pos;
    const auto fromIf = table.matchBlockAt(ifPos);
    // If + If-Then + ElseIf + ElseIf-Then + Else + End If.
    ASSERT_EQ(fromIf.size(), 6u);
    EXPECT_EQ(fromIf.front().first, ifPos);
    EXPECT_EQ(fromIf.back().first, endPos);
    EXPECT_EQ(table.matchBlockAt(endPos), fromIf); // same group from End If
}

TEST_F(SymbolTableTests, MatchIfGroupFromSingleBranch) {
    const auto table = extract(
        "If a Then\n"
        "ElseIf b Then\n"
        "Else\n"
        "End If\n");
    const auto* iff = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(iff, nullptr);
    std::vector<const BlockNode*> branches;
    for (const auto& child : (*iff)->body) {
        if (const auto* b = std::get_if<std::unique_ptr<BlockNode>>(&child)) {
            branches.push_back(b->get());
        }
    }
    ASSERT_EQ(branches.size(), 2u);
    const int elseIfPos = branches[0]->opener->tokens[0].pos;

    const auto spans = table.matchBlockAt(elseIfPos);
    // If header (If + Then) + this branch (ElseIf + Then) + End If.
    ASSERT_EQ(spans.size(), 5u);
    EXPECT_EQ(spans.front().first, (*iff)->opener->tokens[0].pos);     // If
    EXPECT_EQ(spans.back().first, (*iff)->closer->tokens.front().pos); // End If
    bool hasElseIf = false;
    bool hasElse = false;
    for (const auto& sp : spans) {
        if (sp.first == elseIfPos) { hasElseIf = true; }
        if (sp.first == branches[1]->opener->tokens[0].pos) { hasElse = true; }
    }
    EXPECT_TRUE(hasElseIf);
    EXPECT_FALSE(hasElse); // sibling Else excluded
}

TEST_F(SymbolTableTests, MatchThenHighlightsScopeNotBranches) {
    const auto table = extract(
        "If a Then\n"
        "ElseIf b Then\n"
        "Else\n"
        "End If\n");
    const auto* iff = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(iff, nullptr);
    const int ifPos = (*iff)->opener->tokens.front().pos;
    int thenPos = -1;
    for (const auto& tok : (*iff)->opener->tokens) {
        if (tok.keywordKind == lexer::KeywordKind::Then) { thenPos = tok.pos; break; }
    }
    ASSERT_GE(thenPos, 0);

    const auto spans = table.matchBlockAt(thenPos);
    // Just the enclosing If scope: If + that Then + End If. No ElseIf/Else.
    ASSERT_EQ(spans.size(), 3u);
    EXPECT_EQ(spans.front().first, ifPos);
    EXPECT_EQ(spans[1].first, thenPos);
    EXPECT_EQ(spans.back().first, (*iff)->closer->tokens.front().pos);
    // and it differs from the whole-group highlight (caret on If).
    EXPECT_NE(spans, table.matchBlockAt(ifPos));
}

TEST_F(SymbolTableTests, MatchIfGroupIgnoresSingleLine) {
    const auto table = extract(
        "Sub S\n"
        "If a Then Print 1\n"
        "End Sub\n");
    const auto* sub = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(sub, nullptr);
    int ifPos = -1;
    for (const auto& child : (*sub)->body) {
        if (const auto* st = std::get_if<StatementNode>(&child)) {
            if (!st->tokens.empty()) { ifPos = st->tokens[0].pos; break; }
        } else if (const auto* blk = std::get_if<std::unique_ptr<BlockNode>>(&child)) {
            if ((*blk)->opener && !(*blk)->opener->tokens.empty()) { ifPos = (*blk)->opener->tokens[0].pos; break; }
        }
    }
    ASSERT_GE(ifPos, 0);
    EXPECT_TRUE(table.matchBlockAt(ifPos).empty()); // single-line If: no group, no pair
    int thenPos = -1;
    for (const auto& child : (*sub)->body) {
        if (const auto* st = std::get_if<StatementNode>(&child)) {
            for (const auto& tok : st->tokens) {
                if (tok.keywordKind == lexer::KeywordKind::Then) { thenPos = tok.pos; break; }
            }
        }
    }
    if (thenPos >= 0) {
        EXPECT_TRUE(table.matchBlockAt(thenPos).empty()); // its Then triggers nothing either
    }
}

TEST_F(SymbolTableTests, MatchPpIfGroup) {
    const auto table = extract(
        "#if A\n"
        "x = 1\n"
        "#elseif B\n"
        "x = 2\n"
        "#else\n"
        "x = 3\n"
        "#endif\n");
    ASSERT_EQ(table.tree().nodes.size(), 1u);
    const auto* pp = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(pp, nullptr);
    ASSERT_TRUE((*pp)->closer.has_value());
    std::vector<const BlockNode*> branches;
    for (const auto& child : (*pp)->body) {
        if (const auto* b = std::get_if<std::unique_ptr<BlockNode>>(&child)) {
            branches.push_back(b->get());
        }
    }
    ASSERT_EQ(branches.size(), 2u); // #elseif + #else

    const int ifPos = (*pp)->opener->tokens[0].pos;
    const int endPos = (*pp)->closer->tokens.front().pos;
    const auto whole = table.matchBlockAt(ifPos);
    ASSERT_EQ(whole.size(), 4u); // #if + #elseif + #else + #endif
    EXPECT_EQ(whole.front().first, ifPos);
    EXPECT_EQ(whole.back().first, endPos);
    EXPECT_EQ(table.matchBlockAt(endPos), whole); // same group from #endif

    // Caret on a single #elseif -> that branch + #if/#endif pair, no #else.
    const int elseifPos = branches[0]->opener->tokens[0].pos;
    const auto single = table.matchBlockAt(elseifPos);
    ASSERT_EQ(single.size(), 3u);
    EXPECT_EQ(single.front().first, ifPos);
    EXPECT_EQ(single.back().first, endPos);
    bool hasElse = false;
    for (const auto& sp : single) {
        if (sp.first == branches[1]->opener->tokens[0].pos) { hasElse = true; }
    }
    EXPECT_FALSE(hasElse);
}

TEST_F(SymbolTableTests, MatchPpIfdefPair) {
    const auto table = extract(
        "#ifdef FOO\n"
        "x = 1\n"
        "#endif\n");
    const auto* pp = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(pp, nullptr);
    const int ifPos = (*pp)->opener->tokens[0].pos; // #ifdef
    const auto spans = table.matchBlockAt(ifPos);
    ASSERT_EQ(spans.size(), 2u); // #ifdef + #endif (no branches)
    EXPECT_EQ(spans.front().first, ifPos);
    EXPECT_EQ(spans.back().first, (*pp)->closer->tokens.front().pos);
}

TEST_F(SymbolTableTests, MatchContinueResolvesNestedLoops) {
    const auto table = extract(
        "For n = 2 To 20\n"
        "For d = 2 To 10\n"
        "If n Mod d = 0 Then\n"
        "Continue For, For\n"
        "End If\n"
        "Next d\n"
        "Next n\n");
    ASSERT_EQ(table.tree().nodes.size(), 1u);
    const auto* outerFor = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(outerFor, nullptr);
    const BlockNode* innerFor = nullptr;
    for (const auto& child : (*outerFor)->body) {
        if (const auto* b = std::get_if<std::unique_ptr<BlockNode>>(&child)) { innerFor = b->get(); break; }
    }
    ASSERT_NE(innerFor, nullptr);
    const BlockNode* ifBlock = nullptr;
    for (const auto& child : innerFor->body) {
        if (const auto* b = std::get_if<std::unique_ptr<BlockNode>>(&child)) { ifBlock = b->get(); break; }
    }
    ASSERT_NE(ifBlock, nullptr);
    const StatementNode* cont = nullptr;
    for (const auto& child : ifBlock->body) {
        if (const auto* sNode = std::get_if<StatementNode>(&child)) { cont = sNode; break; }
    }
    ASSERT_NE(cont, nullptr);
    ASSERT_EQ(cont->tokens[0].keywordKind, lexer::KeywordKind::Continue);

    std::vector<int> forArgPos;
    for (std::size_t i = 1; i < cont->tokens.size(); ++i) {
        if (cont->tokens[i].keywordKind == lexer::KeywordKind::For) { forArgPos.push_back(cont->tokens[i].pos); }
    }
    ASSERT_EQ(forArgPos.size(), 2u);

    const int continuePos = cont->tokens[0].pos;
    const int innerOpener = innerFor->opener->tokens[0].pos;
    const int outerOpener = (*outerFor)->opener->tokens[0].pos;

    // Caret on Continue -> ultimate (outer For): Continue token + outer opener + Next n.
    const auto onContinue = table.matchBlockAt(continuePos);
    ASSERT_EQ(onContinue.size(), 3u);
    EXPECT_EQ(onContinue[0].first, continuePos);
    EXPECT_EQ(onContinue[1].first, outerOpener);
    EXPECT_EQ(onContinue[2].first, (*outerFor)->closer->tokens.front().pos);

    // Caret on the 1st For arg -> inner For.
    const auto onFirst = table.matchBlockAt(forArgPos[0]);
    ASSERT_EQ(onFirst.size(), 3u);
    EXPECT_EQ(onFirst[0].first, forArgPos[0]);
    EXPECT_EQ(onFirst[1].first, innerOpener);
    EXPECT_EQ(onFirst[2].first, innerFor->closer->tokens.front().pos);

    // Caret on the 2nd For arg -> outer For.
    const auto onSecond = table.matchBlockAt(forArgPos[1]);
    ASSERT_EQ(onSecond.size(), 3u);
    EXPECT_EQ(onSecond[1].first, outerOpener);
}

TEST_F(SymbolTableTests, MatchExitSubThroughLoop) {
    const auto table = extract(
        "Sub S\n"
        "For i = 1 To 10\n"
        "Exit Sub\n"
        "Next\n"
        "End Sub\n");
    const auto* sub = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(sub, nullptr);
    const BlockNode* forB = nullptr;
    for (const auto& child : (*sub)->body) {
        if (const auto* b = std::get_if<std::unique_ptr<BlockNode>>(&child)) { forB = b->get(); break; }
    }
    ASSERT_NE(forB, nullptr);
    const StatementNode* exit = nullptr;
    for (const auto& child : forB->body) {
        if (const auto* sNode = std::get_if<StatementNode>(&child)) { exit = sNode; break; }
    }
    ASSERT_NE(exit, nullptr);
    ASSERT_EQ(exit->tokens[0].keywordKind, lexer::KeywordKind::Exit);

    const int exitPos = exit->tokens[0].pos;
    const auto spans = table.matchBlockAt(exitPos);
    ASSERT_EQ(spans.size(), 3u);
    EXPECT_EQ(spans[0].first, exitPos);                            // Exit
    EXPECT_EQ(spans[1].first, (*sub)->opener->tokens[0].pos);      // Sub (past the For)
    EXPECT_EQ(spans[2].first, (*sub)->closer->tokens.front().pos); // End Sub
}

TEST_F(SymbolTableTests, MatchExitSubInSingleLineIf) {
    const auto table = extract(
        "Sub foo\n"
        "if true then exit sub\n"
        "End Sub\n");
    const auto* sub = std::get_if<std::unique_ptr<BlockNode>>(&table.tree().nodes[0]);
    ASSERT_NE(sub, nullptr);

    // The single-line If may be a statement or an auto-closed block — find the
    // `exit` keyword either way by scanning all tokens.
    int exitPos = -1;
    const std::function<void(const std::vector<Node>&)> scan = [&](const std::vector<Node>& nodes) {
        for (const auto& node : nodes) {
            const std::vector<lexer::Token>* toks = nullptr;
            if (const auto* st = std::get_if<StatementNode>(&node)) {
                toks = &st->tokens;
            } else if (const auto* b = std::get_if<std::unique_ptr<BlockNode>>(&node)) {
                if ((*b)->opener) { toks = &(*b)->opener->tokens; }
                scan((*b)->body);
            }
            if (toks != nullptr) {
                for (const auto& tk : *toks) {
                    if (tk.keywordKind == lexer::KeywordKind::Exit) { exitPos = tk.pos; }
                }
            }
        }
    };
    scan((*sub)->body);
    ASSERT_GE(exitPos, 0);

    const auto spans = table.matchBlockAt(exitPos);
    ASSERT_EQ(spans.size(), 3u);
    EXPECT_EQ(spans[0].first, exitPos);                            // the Exit keyword
    EXPECT_EQ(spans[1].first, (*sub)->opener->tokens[0].pos);      // Sub
    EXPECT_EQ(spans[2].first, (*sub)->closer->tokens.front().pos); // End Sub
}

TEST_F(SymbolTableTests, GlobalCompletionsListsFreeStandingAndTypes) {
    const std::string src =
        "Sub Free\n"
        "End Sub\n"
        "Function Calc() As Integer\n"
        "End Function\n"
        "Type Vec\n"
        "    x As Integer\n"
        "End Type\n"
        "Sub Vec.Method\n"
        "End Sub\n";
    const auto table = extract(src.c_str());

    std::vector<wxString> out;
    table.globalSymbolCompletions(out);

    const auto has = [&](const wxString& name) { return std::ranges::find(out, name) != out.end(); };
    EXPECT_TRUE(has("Free"));
    EXPECT_TRUE(has("Calc"));
    EXPECT_TRUE(has("Vec"));
    EXPECT_FALSE(has("Vec.Method")); // qualified method excluded from globals
    EXPECT_FALSE(has("Method"));     // member is not a global
}

TEST_F(SymbolTableTests, MemberCompletionsInsideTypeMethod) {
    const std::string src =
        "Type Vec\n"
        "    Declare Sub Foo()\n"
        "End Type\n"
        "Sub Vec.Foo()\n"
        "    Print 1\n"
        "End Sub\n"
        "Sub Vec.Bar()\n"
        "End Sub\n"
        "Function Vec.Size() As Integer\n"
        "End Function\n"
        "Sub Free()\n"
        "End Sub\n";
    const auto table = extract(src.c_str());

    const int pos = static_cast<int>(src.find("Print 1"));
    std::vector<wxString> out;
    table.memberCompletionsAt(pos, out);

    const auto has = [&](const wxString& name) { return std::ranges::find(out, name) != out.end(); };
    EXPECT_TRUE(has("Foo"));
    EXPECT_TRUE(has("Bar"));
    EXPECT_TRUE(has("Size"));
    EXPECT_FALSE(has("Free")); // not a member of Vec
}

TEST_F(SymbolTableTests, MemberCompletionsEmptyOutsideTypeMethod) {
    const std::string src =
        "Sub Free()\n"
        "    Print 1\n"
        "End Sub\n";
    const auto table = extract(src.c_str());

    const int pos = static_cast<int>(src.find("Print 1"));
    std::vector<wxString> out;
    table.memberCompletionsAt(pos, out);
    EXPECT_TRUE(out.empty());
}

TEST_F(SymbolTableTests, LocalCompletionsParamsAndDimsBeforeCaret) {
    const std::string src =
        "Sub Foo(arg As Integer)\n"
        "    Dim early As Integer\n"
        "    Print early\n"
        "    Dim late As Integer\n"
        "End Sub\n";
    const auto table = extract(src.c_str());
    const int pos = static_cast<int>(src.find("Print early"));
    std::vector<wxString> out;
    table.localCompletionsAt(pos, out);
    const auto has = [&](const wxString& name) { return std::ranges::find(out, name) != out.end(); };
    EXPECT_TRUE(has("arg"));    // parameter
    EXPECT_TRUE(has("early"));  // declared before caret
    EXPECT_FALSE(has("late"));  // declared after caret
}

TEST_F(SymbolTableTests, LocalCompletionsBlockScopeAndShadow) {
    const std::string src =
        "Sub Foo()\n"
        "    Dim outer As Integer\n"
        "    If true Then\n"
        "        Dim inner As Integer\n"
        "        Print inner\n"
        "    End If\n"
        "End Sub\n";
    const auto table = extract(src.c_str());
    const int pos = static_cast<int>(src.find("Print inner"));
    std::vector<wxString> out;
    table.localCompletionsAt(pos, out);
    const auto has = [&](const wxString& name) { return std::ranges::find(out, name) != out.end(); };
    EXPECT_TRUE(has("outer")); // enclosing scope
    EXPECT_TRUE(has("inner")); // current block
}

TEST_F(SymbolTableTests, LocalCompletionsSiblingBlockHidden) {
    const std::string src =
        "Sub Foo()\n"
        "    If a Then\n"
        "        Dim hidden As Integer\n"
        "    End If\n"
        "    Print 1\n"
        "End Sub\n";
    const auto table = extract(src.c_str());
    const int pos = static_cast<int>(src.find("Print 1"));
    std::vector<wxString> out;
    table.localCompletionsAt(pos, out);
    EXPECT_TRUE(std::ranges::find(out, wxString("hidden")) == out.end());
}

TEST_F(SymbolTableTests, LocalCompletionsMultiNameAndConst) {
    const std::string src =
        "Sub Foo()\n"
        "    Dim a, b, c As Integer\n"
        "    Const K = 10\n"
        "    Print 1\n"
        "End Sub\n";
    const auto table = extract(src.c_str());
    const int pos = static_cast<int>(src.find("Print 1"));
    std::vector<wxString> out;
    table.localCompletionsAt(pos, out);
    const auto has = [&](const wxString& name) { return std::ranges::find(out, name) != out.end(); };
    EXPECT_TRUE(has("a"));
    EXPECT_TRUE(has("b"));
    EXPECT_TRUE(has("c"));
    EXPECT_TRUE(has("K"));
}

TEST_F(SymbolTableTests, TypeFieldsInMemberCompletion) {
    const std::string src =
        "Type Vec\n"
        "    x As Integer\n"
        "    y As Integer\n"
        "    Declare Sub Move()\n"
        "End Type\n"
        "Sub Vec.Move()\n"
        "    Print 1\n"
        "End Sub\n";
    const auto table = extract(src.c_str());
    const int pos = static_cast<int>(src.find("Print 1"));
    std::vector<wxString> out;
    table.memberCompletionsAt(pos, out);
    const auto has = [&](const wxString& name) { return std::ranges::find(out, name) != out.end(); };
    EXPECT_TRUE(has("x"));    // field
    EXPECT_TRUE(has("y"));    // field
    EXPECT_TRUE(has("Move")); // method
}

TEST_F(SymbolTableTests, ModuleVariablesInGlobalCompletions) {
    const std::string src =
        "Dim Shared g As Integer\n"
        "Const MAXV = 10\n"
        "Sub Foo()\n"
        "End Sub\n";
    const auto table = extract(src.c_str());
    std::vector<wxString> vars;
    table.moduleVariableCompletions(vars);
    const auto hasVar = [&](const wxString& name) { return std::ranges::find(vars, name) != vars.end(); };
    EXPECT_TRUE(hasVar("g"));
    EXPECT_TRUE(hasVar("MAXV"));
    std::vector<wxString> syms;
    table.globalSymbolCompletions(syms);
    EXPECT_TRUE(std::ranges::find(syms, wxString("Foo")) != syms.end());
}
