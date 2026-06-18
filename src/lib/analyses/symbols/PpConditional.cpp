//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "PpConditional.hpp"
using namespace fbide;
using fbide::lexer::KeywordKind;
using fbide::lexer::OperatorKind;
using fbide::lexer::Token;
using fbide::lexer::TokenKind;

namespace {

auto isLayout(const Token& tok) -> bool {
    return tok.kind == TokenKind::Whitespace || tok.kind == TokenKind::Newline
        || tok.kind == TokenKind::Comment || tok.kind == TokenKind::CommentBlock;
}

auto toLower(std::string str) -> std::string {
    std::ranges::transform(str, str.begin(), [](unsigned char chr) { return static_cast<char>(std::tolower(chr)); });
    return str;
}

/// Case-insensitive comparison of a token's text against a lowercase keyword.
auto textIs(const Token& tok, const std::string_view lowerKeyword) -> bool {
    if (tok.text.size() != lowerKeyword.size()) {
        return false;
    }
    for (std::size_t idx = 0; idx < lowerKeyword.size(); idx++) {
        if (std::tolower(static_cast<unsigned char>(tok.text[idx])) != lowerKeyword[idx]) {
            return false;
        }
    }
    return true;
}

/// A FreeBASIC integer literal is zero iff every digit (after an optional
/// &H / &O / &B radix prefix) is '0'. `#if` takes an integer constant, so there
/// are no float forms to consider here.
auto numericLiteralIsZero(std::string_view text) -> bool {
    if (text.size() >= 2 && text.front() == '&') {
        text.remove_prefix(2);
    }
    return !text.empty() && std::ranges::all_of(text, [](const char ch) { return ch == '0'; });
}

auto isBinaryOp(const Token& tok) -> bool {
    return textIs(tok, "and") || textIs(tok, "or") || textIs(tok, "andalso") || textIs(tok, "orelse");
}

auto andOp(const PpEval lhs, const PpEval rhs) -> PpEval {
    if (lhs == PpEval::False || rhs == PpEval::False) {
        return PpEval::False;
    }
    if (lhs == PpEval::True && rhs == PpEval::True) {
        return PpEval::True;
    }
    return PpEval::Unknown;
}

auto orOp(const PpEval lhs, const PpEval rhs) -> PpEval {
    if (lhs == PpEval::True || rhs == PpEval::True) {
        return PpEval::True;
    }
    if (lhs == PpEval::False && rhs == PpEval::False) {
        return PpEval::False;
    }
    return PpEval::Unknown;
}

auto notOp(const PpEval val) -> PpEval {
    switch (val) {
    case PpEval::True:
        return PpEval::False;
    case PpEval::False:
        return PpEval::True;
    case PpEval::Unknown:
        return PpEval::Unknown;
    }
    return PpEval::Unknown;
}

/// Built-in *presence* macros FBIde probes at startup (operating system +
/// architecture): each is defined only on its target, so an absent one here is
/// definitively undefined. Value macros (`__FB_VERSION__`, `__FB_OUT_*__`,
/// `__FB_OPTION_*__`, …) are deliberately excluded — they are always defined, so
/// a branch gated on their *value* can't be resolved without value evaluation
/// and must stay Unknown. Keep in sync with resources/ide/fbc-defines.bas.
constexpr std::string_view kKnownBuiltins[] = {
    "__fb_unix__", "__fb_linux__", "__fb_win32__", "__fb_dos__", "__fb_darwin__",
    "__fb_freebsd__", "__fb_netbsd__", "__fb_openbsd__", "__fb_cygwin__", "__fb_js__",
    "__fb_xbox__", "__fb_android__", "__fb_64bit__", "__fb_arm__", "__fb_bigendian__"
};

/// True when `defines` carries at least one probed built-in presence macro — the
/// signal that the compiler was successfully probed. Without it (no fbc, no
/// probe) the closed-world assumption for the `__FB_*` namespace cannot hold, so
/// absent built-ins stay Unknown rather than being treated as undefined.
auto hasProbedBuiltins(const std::unordered_set<std::string>& defines) -> bool {
    return std::ranges::any_of(defines, [](const std::string& def) {
        return std::ranges::find(kKnownBuiltins, def) != std::ranges::end(kKnownBuiltins);
    });
}

/// Tri-state "is this symbol defined?": present in the set (compiler -d defines +
/// probed built-ins) → True; a built-in absent from an *unprobed* compiler →
/// Unknown (its target is unknown); any other absent symbol → False (undefined).
auto definedState(const std::string& name, const std::unordered_set<std::string>& defines, const bool haveBuiltins)
    -> PpEval {
    const auto lname = toLower(name);
    if (defines.contains(lname)) {
        return PpEval::True;
    }
    if (std::ranges::find(kKnownBuiltins, lname) != std::ranges::end(kKnownBuiltins)) {
        // A probed-but-absent built-in is definitively undefined — but only when
        // we have a probe; otherwise we know nothing, so keep the branch.
        return haveBuiltins ? PpEval::False : PpEval::Unknown;
    }
    // A non-built-in symbol absent from the set is treated as undefined: code
    // `#define`s aren't tracked, so `#ifdef`/`defined()` on such a symbol is
    // false. (An include guard's own `#ifndef` thus stays live, keeping content.)
    return PpEval::False;
}

/// Recursive-descent evaluator over the significant condition tokens. Any token
/// outside the supported grammar sets `bail`, collapsing the result to Unknown.
struct Evaluator {
    const std::vector<const Token*>& tokens;
    const std::unordered_set<std::string>& defines;
    bool haveBuiltins = false;
    std::size_t pos = 0;
    bool bail = false;

    [[nodiscard]] auto peek() const -> const Token* { return pos < tokens.size() ? tokens[pos] : nullptr; }
    void advance() { pos++; }

    [[nodiscard]] auto definedCheck(const std::string& name) const -> PpEval {
        return definedState(name, defines, haveBuiltins);
    }

    auto parsePrimary() -> PpEval {
        const Token* tok = peek();
        if (tok == nullptr) {
            bail = true;
            return PpEval::Unknown;
        }
        if (textIs(*tok, "not")) {
            advance();
            return notOp(parsePrimary());
        }
        if (tok->operatorKind == OperatorKind::ParenOpen) {
            advance();
            const PpEval inner = parseOr();
            if (const Token* close = peek(); close != nullptr && close->operatorKind == OperatorKind::ParenClose) {
                advance();
            } else {
                bail = true;
            }
            return inner;
        }
        if (textIs(*tok, "defined")) {
            advance();
            bool paren = false;
            if (const Token* open = peek(); open != nullptr && open->operatorKind == OperatorKind::ParenOpen) {
                advance();
                paren = true;
            }
            const Token* name = peek();
            if (name == nullptr || !isWordLike(name->kind)) {
                bail = true;
                return PpEval::Unknown;
            }
            const PpEval result = definedCheck(name->text);
            advance();
            if (paren) {
                if (const Token* close = peek(); close != nullptr && close->operatorKind == OperatorKind::ParenClose) {
                    advance();
                } else {
                    bail = true;
                }
            }
            return result;
        }
        // Literal values: `true` / `false` and numeric literals (nonzero → true).
        if (textIs(*tok, "true")) {
            advance();
            return PpEval::True;
        }
        if (textIs(*tok, "false")) {
            advance();
            return PpEval::False;
        }
        if (tok->kind == TokenKind::Number) {
            const PpEval result = numericLiteralIsZero(tok->text) ? PpEval::False : PpEval::True;
            advance();
            return result;
        }
        // A bare symbol is a defined-check (no value semantics). A binary operator
        // here is malformed input.
        if (isWordLike(tok->kind) && !isBinaryOp(*tok)) {
            const PpEval result = definedCheck(tok->text);
            advance();
            return result;
        }
        bail = true;
        advance();
        return PpEval::Unknown;
    }

    auto parseAnd() -> PpEval {
        PpEval result = parsePrimary();
        while (const Token* tok = peek()) {
            if (!textIs(*tok, "and") && !textIs(*tok, "andalso")) {
                break;
            }
            advance();
            result = andOp(result, parsePrimary());
        }
        return result;
    }

    auto parseOr() -> PpEval {
        PpEval result = parseAnd();
        while (const Token* tok = peek()) {
            if (!textIs(*tok, "or") && !textIs(*tok, "orelse")) {
                break;
            }
            advance();
            result = orOp(result, parseAnd());
        }
        return result;
    }
};

} // namespace

auto fbide::evaluatePpCondition(
    const std::vector<Token>& opener,
    const std::unordered_set<std::string>& defines
) -> PpEval {
    std::vector<const Token*> sig;
    for (const auto& tok : opener) {
        if (!isLayout(tok)) {
            sig.push_back(&tok);
        }
    }
    if (sig.empty()) {
        return PpEval::Unknown;
    }

    // The first significant token is the directive itself (e.g. the merged
    // `#ifdef`); the condition is whatever follows.
    const KeywordKind directive = sig.front()->keywordKind;
    const std::vector<const Token*> cond(sig.begin() + 1, sig.end());
    const bool haveBuiltins = hasProbedBuiltins(defines);

    switch (directive) {
    case KeywordKind::PpIfDef:
    case KeywordKind::PpElseIfDef:
        if (cond.empty() || !isWordLike(cond.front()->kind)) {
            return PpEval::Unknown;
        }
        return definedState(cond.front()->text, defines, haveBuiltins);
    case KeywordKind::PpIfNDef:
    case KeywordKind::PpElseIfNDef:
        if (cond.empty() || !isWordLike(cond.front()->kind)) {
            return PpEval::Unknown;
        }
        return notOp(definedState(cond.front()->text, defines, haveBuiltins));
    case KeywordKind::PpIf:
    case KeywordKind::PpElseIf: {
        if (cond.empty()) {
            return PpEval::Unknown;
        }
        Evaluator evaluator { .tokens = cond, .defines = defines, .haveBuiltins = haveBuiltins };
        const PpEval result = evaluator.parseOr();
        // Unhandled or leftover tokens (a value check, comparison, …) → Unknown.
        if (evaluator.bail || evaluator.pos != cond.size()) {
            return PpEval::Unknown;
        }
        return result;
    }
    default:
        return PpEval::Unknown;
    }
}
