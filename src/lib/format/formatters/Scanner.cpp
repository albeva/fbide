//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Scanner.hpp"
using namespace fbide::format;
using namespace fbide::lexer;

auto Scanner::scan(const std::vector<Token>& tokens) -> ProgramTree {
    Scanner scanner(tokens);
    scanner.run();
    return scanner.m_builder.finish();
}

Scanner::Scanner(const std::vector<Token>& tokens)
    : m_tokens(tokens) {}

void Scanner::run() {
    while (hasMore()) {
        const auto& tkn = current();

        // Skip whitespace
        if (tkn.kind == TokenKind::Whitespace) {
            advance();
            continue;
        }

        // Track blank lines
        if (tkn.kind == TokenKind::Newline) {
            if (m_prevWasNewline) {
                m_builder.blankLine();
            }
            m_prevWasNewline = true;
            advance();
            continue;
        }

        m_prevWasNewline = false;
        processLine();
    }
}

// region ---------- Line collection ----------

void Scanner::processLine() {
    m_segment.clear();

    while (hasMore()) {
        const auto& tkn = current();

        // Skip whitespace
        if (tkn.kind == TokenKind::Whitespace) {
            advance();
            continue;
        }

        // Newline ends the physical line
        if (tkn.kind == TokenKind::Newline) {
            break;
        }

        // Colon → dispatch current segment, start new one
        if (tkn.operatorKind == OperatorKind::Colon) {
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
            // Skip the newline, continue collecting on next line
            if (hasMore()) {
                advance();
            }
            continue;
        }

        m_segment.push_back(tkn);
        advance();
    }

    dispatch();
}

auto Scanner::isContinuation() const -> bool {
    for (auto j = m_index + 1; j < m_tokens.size(); j++) {
        const auto k = m_tokens[j].kind;
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

void Scanner::dispatch() {
    if (m_segment.empty()) {
        return;
    }

    // Append segment tokens to builder
    for (const auto& tkn : m_segment) {
        m_builder.append(tkn);
    }

    // Preprocessor directives
    if (m_segment[0].kind == TokenKind::Preprocessor) {
        switch (m_segment[0].keywordKind) {
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
    // Rejects: "exit sub", "Function = 10", bare "Sub" without a name.
    // Allows: "Sub Main", "Private Sub Main", "Operator Cast"
    case KeywordKind::Sub:
    case KeywordKind::Function:
    case KeywordKind::Constructor:
    case KeywordKind::Destructor:
    case KeywordKind::Operator:
        if (isBodyDefinition()) {
            m_builder.openBlock();
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
        m_builder.openBlock();
        return;

    // If — multi-line only when Then is the last significant token
    case KeywordKind::If:
        if (lastSignificantKeyword() == KeywordKind::Then) {
            m_builder.openBlock();
        } else {
            m_builder.statement();
        }
        return;

    // Type — block only when NOT followed by As (alias form)
    case KeywordKind::Type: {
        KeywordKind second = KeywordKind::None;
        bool foundFirst = false;
        for (const auto& tkn : m_segment) {
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
            m_builder.openBlock();
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

auto Scanner::firstKeyword() const -> KeywordKind {
    for (const auto& tkn : m_segment) {
        if (tkn.keywordKind != KeywordKind::None && tkn.keywordKind != KeywordKind::Other) {
            return tkn.keywordKind;
        }
    }
    return KeywordKind::None;
}

auto Scanner::lastSignificantKeyword() const -> KeywordKind {
    // Walk backward, skip comments. Return the keywordKind of the last
    // significant token — could be a structural keyword, Other, or None.
    for (auto it = m_segment.rbegin(); it != m_segment.rend(); ++it) {
        if (it->kind == TokenKind::Comment || it->kind == TokenKind::CommentBlock) {
            continue;
        }
        return it->keywordKind;
    }
    return KeywordKind::None;
}

auto Scanner::isBodyDefinition() const -> bool {
    // Check that a callable keyword (Sub/Function/etc.) is followed by a name.
    // Returns false for: "exit sub" (no name after Sub), "Function = 10" (= not a name).
    // Returns true for: "Sub Main", "Private Sub Main", "Operator Cast".
    bool foundKeyword = false;
    for (const auto& tkn : m_segment) {
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
        // After the keyword: identifier or '(' means body definition
        if (tkn.kind == TokenKind::Identifier || tkn.operatorKind == OperatorKind::ParenOpen) {
            return true;
        }
        // Anything else (=, end of line, etc.) means not a definition
        return false;
    }
    return false;
}

// endregion
