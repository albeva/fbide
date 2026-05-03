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

namespace {

/// Drop layout/comment noise; collapse blank-line runs to one Newline.
/// Verbatim regions pass through unchanged so `' format off` content stays
/// as it was.
auto leanFilter(const std::vector<Token>& tokens) -> std::vector<Token> {
    std::vector<Token> result;
    result.reserve(tokens.size());
    bool prevNewline = true; // treat start as if a newline preceded
    for (const auto& tkn : tokens) {
        if (tkn.verbatim) {
            result.push_back(tkn);
            prevNewline = (tkn.kind == TokenKind::Newline);
            continue;
        }
        switch (tkn.kind) {
        case TokenKind::Whitespace:
        case TokenKind::Comment:
        case TokenKind::CommentBlock:
            continue;
        case TokenKind::Newline:
            if (prevNewline) {
                continue;
            }
            prevNewline = true;
            result.push_back(tkn);
            continue;
        default:
            prevNewline = false;
            result.push_back(tkn);
        }
    }
    return result;
}

} // namespace

auto ReFormatter::apply(const std::vector<Token>& tokens) -> std::vector<Token> {
    const auto tree = buildTree(tokens);
    Renderer renderer(m_options);
    return renderer.render(tree);
}

auto ReFormatter::buildTree(const std::vector<Token>& tokens) -> ProgramTree {
    // Reset per-invocation state
    std::vector<Token> filtered;
    if (m_options.lean) {
        filtered = leanFilter(tokens);
        m_tokens = &filtered;
    } else {
        m_tokens = &tokens;
    }
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

    // Verbatim region: lexer marked these tokens as inside `' format off`.
    // Collect the whole contiguous run (including Whitespace/Newline tokens
    // inside, which carry the region's layout) and hand it to the builder
    // as an opaque VerbatimNode. Skip structural dispatch entirely.
    if (tkn.verbatim) {
        std::vector<lexer::Token> run;
        while (hasMore() && current().verbatim) {
            run.push_back(current());
            advance();
        }
        m_builder.verbatim(std::move(run));
        // The trailing Newline belonging to the `' format on` line is
        // typically included in the verbatim run. Resume blank-line
        // tracking as though we just crossed a newline.
        m_prevWasNewline = true;
        return;
    }

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

        // Newline normally ends the physical line. Continuation newlines
        // (preceding line ended with `_`, FBSciLexer set continueLine state)
        // keep the logical statement open across the break — the lexer marks
        // these via Token::continuation, no text heuristics needed.
        if (tkn.kind == TokenKind::Newline) {
            if (tkn.continuation) {
                m_segment.push_back(tkn);
                advance();
                continue;
            }
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

        m_segment.push_back(tkn);
        advance();
    }

    dispatch();
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
            // Access modifiers (`Public Type Foo As Integer`) are transparent —
            // skip so `Type` registers as the first structural keyword and the
            // following `As` registers as the second.
            if (tkn.keywordKind == KeywordKind::AccessModifier) {
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
    // FB reuses keywords inside other statements (e.g. `Open ... For Input As #f`).
    // Block dispatch must look only at the first word-like token of the line —
    // not scan past it for a structural keyword somewhere later. Access
    // modifiers (`Private` / `Public` / `Protected`) are transparent prefixes
    // of Sub / Function / Type / ... and are skipped so the next word-like
    // token decides. A non-word-like token at the head of the line means there
    // is no leading keyword — return None and the dispatch falls through to a
    // plain statement.
    for (const auto& tkn : m_segment) {
        if (tkn.kind == TokenKind::Whitespace || tkn.kind == TokenKind::Newline
            || tkn.kind == TokenKind::Comment || tkn.kind == TokenKind::CommentBlock) {
            continue;
        }
        if (!isWordLike(tkn.kind)) {
            return KeywordKind::None;
        }
        if (tkn.keywordKind == KeywordKind::AccessModifier) {
            continue;
        }
        return tkn.keywordKind;
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
        if (isWordLike(tkn.kind)) {
            return true;
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
