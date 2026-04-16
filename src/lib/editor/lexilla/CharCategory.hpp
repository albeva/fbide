//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

/// Character classification flags for the FreeBASIC lexer.
enum class CharClass : int {
    EndOfFile  = 1 << 0,
    Whitespace = 1 << 1,
    Operator   = 1 << 2,
    Identifier = 1 << 3,
    Digit      = 1 << 4,
    HexDigit   = 1 << 5,
    BinDigit   = 1 << 6,
    OctDigit   = 1 << 7,
};

static constexpr auto operator+(const CharClass& rhs) -> int {
    return static_cast<int>(rhs);
}

// clang-format off
static constexpr auto charClasses = [] consteval {
    using enum CharClass;
    return std::array {
        /* NUL */ +EndOfFile,
        /* SOH */ 0,
        /* STX */ 0,
        /* ETX */ 0,
        /* EOT */ 0,
        /* ENQ */ 0,
        /* ACK */ 0,
        /* BEL */ 0,
        /* BS  */ 0,
        /* \t  */ +Whitespace,
        /* \n  */ +Whitespace,
        /* VT  */ 0,
        /* FF  */ 0,
        /* \r  */ +Whitespace,
        /* SO  */ 0,
        /* SI  */ 0,
        /* DLE */ 0,
        /* DC1 */ 0,
        /* DC2 */ 0,
        /* DC3 */ 0,
        /* DC4 */ 0,
        /* NAK */ 0,
        /* SYN */ 0,
        /* ETB */ 0,
        /* CAN */ 0,
        /* EM  */ 0,
        /* SUB */ 0,
        /* ESC */ 0,
        /* FS  */ 0,
        /* GS  */ 0,
        /* RS  */ 0,
        /* US  */ 0,
        /* ' ' */ +Whitespace,
        /* '!' */ +Operator,
        /* '"' */ 0,
        /* '#' */ +Operator,
        /* '$' */ +Operator,
        /* '%' */ +Operator,
        /* '&' */ +Operator,
        /* ''' */ 0,
        /* '(' */ +Operator,
        /* ')' */ +Operator,
        /* '*' */ +Operator,
        /* '+' */ +Operator,
        /* ',' */ +Operator,
        /* '-' */ +Operator,
        /* '.' */ 0,
        /* '/' */ +Operator,
        /* '0' */ +Identifier | +Digit | +HexDigit | +OctDigit | +BinDigit,
        /* '1' */ +Identifier | +Digit | +HexDigit | +OctDigit | +BinDigit,
        /* '2' */ +Identifier | +Digit | +HexDigit | +OctDigit,
        /* '3' */ +Identifier | +Digit | +HexDigit | +OctDigit,
        /* '4' */ +Identifier | +Digit | +HexDigit | +OctDigit,
        /* '5' */ +Identifier | +Digit | +HexDigit | +OctDigit,
        /* '6' */ +Identifier | +Digit | +HexDigit | +OctDigit,
        /* '7' */ +Identifier | +Digit | +HexDigit | +OctDigit,
        /* '8' */ +Identifier | +Digit | +HexDigit,
        /* '9' */ +Identifier | +Digit | +HexDigit,
        /* ':' */ +Operator,
        /* ';' */ +Operator,
        /* '<' */ +Operator,
        /* '=' */ +Operator,
        /* '>' */ +Operator,
        /* '?' */ +Operator,
        /* '@' */ +Operator,
        /* 'A' */ +Identifier | +HexDigit,
        /* 'B' */ +Identifier | +HexDigit,
        /* 'C' */ +Identifier | +HexDigit,
        /* 'D' */ +Identifier | +HexDigit,
        /* 'E' */ +Identifier | +HexDigit,
        /* 'F' */ +Identifier | +HexDigit,
        /* 'G' */ +Identifier,
        /* 'H' */ +Identifier,
        /* 'I' */ +Identifier,
        /* 'J' */ +Identifier,
        /* 'K' */ +Identifier,
        /* 'L' */ +Identifier,
        /* 'M' */ +Identifier,
        /* 'N' */ +Identifier,
        /* 'O' */ +Identifier,
        /* 'P' */ +Identifier,
        /* 'Q' */ +Identifier,
        /* 'R' */ +Identifier,
        /* 'S' */ +Identifier,
        /* 'T' */ +Identifier,
        /* 'U' */ +Identifier,
        /* 'V' */ +Identifier,
        /* 'W' */ +Identifier,
        /* 'X' */ +Identifier,
        /* 'Y' */ +Identifier,
        /* 'Z' */ +Identifier,
        /* '[' */ +Operator,
        /* '\' */ +Operator,
        /* ']' */ +Operator,
        /* '^' */ +Operator,
        /* '_' */ +Identifier,
        /* '`' */ +Operator,
        /* 'a' */ +Identifier | +HexDigit,
        /* 'b' */ +Identifier | +HexDigit,
        /* 'c' */ +Identifier | +HexDigit,
        /* 'd' */ +Identifier | +HexDigit,
        /* 'e' */ +Identifier | +HexDigit,
        /* 'f' */ +Identifier | +HexDigit,
        /* 'g' */ +Identifier,
        /* 'h' */ +Identifier,
        /* 'i' */ +Identifier,
        /* 'j' */ +Identifier,
        /* 'k' */ +Identifier,
        /* 'l' */ +Identifier,
        /* 'm' */ +Identifier,
        /* 'n' */ +Identifier,
        /* 'o' */ +Identifier,
        /* 'p' */ +Identifier,
        /* 'q' */ +Identifier,
        /* 'r' */ +Identifier,
        /* 's' */ +Identifier,
        /* 't' */ +Identifier,
        /* 'u' */ +Identifier,
        /* 'v' */ +Identifier,
        /* 'w' */ +Identifier,
        /* 'x' */ +Identifier,
        /* 'y' */ +Identifier,
        /* 'z' */ +Identifier,
        /* '{' */ +Operator,
        /* '|' */ +Operator,
        /* '}' */ +Operator,
        /* '~' */ +Operator,
        /* DEL */ 0,
    };
}();
// clang-format on

FBIDE_INLINE static auto isSpace(const int ch) -> bool {
    const auto idx = static_cast<std::size_t>(ch);
    return idx < charClasses.size() && (charClasses[idx] & +CharClass::Whitespace);
}

FBIDE_INLINE static auto isOperator(const int ch) -> bool {
    const auto idx = static_cast<std::size_t>(ch);
    return idx < charClasses.size() && (charClasses[idx] & +CharClass::Operator);
}

FBIDE_INLINE static auto isIdentifier(const int ch) -> bool {
    const auto idx = static_cast<std::size_t>(ch);
    return idx < charClasses.size() && (charClasses[idx] & +CharClass::Identifier);
}

FBIDE_INLINE static auto isDigit(const int ch) -> bool {
    const auto idx = static_cast<std::size_t>(ch);
    return idx < charClasses.size() && (charClasses[idx] & +CharClass::Digit);
}

FBIDE_INLINE static auto isHexDigit(const int ch) -> bool {
    const auto idx = static_cast<std::size_t>(ch);
    return idx < charClasses.size() && (charClasses[idx] & +CharClass::HexDigit);
}

FBIDE_INLINE static auto isBinDigit(const int ch) -> bool {
    const auto idx = static_cast<std::size_t>(ch);
    return idx < charClasses.size() && (charClasses[idx] & +CharClass::BinDigit);
}

FBIDE_INLINE static auto isOctDigit(const int ch) -> bool {
    const auto idx = static_cast<std::size_t>(ch);
    return idx < charClasses.size() && (charClasses[idx] & +CharClass::OctDigit);
}

FBIDE_INLINE static auto lowerCase(const int c) -> int {
    if (c >= 'A' && c <= 'Z') {
        return c | 0x20;
    }
    return c;
}

FBIDE_INLINE static auto fastUnsafeLowerCase(const int c) -> int {
    return c | 0x20;
}

FBIDE_INLINE static auto isValidAfterNumOrWord(const int ch) -> bool {
    using enum CharClass;
    static constexpr auto Valid = +EndOfFile | +Operator | +Whitespace;
    const auto idx = static_cast<std::size_t>(ch);
    return idx < charClasses.size() && charClasses[idx] & Valid;
}
