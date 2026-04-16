//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Renderer.hpp"
using namespace fbide::format;
using namespace fbide::lexer;

auto Renderer::render(const ProgramTree& tree) -> std::string {
    m_output.clear();
    m_lastWasBlankLine = false;
    m_lastWasBlock = false;
    renderNodes(tree.nodes, 0);
    return std::move(m_output);
}

void Renderer::renderNodes(const std::vector<Node>& nodes, const std::size_t indent) {
    for (const auto& node : nodes) {
        std::visit(overloaded {
            [&](const BlankLineNode&) {
                // Collapse multiple blank lines to 1
                if (!m_lastWasBlankLine) {
                    emitNewline();
                    m_lastWasBlankLine = true;
                }
                m_lastWasBlock = false;
            },
            [&](const StatementNode& stmt) {
                renderStatement(stmt, indent);
                m_lastWasBlankLine = false;
                m_lastWasBlock = false;
            },
            [&](const std::unique_ptr<BlockNode>& block) {
                const bool branch = isBranch(*block);

                // Ensure blank line between consecutive definitions
                if (!branch && m_lastWasBlock && !m_lastWasBlankLine
                    && isDefinition(*block)) {
                    emitNewline();
                }

                if (branch && indent > 0) {
                    renderBlock(*block, indent - 1);
                } else {
                    renderBlock(*block, indent);
                }
                m_lastWasBlankLine = false;
                m_lastWasBlock = !branch;
            },
        }, node);
    }
}

void Renderer::renderBlock(const BlockNode& block, const std::size_t indent) {
    if (block.opener) {
        renderStatement(*block.opener, indent);
    }

    renderNodes(block.body, indent + 1);

    if (block.closer) {
        renderStatement(*block.closer, indent);
    }
}

void Renderer::renderStatement(const StatementNode& stmt, const std::size_t indent) {
    if (stmt.tokens.empty()) {
        return;
    }

    // Anchored hash mode for preprocessor tokens
    if (m_options.anchoredPP && stmt.tokens[0].kind == TokenKind::Preprocessor) {
        renderAnchoredPP(stmt, indent);
        return;
    }

    emitIndent(indent);
    m_output += stmt.tokens[0].text;

    for (std::size_t i = 1; i < stmt.tokens.size(); i++) {
        if (needsSpaceBefore(stmt.tokens[i - 1], stmt.tokens[i])) {
            m_output += ' ';
        }
        m_output += stmt.tokens[i].text;
    }

    emitNewline();
}

void Renderer::renderAnchoredPP(const StatementNode& stmt, const std::size_t indent) {
    const auto& text = stmt.tokens[0].text;

    // Emit '#' at column 0
    m_output += '#';

    // Indent between '#' and directive word
    if (indent > 0) {
        m_output.append(indent * m_options.tabSize - 1, ' ');
    }

    // Skip '#' and optional whitespace in original token text
    std::size_t pos = 0;
    if (pos < text.size() && text[pos] == '#') {
        pos++;
    }
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
        pos++;
    }

    m_output += text.substr(pos);
    emitNewline();
}

void Renderer::emitIndent(const std::size_t indent) {
    m_output.append(indent * m_options.tabSize, ' ');
}

void Renderer::emitNewline() {
    m_output += '\n';
}

// region ---------- Classification ----------

auto Renderer::needsSpaceBefore(const Token& prev, const Token& curr) -> bool {
    using enum OperatorKind;
    const auto prevOp = prev.operatorKind;
    const auto currOp = curr.operatorKind;

    // After ( or [ → no space
    if (prevOp == ParenOpen || prevOp == BracketOpen) {
        return false;
    }

    // Before ) or ] → no space
    if (currOp == ParenClose || currOp == BracketClose) {
        return false;
    }

    // Before ( or [ → no space only when preceded by identifier/keyword/closing
    // (function call / indexing). After operators, space is needed for grouping.
    if (currOp == ParenOpen || currOp == BracketOpen) {
        return prev.kind == TokenKind::Operator;
    }

    // Dot and Arrow: no space on either side
    if (prevOp == Dot || prevOp == Arrow || currOp == Dot || currOp == Arrow) {
        return false;
    }

    // Before , → no space
    if (currOp == Comma) {
        return false;
    }

    // After unary operator → no space
    if (prevOp == Negate || prevOp == UnaryPlus
        || prevOp == Dereference || prevOp == AddressOf) {
        return false;
    }

    // Everything else → space
    return true;
}

auto Renderer::isBranch(const BlockNode& block) -> bool {
    if (!block.opener || block.opener->tokens.empty()) {
        return false;
    }
    switch (block.opener->tokens[0].keywordKind) {
    case KeywordKind::Else:
    case KeywordKind::ElseIf:
    case KeywordKind::Case:
    case KeywordKind::PpElse:
    case KeywordKind::PpElseIf:
    case KeywordKind::PpElseIfDef:
    case KeywordKind::PpElseIfNDef:
        return true;
    default:
        return false;
    }
}

auto Renderer::isDefinition(const BlockNode& block) -> bool {
    if (!block.opener || block.opener->tokens.empty()) {
        return false;
    }
    // Check the first structural keyword in the opener
    for (const auto& tkn : block.opener->tokens) {
        if (tkn.keywordKind == KeywordKind::None || tkn.keywordKind == KeywordKind::Other) {
            continue;
        }
        switch (tkn.keywordKind) {
        case KeywordKind::Sub:
        case KeywordKind::Function:
        case KeywordKind::Constructor:
        case KeywordKind::Destructor:
        case KeywordKind::Operator:
        case KeywordKind::Type:
        case KeywordKind::Enum:
        case KeywordKind::Union:
        case KeywordKind::Namespace:
            return true;
        default:
            return false;
        }
    }
    return false;
}

// endregion
