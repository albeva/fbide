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
enum class TokenKind {
    Keyword1,            // keyword group 1
    Keyword2,            // keyword group 2
    Keyword3,            // keyword group 3
    Keyword4,            // keyword group 4
    Comment,             // ' single-line comment
    CommentBlock,        // /' nested multi-line comment '/
    String,              // "double-quoted string"
    UnterminatedString,  // string missing closing quote
    Number,              // numeric literal (including &H, &O, &B prefixes)
    Preprocessor,        // #directive (entire line)
    Operator,            // punctuation and operators
    Identifier,          // any other word
    Whitespace,          // spaces and tabs
    Newline,             // line break
    Invalid,             // unrecognised input
};

/// Structural keyword classification for autoindent and formatting.
enum class KeywordKind {
    None,       // not a keyword
    // Block openers
    Sub,
    Function,
    Do,
    While,
    For,
    With,
    Scope,
    Enum,
    Union,
    Select,
    Asm,
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
    // Comment keyword
    Rem,
    // A keyword not structurally significant
    Other,
};

/// A single token from the lexer.
/// `text` is a view into the source buffer — the source must outlive the token.
struct Token final {
    TokenKind kind {};
    KeywordKind keywordKind = KeywordKind::None;
    std::string_view text {};
};

} // namespace fbide
