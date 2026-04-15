//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ReindentTransform.hpp"
using namespace fbide;
using lexer::KeywordKind;

namespace {

struct LineKeywords {
    KeywordKind first = KeywordKind::None;
    KeywordKind second = KeywordKind::None;
    KeywordKind last = KeywordKind::None;
    bool lastAtEnd = false;
    bool hasColon = false;
};

auto getLineKeywords(const std::vector<const lexer::Token*>& lineTokens) -> LineKeywords {
    LineKeywords result;
    bool trailingContent = false;
    for (const auto* tok : lineTokens) {
        if (tok->kind == lexer::TokenKind::Whitespace || tok->kind == lexer::TokenKind::Newline) {
            continue;
        }
        if (tok->kind == lexer::TokenKind::Comment || tok->kind == lexer::TokenKind::CommentBlock) {
            break;
        }
        if (tok->kind == lexer::TokenKind::Operator && tok->text == ":") {
            result.hasColon = true;
        }
        if (tok->keywordKind == KeywordKind::None || tok->keywordKind == KeywordKind::Other) {
            trailingContent = true;
            continue;
        }
        trailingContent = false;
        if (result.first == KeywordKind::None) {
            result.first = tok->keywordKind;
        } else if (result.second == KeywordKind::None) {
            result.second = tok->keywordKind;
        }
        result.last = tok->keywordKind;
    }
    result.lastAtEnd = !trailingContent && result.last != KeywordKind::None;
    return result;
}

auto opensBlock(const KeywordKind kw) -> bool {
    switch (kw) {
        case KeywordKind::Do:
        case KeywordKind::While:
        case KeywordKind::For:
        case KeywordKind::With:
        case KeywordKind::Scope:
        case KeywordKind::Enum:
        case KeywordKind::Union:
        case KeywordKind::Select:
        case KeywordKind::Asm:
            return true;
        default:
            return false;
    }
}

auto closesBlock(const KeywordKind kw) -> bool {
    switch (kw) {
        case KeywordKind::End:
        case KeywordKind::Loop:
        case KeywordKind::Next:
        case KeywordKind::Wend:
            return true;
        default:
            return false;
    }
}

auto ppOpensBlock(const KeywordKind kw) -> bool {
    switch (kw) {
        case KeywordKind::PpIf:
        case KeywordKind::PpIfDef:
        case KeywordKind::PpIfNDef:
        case KeywordKind::PpMacro:
            return true;
        default:
            return false;
    }
}

auto ppClosesBlock(const KeywordKind kw) -> bool {
    switch (kw) {
        case KeywordKind::PpEndIf:
        case KeywordKind::PpEndMacro:
            return true;
        default:
            return false;
    }
}

auto ppMidBlock(const KeywordKind kw) -> bool {
    switch (kw) {
        case KeywordKind::PpElse:
        case KeywordKind::PpElseIf:
        case KeywordKind::PpElseIfDef:
        case KeywordKind::PpElseIfNDef:
            return true;
        default:
            return false;
    }
}

/// Check if Sub/Function/Constructor/Destructor/Operator is a body definition.
/// Returns false for declarations (Declare ...) and assignments (Function = value).
auto isBodyDefinition(const std::vector<const lexer::Token*>& lineTokens) -> bool {
    bool foundKeyword = false;
    for (const auto* tok : lineTokens) {
        if (tok->kind == lexer::TokenKind::Whitespace) {
            continue;
        }
        if (tok->keywordKind == KeywordKind::Declare) {
            return false;
        }
        switch (tok->keywordKind) {
            case KeywordKind::Sub:
            case KeywordKind::Function:
            case KeywordKind::Constructor:
            case KeywordKind::Destructor:
            case KeywordKind::Operator:
                foundKeyword = true;
                continue;
            default:
                break;
        }
        if (foundKeyword) {
            return tok->kind != lexer::TokenKind::Operator || tok->text != "=";
        }
    }
    return false;
}

/// A line and its trailing newline token (if any).
struct Line {
    std::vector<const lexer::Token*> tokens;
    const lexer::Token* newline = nullptr;
};

/// Skip '#' and optional whitespace, return view of the rest.
auto ppDirectiveRest(std::string_view text) -> std::string_view {
    std::size_t pos = 0;
    if (pos < text.size() && text[pos] == '#') {
        pos++;
    }
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
        pos++;
    }
    return text.substr(pos);
}

} // namespace

auto ReindentTransform::apply(const std::vector<lexer::Token>& tokens) const -> std::vector<lexer::Token> {
    m_pool.clear();
    // Anchored mode splits pp tokens into up to 3 pool entries each
    m_pool.reserve(m_anchorHash ? tokens.size() * 3 : tokens.size());

    // Split into lines, tracking the newline that ends each
    std::vector<Line> lines;
    lines.emplace_back();
    for (const auto& tok : tokens) {
        if (tok.kind == lexer::TokenKind::Newline) {
            lines.back().newline = &tok;
            lines.emplace_back();
        } else {
            lines.back().tokens.push_back(&tok);
        }
    }

    // Indent state
    std::size_t indent = 0;      // total indent (pp + code)
    std::size_t ppIndent = 0;    // portion from preprocessor nesting
    std::vector<std::size_t> ppStack; // saved code indent at each pp block level

    // Build output
    std::vector<lexer::Token> result;
    result.reserve(tokens.size());

    for (const auto& line : lines) {
        // Find first non-whitespace token
        auto contentStart = line.tokens.begin();
        while (contentStart != line.tokens.end() && (*contentStart)->kind == lexer::TokenKind::Whitespace) {
            ++contentStart;
        }

        if (contentStart != line.tokens.end()) {
            bool dedentBefore = false;
            bool indentAfter = false;
            bool isPp = false;

            if ((*contentStart)->kind == lexer::TokenKind::Preprocessor) {
                isPp = true;
                const auto ppKind = (*contentStart)->keywordKind;

                if (ppOpensBlock(ppKind)) {
                    // Save current code indent, then indent
                    ppStack.push_back(indent - ppIndent);
                    indentAfter = true;
                } else if (ppMidBlock(ppKind)) {
                    // Restore code indent from stack top
                    if (!ppStack.empty()) {
                        indent = ppIndent + ppStack.back();
                    }
                    // pp level stays the same: dedent then indent
                    dedentBefore = true;
                    indentAfter = true;
                } else if (ppClosesBlock(ppKind)) {
                    // Restore code indent and pop
                    dedentBefore = true;
                    if (!ppStack.empty()) {
                        const auto savedCode = ppStack.back();
                        ppStack.pop_back();
                        // After dedent, ppIndent decreases by 1
                        // Set indent to what it should be after closing
                        ppIndent--;
                        indent = ppIndent + savedCode;
                        dedentBefore = false; // we set indent directly
                    }
                }
            } else {
                // Code block indentation
                const auto kws = getLineKeywords(line.tokens);
                if (!kws.hasColon) {
                    if (closesBlock(kws.first)) {
                        dedentBefore = true;
                    } else if (kws.first == KeywordKind::Case || kws.first == KeywordKind::ElseIf) {
                        dedentBefore = true;
                        indentAfter = true;
                    } else if (kws.first == KeywordKind::Else && kws.lastAtEnd) {
                        dedentBefore = true;
                        indentAfter = true;
                    } else if (kws.first == KeywordKind::If && kws.last == KeywordKind::Then && kws.lastAtEnd) {
                        indentAfter = true;
                    } else if (kws.first == KeywordKind::Type && kws.second != KeywordKind::As) {
                        indentAfter = true;
                    } else if (kws.first == KeywordKind::Sub
                           || kws.first == KeywordKind::Function
                           || kws.first == KeywordKind::Constructor
                           || kws.first == KeywordKind::Destructor
                           || kws.first == KeywordKind::Operator) {
                        if (isBodyDefinition(line.tokens)) {
                            indentAfter = true;
                        }
                    } else if (opensBlock(kws.first)) {
                        indentAfter = true;
                    }
                }
            }

            if (dedentBefore && indent > 0) {
                indent--;
            }

            // Emit indentation + content
            if (isPp && m_anchorHash) {
                // Anchored # mode: '#' at column 0, indent between # and directive
                const auto& ppTok = **contentStart;

                // Emit '#' as a preprocessor token
                m_pool.emplace_back("#");
                result.push_back({ lexer::TokenKind::Preprocessor, ppTok.keywordKind, m_pool.back() });

                // Emit indent between # and directive (total indent level)
                if (indent > 0) {
                    m_pool.emplace_back(indent * static_cast<std::size_t>(m_tabSize) - 1, ' ');
                    result.push_back({ lexer::TokenKind::Whitespace, lexer::KeywordKind::None, m_pool.back() });
                }

                // Emit the rest of the directive (after '#' and whitespace)
                const auto rest = ppDirectiveRest(ppTok.text);
                if (!rest.empty()) {
                    m_pool.emplace_back(rest);
                    result.push_back({ lexer::TokenKind::Preprocessor, lexer::KeywordKind::PpOther, m_pool.back() });
                }
            } else {
                // Normal mode: indent entire line
                if (indent > 0) {
                    m_pool.emplace_back(indent * static_cast<std::size_t>(m_tabSize), ' ');
                    result.push_back({ lexer::TokenKind::Whitespace, lexer::KeywordKind::None, m_pool.back() });
                }

                // Emit content tokens (skip leading whitespace)
                for (auto it = contentStart; it != line.tokens.end(); ++it) {
                    result.push_back(**it);
                }
            }

            if (indentAfter) {
                if (isPp && ppOpensBlock((*contentStart)->keywordKind)) {
                    ppIndent++;
                }
                indent++;
            }
        }

        // Emit trailing newline
        if (line.newline != nullptr) {
            result.push_back(*line.newline);
        }
    }

    return result;
}
