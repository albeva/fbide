//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Token types produced by the FreeBASIC lexer.
enum class TokenKind {
    Keyword1,      // keyword group 1
    Keyword2,      // keyword group 2
    Keyword3,      // keyword group 3
    Keyword4,      // keyword group 4
    Comment,       // ' single-line comment
    String,        // "double-quoted string"
    Number,        // numeric literal (including &H, &O, &B prefixes)
    Preprocessor,  // #directive (entire line)
    Operator,      // punctuation and operators
    Identifier,    // any other word
    Whitespace,    // spaces and tabs
    Newline,       // line break
};

/// A single token from the lexer.
struct Token final {
    TokenKind kind;
    wxString text;
};

} // namespace fbide
