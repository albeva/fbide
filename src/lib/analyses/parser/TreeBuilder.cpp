//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "TreeBuilder.hpp"
using namespace fbide::parser;

void TreeBuilder::append(const lexer::Token& token) {
    m_collected.push_back(token);
}

void TreeBuilder::statement() {
    if (m_collected.empty()) {
        return;
    }
    addNode(flushTokens());
}

void TreeBuilder::openBlock(const bool isPP) {
    auto block = acquireBlock();
    if (!m_collected.empty()) {
        block->opener = flushTokens();
    }
    m_stack.push_back({ std::move(block), false, isPP });
}

void TreeBuilder::openBranch() {
    closeBranch();

    auto branch = acquireBlock();
    if (!m_collected.empty()) {
        branch->opener = flushTokens();
    }
    m_stack.push_back({ std::move(branch), true });
}

void TreeBuilder::closeBlock() {
    closeBranch();

    if (m_stack.empty() || m_stack.back().isPP) {
        // Unmatched closer or would close a PP block — emit as statement
        statement();
        return;
    }

    popBlock();
}

void TreeBuilder::closePPBlock() {
    closeBranch();

    if (m_stack.empty()) {
        statement();
        return;
    }

    popBlock();
}

void TreeBuilder::popBlock() {
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

void TreeBuilder::verbatim(std::vector<lexer::Token> tokens) {
    addNode(VerbatimNode { std::move(tokens) });
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
    BlockNode* const parent = m_stack.empty() ? nullptr : m_stack.back().node.get();
    if (auto* slot = std::get_if<std::unique_ptr<BlockNode>>(&node); slot != nullptr && *slot != nullptr) {
        (*slot)->parent = parent; // wire up-traversal as the block attaches to its parent
    }
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
    // Refill from a recycled buffer so the next statement reuses capacity
    // instead of allocating; empty (fresh alloc) when the pool is dry.
    if (!m_tokenPool.empty()) {
        m_collected = std::move(m_tokenPool.back());
        m_tokenPool.pop_back();
    }
    return stmt;
}

void TreeBuilder::reclaim(ProgramTree&& old) {
    for (auto& node : old.nodes) {
        reclaimNode(node);
    }
    old.nodes.clear();
}

void TreeBuilder::reclaimNode(Node& node) {
    if (auto* stmt = std::get_if<StatementNode>(&node)) {
        recycleTokens(std::move(stmt->tokens));
        return;
    }
    if (auto* verb = std::get_if<VerbatimNode>(&node)) {
        recycleTokens(std::move(verb->tokens));
        return;
    }
    auto* slot = std::get_if<std::unique_ptr<BlockNode>>(&node);
    if (slot == nullptr || *slot == nullptr) {
        return; // a blank line — nothing to recycle
    }
    auto block = std::move(*slot);
    if (block->opener) {
        recycleTokens(std::move(block->opener->tokens));
    }
    if (block->closer) {
        recycleTokens(std::move(block->closer->tokens));
    }
    for (auto& child : block->body) {
        reclaimNode(child);
    }
    block->opener.reset();
    block->closer.reset();
    block->body.clear(); // keep the vector's capacity for reuse
    block->parent = nullptr;
    m_pool.push_back(std::move(block));
}

void TreeBuilder::recycleTokens(std::vector<lexer::Token>&& toks) {
    if (toks.capacity() == 0) {
        return; // nothing worth keeping
    }
    toks.clear();
    m_tokenPool.push_back(std::move(toks));
}

void TreeBuilder::reset() {
    // Keep vector capacities and both recycle pools; just drop the contents.
    m_collected.clear();
    m_stack.clear();
    m_root.clear();
    m_pool.clear();
    m_tokenPool.clear();
}

auto TreeBuilder::acquireBlock() -> std::unique_ptr<BlockNode> {
    if (!m_pool.empty()) {
        auto node = std::move(m_pool.back());
        m_pool.pop_back();
        return node; // already reset by reclaimNode
    }
    return std::make_unique<BlockNode>();
}
