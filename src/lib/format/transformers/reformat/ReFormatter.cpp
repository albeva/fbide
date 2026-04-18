//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ReFormatter.hpp"
#include "Renderer.hpp"
using namespace fbide::reformat;
using namespace fbide::lexer;

auto ReFormatter::apply(const std::vector<Token>& tokens) -> std::vector<Token> {
    const auto tree = buildTree(tokens);
    Renderer renderer(m_options);
    return renderer.render(tree);
}

auto ReFormatter::buildTree(const std::vector<Token>& tokens) -> ProgramTree {
    // Reset per-invocation state
    m_tokens = &tokens;
    m_index = 0;
    m_prevWasNewline = true;
    m_builder = TreeBuilder {};
    m_segment.clear();
    m_ppDepths.clear();

    while (hasMore()) {
        step();
    }
    return m_builder.finish();
}

void ReFormatter::step() {
    const auto& tkn = current();

    // Whitespace at the top level: peek past it. If followed by a newline
    // (or EOF), it is part of a blank/whitespace-only line and we skip it.
    // Otherwise, leave it in place so processLine() captures it as the
    // statement's leading indent.
    if (tkn.kind == TokenKind::Whitespace) {
        std::size_t j = m_index + 1;
        while (j < m_tokens->size() && (*m_tokens)[j].kind == TokenKind::Whitespace) {
            j++;
        }
        if (j == m_tokens->size() || (*m_tokens)[j].kind == TokenKind::Newline) {
            m_index = j;
            return;
        }
        m_prevWasNewline = false;
        processLine();
        return;
    }

    // Track blank lines
    if (tkn.kind == TokenKind::Newline) {
        if (m_prevWasNewline) {
            m_builder.blankLine();
        }
        m_prevWasNewline = true;
        advance();
        return;
    }

    m_prevWasNewline = false;
    processLine();
}

// region ---------- Line collection ----------

void ReFormatter::processLine() {
    m_segment.clear();

    while (hasMore()) {
        const auto& tkn = current();

        // Newline ends the physical line
        if (tkn.kind == TokenKind::Newline) {
            break;
        }

        // Colon → dispatch current segment, start new one.
        // Under reFormat=false the colon stays as a regular token in the
        // segment so the original inline layout survives.
        if (tkn.operatorKind == OperatorKind::Colon && m_options.reFormat) {
            dispatch();
            m_segment.clear();
            advance();
            continue;
        }

        // Line continuation: standalone _ at end of line
        if (tkn.kind == TokenKind::Identifier && tkn.text == "_" && isContinuation()) {
            // Include _ and trailing tokens (comments etc.) for preservation
            m_segment.push_back(tkn);
            advance();
            while (hasMore() && current().kind != TokenKind::Newline) {
                m_segment.push_back(current());
                advance();
            }
            // Capture the continuation newline so verbatim rendering can echo
            // the original line break. Renderer filters it out in reFormat=true.
            if (hasMore()) {
                m_segment.push_back(current());
                advance();
            }
            continue;
        }

        m_segment.push_back(tkn);
        advance();
    }

    dispatch();
}

auto ReFormatter::isContinuation() const -> bool {
    for (auto j = m_index + 1; j < m_tokens->size(); j++) {
        const auto k = (*m_tokens)[j].kind;
        if (k == TokenKind::Newline) {
            return true;
        }
        if (k == TokenKind::Whitespace || k == TokenKind::Comment || k == TokenKind::CommentBlock) {
            continue;
        }
        return false;
    }
    return true; // end of input
}

// endregion

// region ---------- Dispatch ----------

void ReFormatter::dispatch() {
    if (m_segment.empty()) {
        return;
    }

    // Append segment tokens to builder
    for (const auto& tkn : m_segment) {
        m_builder.append(tkn);
    }

    // Find the first significant token (skipping whitespace/newlines)
    std::size_t firstIdx = 0;
    while (firstIdx < m_segment.size()
           && (m_segment[firstIdx].kind == TokenKind::Whitespace
               || m_segment[firstIdx].kind == TokenKind::Newline)) {
        firstIdx++;
    }
    if (firstIdx == m_segment.size()) {
        // Segment is all layout — nothing structural to dispatch.
        // Flush it as a statement so the layout tokens survive for verbatim render.
        m_builder.statement();
        return;
    }

    // Preprocessor directives
    if (m_segment[firstIdx].kind == TokenKind::Preprocessor) {
        switch (m_segment[firstIdx].keywordKind) {
        case KeywordKind::PpIf:
        case KeywordKind::PpIfDef:
        case KeywordKind::PpIfNDef:
        case KeywordKind::PpMacro:
            m_ppDepths.push_back(m_builder.stackDepth());
            m_builder.openBlock(true);
            return;
        case KeywordKind::PpEndIf:
        case KeywordKind::PpEndMacro:
            // Auto-close any unclosed code blocks inside this PP scope
            if (!m_ppDepths.empty()) {
                m_builder.closeToDepth(m_ppDepths.back() + 1);
                m_ppDepths.pop_back();
            }
            m_builder.closePPBlock();
            return;
        case KeywordKind::PpElse:
        case KeywordKind::PpElseIf:
        case KeywordKind::PpElseIfDef:
        case KeywordKind::PpElseIfNDef:
            // Auto-close any unclosed code blocks inside this PP scope
            if (!m_ppDepths.empty()) {
                m_builder.closeToDepth(m_ppDepths.back() + 1);
            }
            m_builder.openBranch();
            return;
        default:
            m_builder.statement();
            return;
        }
    }

    // Keyword dispatch based on first structural keyword
    switch (firstKeyword()) {
    // Callable block openers — only when followed by a name (body definition).
    case KeywordKind::Sub:
    case KeywordKind::Function:
    case KeywordKind::Constructor:
    case KeywordKind::Destructor:
    case KeywordKind::Operator:
        if (isBodyDefinition()) {
            openBlockOrStatement();
        } else {
            m_builder.statement();
        }
        return;

    case KeywordKind::Do:
    case KeywordKind::While:
    case KeywordKind::For:
    case KeywordKind::With:
    case KeywordKind::Scope:
    case KeywordKind::Enum:
    case KeywordKind::Union:
    case KeywordKind::Asm:
    case KeywordKind::Namespace:
    case KeywordKind::Select:
        openBlockOrStatement();
        return;

    // If — multi-line only when Then is the last significant token
    case KeywordKind::If:
        if (lastSignificantKeyword() == KeywordKind::Then) {
            openBlockOrStatement();
        } else {
            m_builder.statement();
        }
        return;

    // Type — block only when NOT followed by As (alias form)
    case KeywordKind::Type: {
        KeywordKind second = KeywordKind::None;
        bool foundFirst = false;
        for (const auto& tkn : m_segment) {
            if (tkn.kind == TokenKind::Whitespace || tkn.kind == TokenKind::Newline) {
                continue;
            }
            if (tkn.keywordKind != KeywordKind::None && tkn.keywordKind != KeywordKind::Other) {
                if (foundFirst) {
                    second = tkn.keywordKind;
                    break;
                }
                foundFirst = true;
            }
        }
        if (second == KeywordKind::As) {
            m_builder.statement();
        } else {
            openBlockOrStatement();
        }
        return;
    }

    // Block closers — TreeBuilder won't close past PP boundaries
    case KeywordKind::End:
    case KeywordKind::Loop:
    case KeywordKind::Next:
    case KeywordKind::Wend:
        m_builder.closeBlock();
        return;

    // Mid-block → branch
    case KeywordKind::Else:
    case KeywordKind::ElseIf:
    case KeywordKind::Case:
        m_builder.openBranch();
        return;

    // Declare consumes the line (prevents Sub/Function from opening a block)
    case KeywordKind::Declare:
        m_builder.statement();
        return;

    // Everything else
    default:
        m_builder.statement();
        return;
    }
}

auto ReFormatter::firstKeyword() const -> KeywordKind {
    for (const auto& tkn : m_segment) {
        if (tkn.kind == TokenKind::Whitespace || tkn.kind == TokenKind::Newline) {
            continue;
        }
        if (tkn.keywordKind != KeywordKind::None && tkn.keywordKind != KeywordKind::Other) {
            return tkn.keywordKind;
        }
    }
    return KeywordKind::None;
}

auto ReFormatter::lastSignificantKeyword() const -> KeywordKind {
    // Walk backward, skip comments and layout tokens. Return the keywordKind
    // of the last significant token — could be structural, Other, or None.
    for (auto it = m_segment.rbegin(); it != m_segment.rend(); ++it) {
        if (it->kind == TokenKind::Comment || it->kind == TokenKind::CommentBlock
            || it->kind == TokenKind::Whitespace || it->kind == TokenKind::Newline) {
            continue;
        }
        return it->keywordKind;
    }
    return KeywordKind::None;
}

void ReFormatter::openBlockOrStatement() {
    // Under reFormat=false a physical line can contain both opener and
    // closer (e.g. `For i = 1 To 10 : Print i : Next`). Such lines are
    // self-contained and should not push a block onto the stack.
    if (!m_options.reFormat && hasBlockCloserAfterFirst()) {
        m_builder.statement();
    } else {
        m_builder.openBlock();
    }
}

auto ReFormatter::hasBlockCloserAfterFirst() const -> bool {
    bool seenFirstStructural = false;
    for (const auto& tkn : m_segment) {
        if (tkn.kind == TokenKind::Whitespace || tkn.kind == TokenKind::Newline) {
            continue;
        }
        if (!seenFirstStructural) {
            if (tkn.keywordKind != KeywordKind::None && tkn.keywordKind != KeywordKind::Other) {
                seenFirstStructural = true;
            }
            continue;
        }
        switch (tkn.keywordKind) {
        case KeywordKind::End:
        case KeywordKind::Next:
        case KeywordKind::Loop:
        case KeywordKind::Wend:
            return true;
        default:
            break;
        }
    }
    return false;
}

auto ReFormatter::isBodyDefinition() const -> bool {
    // Check that a callable keyword (Sub/Function/etc.) is followed by a name.
    // Returns false for: "exit sub" (no name after Sub), "Function = 10" (= not a name).
    // Returns true for: "Sub Main", "Private Sub Main", "Operator Cast", "Function Add(...)".
    bool foundKeyword = false;
    for (const auto& tkn : m_segment) {
        if (tkn.kind == TokenKind::Whitespace || tkn.kind == TokenKind::Newline) {
            continue;
        }
        if (!foundKeyword) {
            switch (tkn.keywordKind) {
            case KeywordKind::Sub:
            case KeywordKind::Function:
            case KeywordKind::Constructor:
            case KeywordKind::Destructor:
            case KeywordKind::Operator:
                foundKeyword = true;
                continue;
            default:
                continue;
            }
        }
        // After the keyword: any word-like token (identifier or keyword group)
        // or '(' means a name follows — this is a body definition. The name
        // may shadow a keyword (e.g. `Function Add(...)`), so accept any
        // Keyword1-4 here as well as Identifier.
        switch (tkn.kind) {
        case TokenKind::Identifier:
        case TokenKind::Keyword1:
        case TokenKind::Keyword2:
        case TokenKind::Keyword3:
        case TokenKind::Keyword4:
            return true;
        default:
            break;
        }
        if (tkn.operatorKind == OperatorKind::ParenOpen) {
            return true;
        }
        // Anything else (=, end of line, etc.) means not a definition
        return false;
    }
    return false;
}

// endregion
