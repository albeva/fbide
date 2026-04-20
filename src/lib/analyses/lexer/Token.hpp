//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide::lexer {

/// Token types produced by the FreeBASIC lexer.
/// Keyword* kinds mirror the keyword-group categories in ThemeCategory.
enum class TokenKind {
    Keyword1,           // keyword group 1 (code scope)
    Keyword2,           // keyword group 2 (code scope)
    Keyword3,           // keyword group 3 (code scope)
    Keyword4,           // keyword group 4 (code scope)
    KeywordCustom1,     // user-defined keywords (code scope)
    KeywordCustom2,     // user-defined keywords (code scope)
    KeywordPP,          // preprocessor-scoped keyword group
    KeywordAsm1,        // asm-block-scoped keyword group 1
    KeywordAsm2,        // asm-block-scoped keyword group 2
    Comment,            // ' single-line comment
    CommentBlock,       // /' nested multi-line comment '/
    String,             // "double-quoted string"
    UnterminatedString, // string missing closing quote
    Number,             // numeric literal (including &H, &O, &B prefixes)
    Preprocessor,       // #directive (entire line)
    Operator,           // punctuation and operators
    Identifier,         // any other word
    Whitespace,         // spaces and tabs
    Newline,            // line break
    Invalid,            // unrecognised input
};

/// True when `kind` is any of the keyword-group token kinds (excludes Identifier).
constexpr auto isKeywordToken(const TokenKind kind) noexcept -> bool {
    switch (kind) {
    case TokenKind::Keyword1:
    case TokenKind::Keyword2:
    case TokenKind::Keyword3:
    case TokenKind::Keyword4:
    case TokenKind::KeywordCustom1:
    case TokenKind::KeywordCustom2:
    case TokenKind::KeywordPP:
    case TokenKind::KeywordAsm1:
    case TokenKind::KeywordAsm2:
        return true;
    default:
        return false;
    }
}

/// True when `kind` is word-shaped: Identifier or any keyword kind.
constexpr auto isWordLike(const TokenKind kind) noexcept -> bool {
    return kind == TokenKind::Identifier || isKeywordToken(kind);
}

/// Structural keyword classification for autoindent and formatting.
enum class KeywordKind {
    None, // not a keyword
    // Block openers (require definition check — may be Declare'd)
    Sub,
    Function,
    Constructor,
    Destructor,
    Operator,
    Do,
    While,
    For,
    With,
    Scope,
    Enum,
    Union,
    Select,
    Asm,
    Namespace,
    // Block closers
    End,
    Loop,
    Next,
    Wend,
    // Mid-block
    Else,
    ElseIf,
    Case,
    // Conditional
    If,
    Then,
    // Type
    Type,
    As,
    // Declaration
    Declare,
    // Early-exit statements (prevent following block keyword from opening a scope)
    Exit,
    Continue,
    // Comment keyword
    Rem,
    // Preprocessor block openers
    PpIf,
    PpIfDef,
    PpIfNDef,
    PpMacro,
    // Preprocessor block closers
    PpEndIf,
    PpEndMacro,
    // Preprocessor mid-block
    PpElse,
    PpElseIf,
    PpElseIfDef,
    PpElseIfNDef,
    // Preprocessor non-block
    PpOther,
    // A keyword not structurally significant
    Other,
};

/// Classification of symbol operators for formatting.
/// Keyword operators (And, Or, Mod, Shl, etc.) are identified by TokenKind::Keyword3
/// and don't need OperatorKind — they always get binary spacing.
enum class OperatorKind : std::uint8_t {
    None, // not a symbol operator

    // Punctuation / structural
    ParenOpen,    // (
    ParenClose,   // )
    BracketOpen,  // [
    BracketClose, // ]
    BraceOpen,    // {
    BraceClose,   // }
    Comma,        // ,
    Semicolon,    // ;
    Colon,        // :
    Dot,          // .
    Ellipsis2,    // ..
    Ellipsis3,    // ...
    Arrow,        // ->
    Question,     // ?

    // Assignment
    Assign,       // =
    AddAssign,    // +=
    SubAssign,    // -=
    MulAssign,    // *=
    DivAssign,    // /=
    IntDivAssign, // \=
    ExpAssign,    // ^=
    ConcatAssign, // &=

    // Comparison
    NotEqual,     // <>
    Less,         // <
    Greater,      // >
    LessEqual,    // <=
    GreaterEqual, // >=

    // Arithmetic (binary)
    Add,          // + (binary)
    Subtract,     // - (binary)
    Multiply,     // * (binary)
    Divide,       // /
    IntDivide,    // backslash
    Exponentiate, // ^
    Concatenate,  // &

    // Arithmetic (unary)
    Negate,      // - (unary)
    UnaryPlus,   // + (unary)
    AddressOf,   // @ (unary)
    Dereference, // * (unary)

    // Shift (symbol forms)
    ShiftLeft,  // <<
    ShiftRight, // >>
    ShlAssign,  // <<=
    ShrAssign,  // >>=

    // Type suffix sigils
    Hash,        // #
    Dollar,      // $
    Percent,     // %
    Exclamation, // !
};

/// A single token from the lexer.
/// `text` owns its contents — tokens are self-contained and outlive the source.
/// `verbatim` marks the token as residing inside a `' format off` region;
/// downstream transforms must preserve its original text unchanged.
struct Token final {
    TokenKind kind {};
    KeywordKind keywordKind = KeywordKind::None;
    OperatorKind operatorKind = OperatorKind::None;
    bool verbatim = false;
    std::string text;
};

} // namespace fbide::lexer
