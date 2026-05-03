//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AutoIndent.hpp"
#include "analyses/lexer/KeywordTables.hpp"
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "analyses/lexer/Token.hpp"
#include "config/ThemeCategory.hpp"
#include "editor/Editor.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
using namespace fbide;
using namespace fbide::lexer;
using namespace fbide::indent;

namespace {

auto isLayout(const TokenKind k) -> bool {
    return k == TokenKind::Whitespace || k == TokenKind::Newline;
}

auto firstKeyword(const std::vector<Token>& tokens) -> KeywordKind {
    // FB reuses keywords inside other statements (e.g. `Open ... For Input As #f`).
    // Indent dispatch must look only at the first word-like token of the line —
    // not scan past it for a structural keyword somewhere later. Access
    // modifiers (`Private` / `Public` / `Protected`) are transparent prefixes
    // of Sub / Function / Type / ... and are skipped so the next word-like
    // token decides. A non-word-like token at the head of the line means there
    // is no leading keyword — return None and the dispatch falls through to a
    // plain statement (no indent change).
    for (const auto& t : tokens) {
        if (isLayout(t.kind) || t.kind == TokenKind::Comment || t.kind == TokenKind::CommentBlock) {
            continue;
        }
        if (!isWordLike(t.kind)) {
            return KeywordKind::None;
        }
        if (t.keywordKind == KeywordKind::AccessModifier) {
            continue;
        }
        return t.keywordKind;
    }
    return KeywordKind::None;
}

auto secondStructuralKeyword(const std::vector<Token>& tokens) -> KeywordKind {
    bool seenFirst = false;
    for (const auto& t : tokens) {
        if (isLayout(t.kind)) {
            continue;
        }
        // Access modifiers (`Public Type Foo As Integer`) are transparent —
        // skip so `Type` registers as the first structural keyword.
        if (t.keywordKind == KeywordKind::AccessModifier) {
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
        switch (it->kind) {
        case TokenKind::Comment:
        case TokenKind::CommentBlock:
        case TokenKind::Invalid:
            continue;
        default:
            break;
        }
        if (isLayout(it->kind)) {
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

// Closer keyword arrays — words stored lowercase; the renderer applies
// case rule. constexpr so spans remain valid forever.
constexpr std::array<std::string_view, 1> kLoop { "loop" };
constexpr std::array<std::string_view, 1> kNext { "next" };
constexpr std::array<std::string_view, 1> kWend { "wend" };
constexpr std::array<std::string_view, 2> kEndIf { "end", "if" };
constexpr std::array<std::string_view, 2> kEndSub { "end", "sub" };
constexpr std::array<std::string_view, 2> kEndFunction { "end", "function" };
constexpr std::array<std::string_view, 2> kEndCtor { "end", "constructor" };
constexpr std::array<std::string_view, 2> kEndDtor { "end", "destructor" };
constexpr std::array<std::string_view, 2> kEndOperator { "end", "operator" };
constexpr std::array<std::string_view, 2> kEndSelect { "end", "select" };
constexpr std::array<std::string_view, 2> kEndType { "end", "type" };
constexpr std::array<std::string_view, 2> kEndEnum { "end", "enum" };
constexpr std::array<std::string_view, 2> kEndUnion { "end", "union" };
constexpr std::array<std::string_view, 2> kEndWith { "end", "with" };
constexpr std::array<std::string_view, 2> kEndNS { "end", "namespace" };
constexpr std::array<std::string_view, 2> kEndScope { "end", "scope" };
constexpr std::array<std::string_view, 2> kEndAsm { "end", "asm" };

auto closerFor(const KeywordKind k) -> std::span<const std::string_view> {
    switch (k) {
    case KeywordKind::If:
        return kEndIf;
    case KeywordKind::Do:
        return kLoop;
    case KeywordKind::For:
        return kNext;
    case KeywordKind::While:
        return kWend;
    case KeywordKind::Sub:
        return kEndSub;
    case KeywordKind::Function:
        return kEndFunction;
    case KeywordKind::Constructor:
        return kEndCtor;
    case KeywordKind::Destructor:
        return kEndDtor;
    case KeywordKind::Operator:
        return kEndOperator;
    case KeywordKind::Select:
        return kEndSelect;
    case KeywordKind::Type:
        return kEndType;
    case KeywordKind::Enum:
        return kEndEnum;
    case KeywordKind::Union:
        return kEndUnion;
    case KeywordKind::With:
        return kEndWith;
    case KeywordKind::Namespace:
        return kEndNS;
    case KeywordKind::Scope:
        return kEndScope;
    case KeywordKind::Asm:
        return kEndAsm;
    default:
        return {};
    }
}

} // namespace

auto Decision::decide(const std::vector<Token>& tokens) -> Decision {
    const auto first = firstKeyword(tokens);
    if (isCloser(first)) {
        return { 0, true, {} };
    }
    if (isMid(first)) {
        return { 1, true, {} };
    }
    if (isOpener(tokens)) {
        return { 1, false, closerFor(first) };
    }
    return { 0, false, {} };
}

auto Decision::decide(Editor& editor, const int prevLine) -> Decision {
    const auto start = editor.PositionFromLine(prevLine);
    const auto end = editor.GetLineEndPosition(prevLine);

    WxStcStyledSource src { editor };
    StyleLexer adapter(src);
    return decide(adapter.tokenise({ start, end }));
}
