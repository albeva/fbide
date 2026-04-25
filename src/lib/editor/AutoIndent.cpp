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
#include "editor/lexilla/FBSciLexer.hpp"
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

auto indent::decide(const wxString& prevLine) -> Decision {
    // Run FBSciLexer over the line, then walk style runs via StyleLexer.
    // Use the structural-keyword wordlist so block words (if/then/sub/end
    // /etc.) are styled as Keyword1 — only category that matters for block
    // detection. User's editor wordlist config is irrelevant here.
    MemoryDocument doc;
    // Append newline so FBSciLexer commits trailing comments/strings as the
    // intended style instead of resetting the last char to Default at EOF
    // (its way of preparing the next line). Without this, `If x Then ' c`
    // would mis-classify the trailing `c`.
    const auto withNewline = prevLine + "\n";
    const auto utf8 = withNewline.utf8_str();
    doc.Set(std::string_view { utf8.data(), utf8.length() });
    auto* fb = FBSciLexer::Create();
    fb->WordListSet(0, structuralKeywordsList().c_str());
    fb->Lex(0, doc.Length(), +ThemeCategory::Default, &doc);
    MemoryDocStyledSource src(doc);
    StyleLexer adapter(src);
    const auto tokens = adapter.tokenise();
    fb->Release();

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
