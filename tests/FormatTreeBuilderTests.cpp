//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "../src/lib/format/formatters/reformat/TreeBuilder.hpp"
#include <gtest/gtest.h>

using namespace fbide::lexer;
using namespace fbide::format;

namespace {

// Helpers to make tokens concisely
auto kw(const std::string_view text, const KeywordKind kk = KeywordKind::Other) -> Token {
    return { TokenKind::Keyword1, kk, OperatorKind::None, std::string(text) };
}

auto id(const std::string_view text) -> Token {
    return { TokenKind::Identifier, KeywordKind::None, OperatorKind::None, std::string(text) };
}

auto op(const std::string_view text, const OperatorKind ok = OperatorKind::Assign) -> Token {
    return { TokenKind::Operator, KeywordKind::None, ok, std::string(text) };
}

auto num(const std::string_view text) -> Token {
    return { TokenKind::Number, KeywordKind::None, OperatorKind::None, std::string(text) };
}

auto str(const std::string_view text) -> Token {
    return { TokenKind::String, KeywordKind::None, OperatorKind::None, std::string(text) };
}

auto pp(const std::string_view text, const KeywordKind kk = KeywordKind::PpOther) -> Token {
    return { TokenKind::Preprocessor, kk, OperatorKind::None, std::string(text) };
}

// Helpers to inspect the tree
auto asStatement(const Node& node) -> const StatementNode* {
    return std::get_if<StatementNode>(&node);
}

auto asBlock(const Node& node) -> const BlockNode* {
    const auto* p = std::get_if<std::unique_ptr<BlockNode>>(&node);
    return p ? p->get() : nullptr;
}

auto isBlankLine(const Node& node) -> bool {
    return std::holds_alternative<BlankLineNode>(node);
}

} // namespace

// ---------------------------------------------------------------------------
// Basic statements
// ---------------------------------------------------------------------------

TEST(TreeBuilderTests, EmptyInput) {
    TreeBuilder builder;
    auto tree = builder.finish();
    EXPECT_TRUE(tree.nodes.empty());
}

TEST(TreeBuilderTests, SingleStatement) {
    TreeBuilder builder;
    builder.append(kw("Print"));
    builder.append(str("\"hello\""));
    builder.statement();
    auto tree = builder.finish();

    ASSERT_EQ(tree.nodes.size(), 1);
    const auto* stmt = asStatement(tree.nodes[0]);
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->tokens.size(), 2);
    EXPECT_EQ(stmt->tokens[0].text, "Print");
    EXPECT_EQ(stmt->tokens[1].text, "\"hello\"");
}

TEST(TreeBuilderTests, MultipleStatements) {
    TreeBuilder builder;
    builder.append(id("x"));
    builder.append(op("="));
    builder.append(num("1"));
    builder.statement();
    builder.append(id("y"));
    builder.append(op("="));
    builder.append(num("2"));
    builder.statement();
    auto tree = builder.finish();

    ASSERT_EQ(tree.nodes.size(), 2);
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
    EXPECT_NE(asStatement(tree.nodes[1]), nullptr);
}

// ---------------------------------------------------------------------------
// Simple blocks
// ---------------------------------------------------------------------------

TEST(TreeBuilderTests, SimpleBlock) {
    TreeBuilder builder;
    builder.append(kw("Sub", KeywordKind::Sub));
    builder.append(id("Main"));
    builder.openBlock();

    builder.append(kw("Print"));
    builder.append(str("\"hello\""));
    builder.statement();

    builder.append(kw("End", KeywordKind::End));
    builder.append(kw("Sub", KeywordKind::Sub));
    builder.closeBlock();

    auto tree = builder.finish();

    ASSERT_EQ(tree.nodes.size(), 1);
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);

    // Opener
    ASSERT_TRUE(block->opener.has_value());
    EXPECT_EQ(block->opener->tokens.size(), 2);
    EXPECT_EQ(block->opener->tokens[0].text, "Sub");

    // Body
    ASSERT_EQ(block->body.size(), 1);
    EXPECT_NE(asStatement(block->body[0]), nullptr);

    // Closer
    ASSERT_TRUE(block->closer.has_value());
    EXPECT_EQ(block->closer->tokens.size(), 2);
    EXPECT_EQ(block->closer->tokens[0].text, "End");
}

// ---------------------------------------------------------------------------
// Nested blocks
// ---------------------------------------------------------------------------

TEST(TreeBuilderTests, NestedBlocks) {
    TreeBuilder builder;
    builder.append(kw("Sub", KeywordKind::Sub));
    builder.openBlock();

    builder.append(kw("For", KeywordKind::For));
    builder.openBlock();

    builder.append(kw("Print"));
    builder.statement();

    builder.append(kw("Next", KeywordKind::Next));
    builder.closeBlock();

    builder.append(kw("End", KeywordKind::End));
    builder.closeBlock();

    auto tree = builder.finish();

    ASSERT_EQ(tree.nodes.size(), 1);
    const auto* outer = asBlock(tree.nodes[0]);
    ASSERT_NE(outer, nullptr);
    ASSERT_EQ(outer->body.size(), 1);

    const auto* inner = asBlock(outer->body[0]);
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->body.size(), 1);
    EXPECT_TRUE(inner->closer.has_value());
}

// ---------------------------------------------------------------------------
// Branches (Else, Case)
// ---------------------------------------------------------------------------

TEST(TreeBuilderTests, IfElseBranch) {
    TreeBuilder builder;
    // If x Then
    builder.append(kw("If", KeywordKind::If));
    builder.append(id("x"));
    builder.append(kw("Then", KeywordKind::Then));
    builder.openBlock();

    // body
    builder.append(kw("Print"));
    builder.append(str("\"yes\""));
    builder.statement();

    // Else
    builder.append(kw("Else", KeywordKind::Else));
    builder.openBranch();

    // else body
    builder.append(kw("Print"));
    builder.append(str("\"no\""));
    builder.statement();

    // End If
    builder.append(kw("End", KeywordKind::End));
    builder.append(kw("If", KeywordKind::If));
    builder.closeBlock();

    auto tree = builder.finish();

    ASSERT_EQ(tree.nodes.size(), 1);
    const auto* ifBlock = asBlock(tree.nodes[0]);
    ASSERT_NE(ifBlock, nullptr);
    EXPECT_TRUE(ifBlock->opener.has_value());
    EXPECT_TRUE(ifBlock->closer.has_value());

    // Body: one statement + one branch block
    ASSERT_EQ(ifBlock->body.size(), 2);
    EXPECT_NE(asStatement(ifBlock->body[0]), nullptr);

    const auto* elseBranch = asBlock(ifBlock->body[1]);
    ASSERT_NE(elseBranch, nullptr);
    EXPECT_TRUE(elseBranch->opener.has_value());
    EXPECT_EQ(elseBranch->opener->tokens[0].text, "Else");
    EXPECT_FALSE(elseBranch->closer.has_value()); // branch has no closer
    ASSERT_EQ(elseBranch->body.size(), 1);
}

TEST(TreeBuilderTests, MultipleBranches) {
    TreeBuilder builder;
    // Select Case x
    builder.append(kw("Select", KeywordKind::Select));
    builder.openBlock();

    // Case 1
    builder.append(kw("Case", KeywordKind::Case));
    builder.append(num("1"));
    builder.openBranch();
    builder.append(kw("Print"));
    builder.append(str("\"one\""));
    builder.statement();

    // Case 2
    builder.append(kw("Case", KeywordKind::Case));
    builder.append(num("2"));
    builder.openBranch();
    builder.append(kw("Print"));
    builder.append(str("\"two\""));
    builder.statement();

    // End Select
    builder.append(kw("End", KeywordKind::End));
    builder.closeBlock();

    auto tree = builder.finish();

    const auto* selectBlock = asBlock(tree.nodes[0]);
    ASSERT_NE(selectBlock, nullptr);

    // Two Case branches
    ASSERT_EQ(selectBlock->body.size(), 2);
    const auto* case1 = asBlock(selectBlock->body[0]);
    const auto* case2 = asBlock(selectBlock->body[1]);
    ASSERT_NE(case1, nullptr);
    ASSERT_NE(case2, nullptr);
    EXPECT_EQ(case1->opener->tokens[0].text, "Case");
    EXPECT_EQ(case2->opener->tokens[0].text, "Case");
    EXPECT_EQ(case1->body.size(), 1);
    EXPECT_EQ(case2->body.size(), 1);
}

// ---------------------------------------------------------------------------
// Blank lines
// ---------------------------------------------------------------------------

TEST(TreeBuilderTests, BlankLineBetweenStatements) {
    TreeBuilder builder;
    builder.append(id("x"));
    builder.statement();
    builder.blankLine();
    builder.append(id("y"));
    builder.statement();
    auto tree = builder.finish();

    ASSERT_EQ(tree.nodes.size(), 3);
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
    EXPECT_TRUE(isBlankLine(tree.nodes[1]));
    EXPECT_NE(asStatement(tree.nodes[2]), nullptr);
}

// ---------------------------------------------------------------------------
// Malformed input
// ---------------------------------------------------------------------------

TEST(TreeBuilderTests, UnmatchedCloserBecomesStatement) {
    TreeBuilder builder;
    builder.append(kw("End", KeywordKind::End));
    builder.append(kw("Sub", KeywordKind::Sub));
    builder.closeBlock();
    auto tree = builder.finish();

    // No open block → emitted as statement
    ASSERT_EQ(tree.nodes.size(), 1);
    EXPECT_NE(asStatement(tree.nodes[0]), nullptr);
}

TEST(TreeBuilderTests, UnclosedBlockAutoCloses) {
    TreeBuilder builder;
    builder.append(kw("Sub", KeywordKind::Sub));
    builder.openBlock();
    builder.append(kw("Print"));
    builder.statement();
    // No closeBlock — finish() auto-closes
    auto tree = builder.finish();

    ASSERT_EQ(tree.nodes.size(), 1);
    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    EXPECT_TRUE(block->opener.has_value());
    EXPECT_FALSE(block->closer.has_value()); // auto-closed, no closer
    EXPECT_EQ(block->body.size(), 1);
}

TEST(TreeBuilderTests, UnclosedBranchAutoCloses) {
    TreeBuilder builder;
    builder.append(kw("If", KeywordKind::If));
    builder.openBlock();
    builder.append(kw("Else", KeywordKind::Else));
    builder.openBranch();
    builder.append(kw("Print"));
    builder.statement();
    // No closeBlock — finish() auto-closes both
    auto tree = builder.finish();

    ASSERT_EQ(tree.nodes.size(), 1);
    const auto* ifBlock = asBlock(tree.nodes[0]);
    ASSERT_NE(ifBlock, nullptr);
    // Branch was auto-closed into If body
    ASSERT_EQ(ifBlock->body.size(), 1);
    EXPECT_NE(asBlock(ifBlock->body[0]), nullptr);
}

// ---------------------------------------------------------------------------
// PP blocks
// ---------------------------------------------------------------------------

TEST(TreeBuilderTests, PPBlock) {
    TreeBuilder builder;
    builder.append(pp("#ifdef DEBUG", KeywordKind::PpIfDef));
    builder.openBlock();

    builder.append(kw("Print"));
    builder.statement();

    builder.append(pp("#else", KeywordKind::PpElse));
    builder.openBranch();

    builder.append(kw("Print"));
    builder.statement();

    builder.append(pp("#endif", KeywordKind::PpEndIf));
    builder.closeBlock();

    auto tree = builder.finish();

    const auto* ppBlock = asBlock(tree.nodes[0]);
    ASSERT_NE(ppBlock, nullptr);
    EXPECT_TRUE(ppBlock->opener.has_value());
    EXPECT_TRUE(ppBlock->closer.has_value());

    // Body: one statement + one #else branch
    ASSERT_EQ(ppBlock->body.size(), 2);
    EXPECT_NE(asStatement(ppBlock->body[0]), nullptr);
    const auto* elseBranch = asBlock(ppBlock->body[1]);
    ASSERT_NE(elseBranch, nullptr);
    EXPECT_EQ(elseBranch->body.size(), 1);
}

// ---------------------------------------------------------------------------
// Empty collections
// ---------------------------------------------------------------------------

TEST(TreeBuilderTests, EmptyStatementIgnored) {
    TreeBuilder builder;
    builder.statement(); // nothing collected
    builder.append(id("x"));
    builder.statement();
    auto tree = builder.finish();

    ASSERT_EQ(tree.nodes.size(), 1);
}

TEST(TreeBuilderTests, BlockWithNoOpener) {
    TreeBuilder builder;
    builder.openBlock(); // no tokens collected → opener is empty
    builder.append(kw("Print"));
    builder.statement();
    builder.append(kw("End"));
    builder.closeBlock();
    auto tree = builder.finish();

    const auto* block = asBlock(tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    EXPECT_FALSE(block->opener.has_value()); // no opener tokens
    EXPECT_TRUE(block->closer.has_value());
}
