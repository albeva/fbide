//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "lib/analyses/lexer/Lexer.hpp"
#include "lib/config/Keywords.hpp"
#include "lib/format/formatters/Scanner.hpp"
#include <gtest/gtest.h>

using namespace fbide;
using namespace fbide::format;
using namespace fbide::lexer;

class FormatScannerTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;

    void SetUp() override {
        Keywords kw;
        kw.load(testDataPath + "fbfull.lng");
        m_lexer = std::make_unique<Lexer>(kw);
    }

    auto scan(const char* source) -> ProgramTree {
        const auto tokens = m_lexer->tokenise(source);
        return Scanner::scan(tokens);
    }

    static auto asStatement(const Node& node) -> const StatementNode* {
        return std::get_if<StatementNode>(&node);
    }

    static auto asBlock(const Node& node) -> const BlockNode* {
        const auto* p = std::get_if<std::unique_ptr<BlockNode>>(&node);
        return p ? p->get() : nullptr;
    }

    static auto isBlankLine(const Node& node) -> bool {
        return std::holds_alternative<BlankLineNode>(node);
    }

    /// Get text of first token in a statement
    static auto firstText(const StatementNode& stmt) -> std::string_view {
        return stmt.tokens.empty() ? "" : stmt.tokens[0].text;
    }

    std::unique_ptr<Lexer> m_lexer;
};

// ---------------------------------------------------------------------------
// Simple statements
// ---------------------------------------------------------------------------

TEST_F(FormatScannerTests, SingleStatement) {
    auto tree = scan("Print \"hello\"\n");
    ASSERT_EQ(tree.nodes.size(), 1);
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
}

TEST_F(FormatScannerTests, MultipleStatements) {
    auto tree = scan("x = 1\ny = 2\n");
    ASSERT_EQ(tree.nodes.size(), 2);
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
    EXPECT_NE(asStatement(tree.nodes[1]), nullptr);
}

// ---------------------------------------------------------------------------
// Simple blocks
// ---------------------------------------------------------------------------

TEST_F(FormatScannerTests, SubBlock) {
    auto tree = scan(
        "Sub Main\n"
        "Print \"hello\"\n"
        "End Sub\n"
    );
    ASSERT_EQ(tree.nodes.size(), 1);
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    EXPECT_TRUE(block->opener.has_value());
    EXPECT_EQ(firstText(*block->opener), "Sub");
    EXPECT_TRUE(block->closer.has_value());
    EXPECT_EQ(firstText(*block->closer), "End");
    ASSERT_EQ(block->body.size(), 1);
    EXPECT_NE(asStatement(block->body[0]), nullptr);
}

TEST_F(FormatScannerTests, ForNextBlock) {
    auto tree = scan(
        "For i = 1 To 10\n"
        "Print i\n"
        "Next\n"
    );
    ASSERT_EQ(tree.nodes.size(), 1);
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(firstText(*block->opener), "For");
    EXPECT_EQ(firstText(*block->closer), "Next");
}

TEST_F(FormatScannerTests, NestedBlocks) {
    auto tree = scan(
        "Sub Main\n"
        "For i = 1 To 10\n"
        "Print i\n"
        "Next\n"
        "End Sub\n"
    );
    const auto* outer = asBlock(tree.nodes[0]);
    ASSERT_NE(outer, nullptr);
    ASSERT_EQ(outer->body.size(), 1);
    const auto* inner = asBlock(outer->body[0]);
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(firstText(*inner->opener), "For");
}

// ---------------------------------------------------------------------------
// If / Then / Else
// ---------------------------------------------------------------------------

TEST_F(FormatScannerTests, MultiLineIf) {
    auto tree = scan(
        "If x > 0 Then\n"
        "Print x\n"
        "End If\n"
    );
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(firstText(*block->opener), "If");
    ASSERT_EQ(block->body.size(), 1);
    EXPECT_NE(asStatement(block->body[0]), nullptr);
}

TEST_F(FormatScannerTests, SingleLineIf) {
    auto tree = scan("If x > 0 Then Print x\n");
    ASSERT_EQ(tree.nodes.size(), 1);
    // Single-line If is a statement, not a block
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
}

TEST_F(FormatScannerTests, IfElse) {
    auto tree = scan(
        "If x Then\n"
        "Print \"yes\"\n"
        "Else\n"
        "Print \"no\"\n"
        "End If\n"
    );
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    // Body: statement + Else branch
    ASSERT_EQ(block->body.size(), 2);
    EXPECT_NE(asStatement(block->body[0]), nullptr);
    const auto* elseBranch = asBlock(block->body[1]);
    ASSERT_NE(elseBranch, nullptr);
    EXPECT_EQ(firstText(*elseBranch->opener), "Else");
    EXPECT_FALSE(elseBranch->closer.has_value());
}

TEST_F(FormatScannerTests, IfElseIfElse) {
    auto tree = scan(
        "If x Then\n"
        "Print 1\n"
        "ElseIf y Then\n"
        "Print 2\n"
        "Else\n"
        "Print 3\n"
        "End If\n"
    );
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    // Body: stmt, ElseIf branch, Else branch
    ASSERT_EQ(block->body.size(), 3);
    EXPECT_NE(asStatement(block->body[0]), nullptr);
    EXPECT_NE(asBlock(block->body[1]), nullptr);
    EXPECT_NE(asBlock(block->body[2]), nullptr);
}

TEST_F(FormatScannerTests, IfThenWithComment) {
    auto tree = scan(
        "If x Then ' comment\n"
        "Print x\n"
        "End If\n"
    );
    // Then is last significant token (comment ignored) → multi-line
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
}

// ---------------------------------------------------------------------------
// Select / Case
// ---------------------------------------------------------------------------

TEST_F(FormatScannerTests, SelectCase) {
    auto tree = scan(
        "Select Case x\n"
        "Case 1\n"
        "Print \"one\"\n"
        "Case 2\n"
        "Print \"two\"\n"
        "End Select\n"
    );
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(firstText(*block->opener), "Select");
    // Two Case branches
    ASSERT_EQ(block->body.size(), 2);
    EXPECT_NE(asBlock(block->body[0]), nullptr);
    EXPECT_NE(asBlock(block->body[1]), nullptr);
}

// ---------------------------------------------------------------------------
// Type
// ---------------------------------------------------------------------------

TEST_F(FormatScannerTests, TypeBlock) {
    auto tree = scan(
        "Type MyType\n"
        "x As Integer\n"
        "End Type\n"
    );
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(firstText(*block->opener), "Type");
}

TEST_F(FormatScannerTests, TypeAsAlias) {
    auto tree = scan("Type As Integer MyInt\n");
    // Type As → alias, not a block
    ASSERT_EQ(tree.nodes.size(), 1);
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
}

// ---------------------------------------------------------------------------
// Declare
// ---------------------------------------------------------------------------

TEST_F(FormatScannerTests, DeclareSubNoBlock) {
    auto tree = scan("Declare Sub Main()\nDim x = 1\n");
    // Declare prevents Sub from opening a block
    ASSERT_EQ(tree.nodes.size(), 2);
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
    EXPECT_NE(asStatement(tree.nodes[1]), nullptr);
}

// ---------------------------------------------------------------------------
// Preprocessor
// ---------------------------------------------------------------------------

TEST_F(FormatScannerTests, PPIfdefEndif) {
    auto tree = scan(
        "#ifdef DEBUG\n"
        "#define X 1\n"
        "#endif\n"
    );
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->body.size(), 1);
    EXPECT_NE(asStatement(block->body[0]), nullptr);
}

TEST_F(FormatScannerTests, PPIfdefElseEndif) {
    auto tree = scan(
        "#ifdef DEBUG\n"
        "Print 1\n"
        "#else\n"
        "Print 2\n"
        "#endif\n"
    );
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    // Body: statement + #else branch
    ASSERT_EQ(block->body.size(), 2);
    const auto* elseBranch = asBlock(block->body[1]);
    ASSERT_NE(elseBranch, nullptr);
    EXPECT_FALSE(elseBranch->closer.has_value());
}

TEST_F(FormatScannerTests, PPInsideCodeBlock) {
    auto tree = scan(
        "Sub Main\n"
        "#ifdef DEBUG\n"
        "Print x\n"
        "#endif\n"
        "End Sub\n"
    );
    const auto* sub = asBlock(tree.nodes[0]);
    ASSERT_NE(sub, nullptr);
    // Body: PP block
    ASSERT_EQ(sub->body.size(), 1);
    const auto* ppBlock = asBlock(sub->body[0]);
    ASSERT_NE(ppBlock, nullptr);
}

TEST_F(FormatScannerTests, PPNonBlockDirective) {
    auto tree = scan("#include \"file.bi\"\n");
    ASSERT_EQ(tree.nodes.size(), 1);
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
}

// ---------------------------------------------------------------------------
// Colon splitting
// ---------------------------------------------------------------------------

TEST_F(FormatScannerTests, ColonSplitsStatements) {
    auto tree = scan("x = 1 : y = 2\n");
    ASSERT_EQ(tree.nodes.size(), 2);
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
    EXPECT_NE(asStatement(tree.nodes[1]), nullptr);
}

TEST_F(FormatScannerTests, ColonSplitsIntoBlock) {
    auto tree = scan("If x Then : Print x : End If\n");
    // "If x Then" → block, "Print x" → body, "End If" → closer
    ASSERT_EQ(tree.nodes.size(), 1);
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->body.size(), 1);
    EXPECT_TRUE(block->closer.has_value());
}

// ---------------------------------------------------------------------------
// Blank lines
// ---------------------------------------------------------------------------

TEST_F(FormatScannerTests, BlankLinesPreserved) {
    auto tree = scan("x = 1\n\ny = 2\n");
    ASSERT_EQ(tree.nodes.size(), 3);
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
    EXPECT_TRUE(isBlankLine(tree.nodes[1]));
    EXPECT_NE(asStatement(tree.nodes[2]), nullptr);
}

// ---------------------------------------------------------------------------
// Malformed input
// ---------------------------------------------------------------------------

TEST_F(FormatScannerTests, UnmatchedEndSub) {
    auto tree = scan("End Sub\n");
    // No open block → emitted as statement
    ASSERT_EQ(tree.nodes.size(), 1);
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
}

TEST_F(FormatScannerTests, UnclosedSub) {
    auto tree = scan("Sub Main\nPrint x\n");
    // Auto-closed at end
    ASSERT_EQ(tree.nodes.size(), 1);
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    EXPECT_FALSE(block->closer.has_value());
}
