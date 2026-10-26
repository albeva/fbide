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
        m_lexer = tests::createFbLexer(testDataPath + "resources/ide/keywords.ini");
    }

    void TearDown() override {
        m_lexer->Release();
        m_lexer = nullptr;
    }

    auto parse(const char* source) -> ProgramTree {
        const auto tokens = tests::tokenise(*m_lexer, source);
        TreeParser parser({});
        return parser.parse(tokens);
    }

    /// Text of the first significant token of a statement, or "".
    static auto head(const std::optional<StatementNode>& stmt) -> std::string {
        if (!stmt) {
            return {};
        }
        for (const auto& tkn : stmt->tokens) {
            if (tkn.kind != lexer::TokenKind::Whitespace && tkn.kind != lexer::TokenKind::Newline) {
                return tkn.text;
            }
        }
        return {};
    }

    /// Blocks directly in `nodes`.
    static auto blocks(const std::vector<Node>& nodes) -> std::vector<const BlockNode*> {
        std::vector<const BlockNode*> out;
        for (const auto& node : nodes) {
            if (const auto* block = std::get_if<std::unique_ptr<BlockNode>>(&node)) {
                out.push_back(block->get());
            }
        }
        return out;
    }

    /// Depth-first search for the first block whose opener starts with `text`.
    static auto findBlock(const std::vector<Node>& nodes, const std::string& text) -> const BlockNode* {
        for (const auto* block : blocks(nodes)) {
            if (head(block->opener) == text) {
                return block;
            }
            if (const auto* found = findBlock(block->body, text)) {
                return found;
            }
        }
        return nullptr;
    }

    /// Heads of the statements directly in `nodes`.
    static auto statementHeads(const std::vector<Node>& nodes) -> std::vector<std::string> {
        std::vector<std::string> out;
        for (const auto& node : nodes) {
            if (const auto* st = std::get_if<StatementNode>(&node)) {
                if (auto text = head(*st); !text.empty()) {
                    out.push_back(std::move(text));
                }
            }
        }
        return out;
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

TEST_F(TreeParserTests, SingleLineIfWithColonsIsOneStatement) {
    const auto tokens = tests::tokenise(*m_lexer, "If c Then a = 1 : b = 2\n");
    TreeParser parser({});
    const auto tree = parser.parse(tokens);

    std::size_t statements = 0;
    for (const auto& node : tree.nodes) {
        if (const auto* st = std::get_if<StatementNode>(&node)) {
            const bool significant = std::ranges::any_of(st->tokens, [](const lexer::Token& t) {
                return t.kind != lexer::TokenKind::Whitespace && t.kind != lexer::TokenKind::Newline;
            });
            statements += significant ? 1 : 0;
        }
    }
    EXPECT_EQ(statements, 1u);
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

// ---------------------------------------------------------------------------
// Blocks closed inside preprocessor branches (issue #136)
// ---------------------------------------------------------------------------

TEST_F(TreeParserTests, BlockClosedInEveryPpBranchClosesAtEndIf) {
    const auto tree = parse(
        "Sub A\n"
        "Do\n"
        "#if X\n"
        "Loop Until p\n"
        "#else\n"
        "Loop Until q\n"
        "#endif\n"
        "End Sub\n"
        "Sub B\n"
        "End Sub\n");
    const auto top = blocks(tree.nodes);
    ASSERT_EQ(top.size(), 2u);
    EXPECT_EQ(head(top[0]->opener), "Sub");
    EXPECT_EQ(head(top[0]->closer), "End");
    EXPECT_EQ(head(top[1]->opener), "Sub");
    // The `Do` sits in `A` and holds the `#if` block.
    const auto inner = blocks(top[0]->body);
    ASSERT_EQ(inner.size(), 1u);
    EXPECT_EQ(head(inner[0]->opener), "Do");
    EXPECT_NE(findBlock(inner[0]->body, "#if"), nullptr);
}

TEST_F(TreeParserTests, Issue136DelayHrSampleKeepsProceduresTopLevel) {
    const auto tree = parse(
        "Sub delayHR(ByVal amount As Single, ByVal threshold As Ulong)\n"
        "    Dim As Double t1 = Timer\n"
        "    Dim As Double t2\n"
        "    Dim As Double t3 = t1 + amount / 1000\n"
        "    If amount > threshold + 0.5 Then\n"
        "        _setTimer()\n"
        "        Sleep amount - threshold, 1\n"
        "        _resetTimer()\n"
        "    End If\n"
        "    Do\n"
        "    #if Not defined(__FB_WIN32__) And Not defined(__FB_LINUX__)\n"
        "        t2 = Timer\n"
        "        If t2 < t1 Then t1 -= 24 * 60 * 60 : t3 -= 24 * 60 * 60\n"
        "    Loop Until t2 >= t3\n"
        "    #else\n"
        "    Loop Until Timer >= t3\n"
        "    #endif\n"
        "End Sub\n"
        "\n"
        "Sub Other()\n"
        "End Sub\n");
    const auto top = blocks(tree.nodes);
    ASSERT_EQ(top.size(), 2u);
    EXPECT_EQ(head(top[0]->closer), "End");
    EXPECT_EQ(head(top[1]->opener), "Sub");
}

TEST_F(TreeParserTests, CloserInPpElseDoesNotEndBranch) {
    const auto tree = parse(
        "Do\n"
        "#if X\n"
        "Loop Until p\n"
        "#else\n"
        "Loop Until q\n"
        "x = 1\n"
        "#endif\n");
    const auto* elseBranch = findBlock(tree.nodes, "#else");
    ASSERT_NE(elseBranch, nullptr);
    EXPECT_EQ(statementHeads(elseBranch->body), (std::vector<std::string> { "Loop", "x" }));
}

TEST_F(TreeParserTests, UnevenPpBranchClosersKeepOuterBlockOpen) {
    const auto tree = parse(
        "Do\n"
        "#if X\n"
        "Loop\n"
        "#endif\n"
        "Loop\n");
    const auto top = blocks(tree.nodes);
    ASSERT_EQ(top.size(), 1u);
    EXPECT_EQ(head(top[0]->opener), "Do");
    EXPECT_EQ(head(top[0]->closer), "Loop"); // the final Loop closes the Do
    const auto* ppIf = findBlock(top[0]->body, "#if");
    ASSERT_NE(ppIf, nullptr);
    EXPECT_EQ(statementHeads(ppIf->body), (std::vector<std::string> { "Loop" }));
}
