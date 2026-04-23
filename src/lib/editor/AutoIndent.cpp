//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AutoIndent.hpp"
#include <span>
#include "analyses/lexer/Lexer.hpp"
#include "analyses/lexer/Token.hpp"
using namespace fbide;
using namespace fbide::lexer;

namespace {

auto isLayout(const TokenKind k) -> bool {
    return k == TokenKind::Whitespace || k == TokenKind::Newline;
}

auto firstKeyword(const std::vector<Token>& tokens) -> KeywordKind {
    for (const auto& t : tokens) {
        if (isLayout(t.kind)) {
            continue;
        }
        if (t.keywordKind != KeywordKind::None && t.keywordKind != KeywordKind::Other) {
            return t.keywordKind;
        }
    }
    return KeywordKind::None;
}

auto secondStructuralKeyword(const std::vector<Token>& tokens) -> KeywordKind {
    bool seenFirst = false;
    for (const auto& t : tokens) {
        if (isLayout(t.kind)) {
            continue;
        }
        if (t.keywordKind != KeywordKind::None && t.keywordKind != KeywordKind::Other) {
            if (seenFirst) {
                return t.keywordKind;
            }
            seenFirst = true;
        }
    }
    return KeywordKind::None;
}

auto lastSignificantKeyword(const std::vector<Token>& tokens) -> KeywordKind {
    for (auto it = tokens.rbegin(); it != tokens.rend(); ++it) {
        if (it->kind == TokenKind::Comment || it->kind == TokenKind::CommentBlock || isLayout(it->kind)) {
            continue;
        }
        return it->keywordKind;
    }
    return KeywordKind::None;
}

auto hasBlockCloserAfterFirst(const std::vector<Token>& tokens) -> bool {
    bool seenFirst = false;
    for (const auto& t : tokens) {
        if (isLayout(t.kind)) {
            continue;
        }
        if (!seenFirst) {
            if (t.keywordKind != KeywordKind::None && t.keywordKind != KeywordKind::Other) {
                seenFirst = true;
            }
            continue;
        }
        switch (t.keywordKind) {
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

auto isBodyDefinition(const std::vector<Token>& tokens) -> bool {
    bool foundKw = false;
    for (const auto& t : tokens) {
        if (isLayout(t.kind)) {
            continue;
        }
        if (!foundKw) {
            switch (t.keywordKind) {
            case KeywordKind::Sub:
            case KeywordKind::Function:
            case KeywordKind::Constructor:
            case KeywordKind::Destructor:
            case KeywordKind::Operator:
                foundKw = true;
                continue;
            default:
                continue;
            }
        }
        if (isWordLike(t.kind)) {
            return true;
        }
        if (t.operatorKind == OperatorKind::ParenOpen) {
            return true;
        }
        return false;
    }
    return false;
}

auto isCloser(const KeywordKind first) -> bool {
    return first == KeywordKind::End
        || first == KeywordKind::Loop
        || first == KeywordKind::Next
        || first == KeywordKind::Wend;
}

auto isMid(const KeywordKind first) -> bool {
    return first == KeywordKind::Else
        || first == KeywordKind::ElseIf
        || first == KeywordKind::Case;
}

auto isOpener(const std::vector<Token>& tokens) -> bool {
    if (hasBlockCloserAfterFirst(tokens)) {
        return false;
    }
    switch (firstKeyword(tokens)) {
    case KeywordKind::Sub:
    case KeywordKind::Function:
    case KeywordKind::Constructor:
    case KeywordKind::Destructor:
    case KeywordKind::Operator:
        return isBodyDefinition(tokens);
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
        return true;
    case KeywordKind::If:
        return lastSignificantKeyword(tokens) == KeywordKind::Then;
    case KeywordKind::Type:
        return secondStructuralKeyword(tokens) != KeywordKind::As;
    default:
        return false;
    }
}

/// Canonical closing keyword for a given opener kind. Returns nullopt for
/// keywords that don't open a block (caller filters via isOpener first).
auto closerFor(const KeywordKind k) -> std::optional<wxString> {
    switch (k) {
    case KeywordKind::If:          return wxString { "End If" };
    case KeywordKind::Do:          return wxString { "Loop" };
    case KeywordKind::For:         return wxString { "Next" };
    case KeywordKind::While:       return wxString { "Wend" };
    case KeywordKind::Sub:         return wxString { "End Sub" };
    case KeywordKind::Function:    return wxString { "End Function" };
    case KeywordKind::Constructor: return wxString { "End Constructor" };
    case KeywordKind::Destructor:  return wxString { "End Destructor" };
    case KeywordKind::Operator:    return wxString { "End Operator" };
    case KeywordKind::Select:      return wxString { "End Select" };
    case KeywordKind::Type:        return wxString { "End Type" };
    case KeywordKind::Enum:        return wxString { "End Enum" };
    case KeywordKind::Union:       return wxString { "End Union" };
    case KeywordKind::With:        return wxString { "End With" };
    case KeywordKind::Namespace:   return wxString { "End Namespace" };
    case KeywordKind::Scope:       return wxString { "End Scope" };
    case KeywordKind::Asm:         return wxString { "End Asm" };
    default:                       return std::nullopt;
    }
}

} // namespace

auto indent::decide(const wxString& prevLine) -> Decision {
    Lexer lex(std::span<const KeywordGroup> {});
    const auto utf8 = prevLine.utf8_str();
    const auto tokens = lex.tokenise(utf8.data());

    const auto first = firstKeyword(tokens);
    if (isCloser(first)) {
        return { 0, true, std::nullopt };
    }
    if (isMid(first)) {
        return { 1, true, std::nullopt };
    }
    if (isOpener(tokens)) {
        return { 1, false, closerFor(first) };
    }
    return { 0, false, std::nullopt };
}
