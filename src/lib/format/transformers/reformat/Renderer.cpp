//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Renderer.hpp"
using namespace fbide::reformat;
using namespace fbide::parser;
using namespace fbide::lexer;

auto Renderer::render(const ProgramTree& tree) -> std::vector<Token> {
    m_output.clear();
    m_lastWasBlankLine = false;
    m_lastWasBlock = false;
    renderNodes(tree.nodes, m_options.baseIndent);
    return std::move(m_output);
}

void Renderer::renderNodes(const std::vector<Node>& nodes, const std::size_t indent) {
    for (const auto& node : nodes) {
        std::visit(
            Visitor {
                [&](const BlankLineNode&) {
                    // reFormat=true collapses runs of blank lines to one.
                    // reFormat=false emits each blank line to preserve the
                    // original vertical layout.
                    if (m_options.reFormat && m_lastWasBlankLine) {
                        // skip — collapse
                    } else {
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
                [&](const VerbatimNode& region) {
                    // Copy every token's text exactly — no indent prefix,
                    // no spacing normalisation. Original Whitespace and
                    // Newline tokens in the region supply all layout.
                    for (const auto& tok : region.tokens) {
                        m_output.push_back(tok);
                    }
                    m_lastWasBlankLine = false;
                    m_lastWasBlock = false;
                },
                [&](const std::unique_ptr<BlockNode>& block) {
                    const bool branch = isBranch(*block);

                    // Ensure blank line between consecutive definitions.
                    // Suppressed when reFormat=false — user's original vertical
                    // layout takes precedence.
                    if (m_options.reFormat && !branch && m_lastWasBlock && !m_lastWasBlankLine
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
            },
            node
        );
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
    // Find first significant (non-whitespace, non-newline) token.
    std::size_t first = 0;
    while (first < stmt.tokens.size() && isLayout(stmt.tokens[first])) {
        first++;
    }
    if (first == stmt.tokens.size()) {
        return;
    }

    // Anchored hash mode for preprocessor tokens. Only meaningful when we
    // are rebuilding indentation — with reIndent=false we preserve the
    // source's own leading whitespace for PP lines instead.
    if (m_options.anchoredPP && m_options.reIndent
        && stmt.tokens[first].kind == TokenKind::Preprocessor) {
        renderAnchoredPP(stmt, indent, first);
        return;
    }

    emitLeadingIndent(stmt, first, indent);
    emit(stmt.tokens[first]);

    if (m_options.reFormat) {
        const Token* prev = &stmt.tokens[first];
        bool justBrokeLine = false;
        for (std::size_t i = first + 1; i < stmt.tokens.size(); i++) {
            // Preserve `_` line continuations: emit the newline and re-indent
            // so the logical statement spans multiple physical lines like the
            // original. The `_` Comment marker is already in the segment and
            // gets emitted by the regular path on the previous iteration.
            if (stmt.tokens[i].kind == TokenKind::Newline && stmt.tokens[i].continuation) {
                emitNewline();
                emitIndent(indent + 1);
                justBrokeLine = true;
                continue;
            }
            if (isLayout(stmt.tokens[i])) {
                continue;
            }
            if (!justBrokeLine && needsSpaceBefore(*prev, stmt.tokens[i])) {
                emitSpace();
            }
            emit(stmt.tokens[i]);
            prev = &stmt.tokens[i];
            justBrokeLine = false;
        }
        emitNewline();
    } else {
        // Verbatim: echo every remaining token as-is.
        for (std::size_t i = first + 1; i < stmt.tokens.size(); i++) {
            emit(stmt.tokens[i]);
        }
        // Suppress our trailing newline only if the statement already ends with one.
        if (stmt.tokens.back().kind != TokenKind::Newline) {
            emitNewline();
        }
    }
}

void Renderer::emitLeadingIndent(const StatementNode& stmt, const std::size_t first, const std::size_t indent) {
    if (m_options.reIndent) {
        emitIndent(indent);
        return;
    }
    // Echo original leading whitespace verbatim (ignore any stray newlines).
    for (std::size_t i = 0; i < first; i++) {
        if (stmt.tokens[i].kind == TokenKind::Whitespace) {
            emit(stmt.tokens[i]);
        }
    }
}

void Renderer::renderAnchoredPP(const StatementNode& stmt, const std::size_t indent, const std::size_t first) {
    const auto& original = stmt.tokens[first];
    const auto& text = original.text;

    // Build a rewritten directive token with '#' at column 0, then padding
    // and the directive word. Keeps kind/keywordKind so downstream renderers
    // (HTML) can still style it.
    std::string rewritten;
    rewritten.reserve(text.size() + indent * m_options.tabSize);
    rewritten += '#';
    if (indent > 0) {
        rewritten.append(indent * m_options.tabSize - 1, ' ');
    }

    std::size_t pos = 0;
    if (pos < text.size() && text[pos] == '#') {
        pos++;
    }
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
        pos++;
    }
    rewritten.append(text, pos);

    m_output.push_back(Token {
        .kind = TokenKind::Preprocessor,
        .keywordKind = original.keywordKind,
        .style = original.style,
        .text = std::move(rewritten),
    });

    // Emit body tokens (identifiers, strings, numbers, operators, modifier
    // keywords) with the same spacing rules as a normal statement so the
    // line preserves `#define LOG(x) Print x`-style content.
    const Token* prev = &stmt.tokens[first];
    for (std::size_t i = first + 1; i < stmt.tokens.size(); i++) {
        if (isLayout(stmt.tokens[i])) {
            continue;
        }
        if (needsSpaceBefore(*prev, stmt.tokens[i])) {
            emitSpace();
        }
        emit(stmt.tokens[i]);
        prev = &stmt.tokens[i];
    }
    emitNewline();
}

void Renderer::emitIndent(const std::size_t indent) {
    if (indent == 0) {
        return;
    }
    m_output.push_back(Token { .kind = TokenKind::Whitespace, .text = std::string(indent * m_options.tabSize, ' ') });
}

void Renderer::emitSpace() {
    m_output.push_back(Token { .kind = TokenKind::Whitespace, .text = " " });
}

void Renderer::emitNewline() {
    m_output.push_back(Token { .kind = TokenKind::Newline, .text = "\n" });
}

void Renderer::emit(Token token) {
    m_output.push_back(std::move(token));
}

// region ---------- Classification ----------

auto Renderer::isLayout(const Token& token) -> bool {
    return token.kind == TokenKind::Whitespace || token.kind == TokenKind::Newline;
}

auto Renderer::isCompoundAssignKeyword(const Token& token) -> bool {
    if (token.kind != TokenKind::KeywordOperators) {
        return false;
    }
    static constexpr std::array kNames { "and", "or", "xor", "eqv", "imp", "mod", "shl", "shr" };
    std::string lowered;
    lowered.reserve(token.text.size());
    for (const char chr : token.text) {
        lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
    return std::ranges::find(kNames, lowered) != kNames.end();
}

auto Renderer::needsSpaceBefore(const Token& prev, const Token& curr) -> bool {
    using enum OperatorKind;
    const auto prevOp = prev.operatorKind;
    const auto currOp = curr.operatorKind;

    // Comments always have a leading space. The `_` continuation marker is
    // a Comment under FBSciLexer styling and FB syntax requires whitespace
    // before it (`obj._` would parse as a member access on `_`).
    if (curr.kind == TokenKind::Comment) {
        return true;
    }

    // `and=` / `or=` / `xor=` / `eqv=` / `imp=` / `mod=` / `shl=` / `shr=` are
    // single compound-assignment operators spelled as a keyword plus `=`. The
    // `=` hugs the keyword; the keyword-operator rule below would split them.
    if (currOp == Assign && isCompoundAssignKeyword(prev)) {
        return false;
    }

    // Keyword operators (And, Or, Not, Mod, Xor, Shl, Shr) are styled as
    // KeywordOperators by FBSciLexer. They need whitespace on both sides:
    // `x AND (y)` not `x AND(y)`.
    if (prev.kind == TokenKind::KeywordOperators
        || curr.kind == TokenKind::KeywordOperators) {
        return true;
    }

    // A type suffix is part of the name it follows: `x%`, `n&`, `s$`.
    if (currOp == TypeSuffix) {
        return false;
    }

    // `##` pastes macro arguments and reads as a binary operator — `t ## n`.
    // An underscore operand is the exception: `A##_##B` builds the identifier
    // `A_B`, and a detached `_` would read as a line continuation instead.
    if (prevOp == TokenPaste) {
        return curr.text != "_";
    }
    if (currOp == TokenPaste) {
        return prev.text != "_";
    }

    // After ( or [ → no space
    if (prevOp == ParenOpen || prevOp == BracketOpen) {
        return false;
    }

    // Before ) or ] → no space
    if (currOp == ParenClose || currOp == BracketClose) {
        return false;
    }

    // After a unary prefix operator → no space, even before an opening paren:
    // `*(p+1)`, `@(x)`, `-(a+b)`. Also the `#` file-number sigil so it hugs its
    // operand (`#1`, not `# 1`). Must precede the ParenOpen rule below, which
    // would otherwise re-insert a space for the `*(` / `@(` cases.
    if (prevOp == Negate || prevOp == UnaryPlus
        || prevOp == Dereference || prevOp == AddressOf || prevOp == Hash) {
        return false;
    }

    // Before ( or [ → no space only when preceded by identifier/keyword/closing
    // (function call / indexing). After operators, space is needed for grouping.
    // A type suffix counts as part of the name, so `arr%(i)` stays a call.
    if (currOp == ParenOpen || currOp == BracketOpen) {
        return prev.kind == TokenKind::Operator && prevOp != TypeSuffix;
    }

    // Dot and Arrow: no space on either side
    if (prevOp == Dot || prevOp == Arrow || currOp == Dot || currOp == Arrow) {
        return false;
    }

    // Before , or ; → no space. Both are separators: the space goes after.
    if (currOp == Comma || currOp == Semicolon) {
        return false;
    }

    // `Public:` / `Private:` / `Protected:` — the colon belongs to the
    // visibility label. Every other colon is a statement separator and never
    // reaches here (the parser splits on it).
    if (currOp == Colon && isAccessModifier(prev.keywordKind)) {
        return false;
    }

    // Everything else → space
    return true;
}

auto Renderer::isBranch(const BlockNode& block) -> bool {
    if (!block.opener) {
        return false;
    }
    // Skip leading layout tokens (whitespace/newline may appear when
    // scanner preserves original indentation).
    for (const auto& tkn : block.opener->tokens) {
        if (isLayout(tkn)) {
            continue;
        }
        switch (tkn.keywordKind) {
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
    return false;
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
        case KeywordKind::Property:
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
