//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "analyses/symbols/SymbolTable.hpp"
#include "format/transformers/reformat/ReFormatter.hpp"
#include "TestHelpers.hpp"

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
