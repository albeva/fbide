//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "TreeBuilder.hpp"
using namespace fbide::format;

void TreeBuilder::append(const lexer::Token& token) {
    m_collected.push_back(token);
}

void TreeBuilder::statement() {
    if (m_collected.empty()) {
        return;
    }
    addNode(flushTokens());
}

void TreeBuilder::openBlock() {
    auto block = std::make_unique<BlockNode>();
    if (!m_collected.empty()) {
        block->opener = flushTokens();
    }
    m_stack.push_back({ std::move(block), false });
}

void TreeBuilder::openBranch() {
    closeBranch();

    auto branch = std::make_unique<BlockNode>();
    if (!m_collected.empty()) {
        branch->opener = flushTokens();
    }
    m_stack.push_back({ std::move(branch), true });
}

void TreeBuilder::closeBlock() {
    closeBranch();

    if (m_stack.empty()) {
        // Unmatched closer — emit as a statement
        statement();
        return;
    }

    auto entry = std::move(m_stack.back());
    m_stack.pop_back();

    if (!m_collected.empty()) {
        entry.node->closer = flushTokens();
    }

    addNode(std::move(entry.node));
}

void TreeBuilder::blankLine() {
    addNode(BlankLineNode {});
}

auto TreeBuilder::finish() -> ProgramTree {
    // Auto-close any unclosed blocks
    while (!m_stack.empty()) {
        closeBranch();
        if (!m_stack.empty()) {
            auto entry = std::move(m_stack.back());
            m_stack.pop_back();
            addNode(std::move(entry.node));
        }
    }

    // Flush any remaining collected tokens
    statement();

    return ProgramTree { std::move(m_root) };
}

void TreeBuilder::closeToDepth(const std::size_t depth) {
    while (m_stack.size() > depth) {
        closeBranch();
        if (m_stack.size() > depth) {
            auto entry = std::move(m_stack.back());
            m_stack.pop_back();
            addNode(std::move(entry.node));
        }
    }
}

void TreeBuilder::addNode(Node node) {
    if (m_stack.empty()) {
        m_root.push_back(std::move(node));
    } else {
        m_stack.back().node->body.push_back(std::move(node));
    }
}

void TreeBuilder::closeBranch() {
    if (!m_stack.empty() && m_stack.back().isBranch) {
        auto entry = std::move(m_stack.back());
        m_stack.pop_back();
        addNode(std::move(entry.node));
    }
}

auto TreeBuilder::flushTokens() -> StatementNode {
    StatementNode stmt { std::move(m_collected) };
    m_collected.clear();
    return stmt;
}
