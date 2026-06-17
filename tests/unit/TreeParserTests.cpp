//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "TestHelpers.hpp"
#include "analyses/parser/TreeParser.hpp"

using namespace fbide;
using namespace fbide::parser;

class TreeParserTests : public testing::Test {
protected:
    static inline const wxString testDataPath = FBIDE_TEST_DATA_DIR;

    void SetUp() override {
        m_lexer = tests::createFbLexer(testDataPath + "fbfull.lng");
    }

    void TearDown() override {
        m_lexer->Release();
        m_lexer = nullptr;
    }

    Scintilla::ILexer5* m_lexer { nullptr };
};

// ---------------------------------------------------------------------------
// Lean mode — strips Whitespace/Comment/CommentBlock tokens and collapses
// runs of Newlines. Used by non-rendering consumers (symbol browser).
// ---------------------------------------------------------------------------

TEST_F(TreeParserTests, LeanFilterDropsLayoutAndCollapsesBlankLines) {
    const auto tokens = tests::tokenise(*m_lexer,
        "  Sub Foo  ' inline comment\n"
        "\n"
        "\n"
        "    /' multi\n"
        "       line '/\n"
        "    Print 1\n"
        "End Sub\n");
    TreeParser parser({ .lean = true });
    const auto tree = parser.parse(tokens);

    // Walk every token in the tree and verify no layout/comment tokens remain.
    const auto countByKind = [&](lexer::TokenKind kind) {
        int count = 0;
        const auto countInTokens = [&](const std::vector<lexer::Token>& tokens) {
            for (const auto& tkn : tokens) {
                if (tkn.kind == kind) {
                    count++;
                }
            }
        };
        const std::function<void(const Node&)> walk = [&](const Node& node) {
            if (const auto* st = std::get_if<StatementNode>(&node)) {
                countInTokens(st->tokens);
            } else if (const auto* block = std::get_if<std::unique_ptr<BlockNode>>(&node)) {
                if ((*block)->opener) {
                    countInTokens((*block)->opener->tokens);
                }
                for (const auto& child : (*block)->body) {
                    walk(child);
                }
                if ((*block)->closer) {
                    countInTokens((*block)->closer->tokens);
                }
            }
        };
        for (const auto& node : tree.nodes) {
            walk(node);
        }
        return count;
    };

    EXPECT_EQ(countByKind(lexer::TokenKind::Whitespace), 0);
    EXPECT_EQ(countByKind(lexer::TokenKind::Comment), 0);
    EXPECT_EQ(countByKind(lexer::TokenKind::CommentBlock), 0);
}

TEST_F(TreeParserTests, LeanModeProducesSubBlock) {
    const auto tokens = tests::tokenise(*m_lexer,
        "Sub Foo\n"
        "Print 1\n"
        "End Sub\n");
    TreeParser parser({ .lean = true });
    const auto tree = parser.parse(tokens);

    ASSERT_EQ(tree.nodes.size(), 1u);
    const auto* block = std::get_if<std::unique_ptr<BlockNode>>(&tree.nodes[0]);
    ASSERT_NE(block, nullptr);
    ASSERT_TRUE((*block)->opener.has_value());
    ASSERT_TRUE((*block)->closer.has_value());

    // First significant token in opener is the Sub keyword.
    const auto& openerTokens = (*block)->opener->tokens;
    ASSERT_FALSE(openerTokens.empty());
    EXPECT_EQ(openerTokens[0].keywordKind, lexer::KeywordKind::Sub);
}

TEST_F(TreeParserTests, LeanModePreservesVerbatimRegion) {
    const auto tokens = tests::tokenise(*m_lexer,
        "' format off\n"
        "  X   =   1\n"
        "' format on\n"
        "Sub Foo\n"
        "End Sub\n");
    TreeParser parser({ .lean = true });
    const auto tree = parser.parse(tokens);

    bool sawVerbatim = false;
    for (const auto& node : tree.nodes) {
        if (std::holds_alternative<VerbatimNode>(node)) {
            sawVerbatim = true;
            break;
        }
    }
    EXPECT_TRUE(sawVerbatim);
}

TEST_F(TreeParserTests, WiresBlockParentPointers) {
    const auto tokens = tests::tokenise(*m_lexer,
        "Sub Foo\n"
        "For i = 1 To 10\n"
        "Print i\n"
        "Next\n"
        "End Sub\n");
    TreeParser parser({ .lean = true });
    const auto tree = parser.parse(tokens);

    ASSERT_EQ(tree.nodes.size(), 1u);
    const auto* sub = std::get_if<std::unique_ptr<BlockNode>>(&tree.nodes[0]);
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ((*sub)->parent, nullptr); // top level

    const BlockNode* forBlock = nullptr;
    for (const auto& child : (*sub)->body) {
        if (const auto* nested = std::get_if<std::unique_ptr<BlockNode>>(&child)) {
            forBlock = nested->get();
        }
    }
    ASSERT_NE(forBlock, nullptr);
    EXPECT_EQ(forBlock->parent, sub->get()); // For's parent is the enclosing Sub
}

TEST_F(TreeParserTests, RecyclesNodesAcrossBuilds) {
    const auto tokens = tests::tokenise(*m_lexer,
        "Sub Foo\n"
        "For i = 1 To 10\n"
        "Next\n"
        "End Sub\n");
    TreeParser parser({ .lean = true });
    auto first = parser.parse(tokens);
    ASSERT_EQ(first.nodes.size(), 1u);

    // Rebuild reusing the first tree's BlockNodes; recycled nodes must be reset
    // cleanly, so the second tree has the same shape with no leftovers.
    auto second = parser.parse(tokens, std::move(first));
    ASSERT_EQ(second.nodes.size(), 1u);
    const auto* sub = std::get_if<std::unique_ptr<BlockNode>>(&second.nodes[0]);
    ASSERT_NE(sub, nullptr);
    ASSERT_TRUE((*sub)->opener.has_value());
    ASSERT_TRUE((*sub)->closer.has_value());
    EXPECT_EQ((*sub)->opener->tokens[0].keywordKind, lexer::KeywordKind::Sub);
    EXPECT_EQ((*sub)->parent, nullptr);
    int blockChildren = 0;
    for (const auto& child : (*sub)->body) {
        if (std::holds_alternative<std::unique_ptr<BlockNode>>(child)) {
            blockChildren++;
        }
    }
    EXPECT_EQ(blockChildren, 1);
}
