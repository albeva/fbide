/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

namespace fbide::FB::Parser {

enum class Kind: uint8_t {
    Invalid,
    EndOfLine,
    EndOfFile,
    LineContinuation, // _
    SingleComment,
    MultiLineCommentStart,
    MultiLineCommentEnd,
    Identifier,
    NumberStart,
    NumberEnd,
    StringStart,
    StringEnd,
    ppInclude,
    ppMacro,
    ppMacroEnd,
    ppDefine,
    kwConst,
    kwDim,
    kwVar,
    kwShared,
    kwEnum,
    kwType,
    kwFunction,
    kwSub,
    opAssign,     // =
    opComma,      // ,
    opPeriod,     // .
    opBraceOpem,  // (
    opBraceClose, // )
};

class SymbolTable;

/**
 * Token is super compact structure that indicates token start positions
 * and does not encode length of given tokens
 */
struct Token {
    /**
     * Token start position. MAX supported is 4MB boundry
     * Beyond that fbide won't bother parsing
     */
    uint32_t pos: 22;

    /**
     * Indicate range token like string, number, comment, etc.
     */
    uint32_t range: 1;

    /**
     * Indicate error. Errors shsould have squiggly underline beneith them
     */
    uint32_t error: 1;

    /**
     * Type of token. Max 256, should be enough
     */
    uint32_t kind: 8;

    /**
     * ID of the Symbol
     * MAX 4194303 symbols
     */
    uint32_t symbolId: 22;

    /**
     * ID of the file
     * MAX 1023 files
     */
    uint32_t fileId: 10;

    [[nodiscard]] Kind GetKind() const noexcept {
        return static_cast<Kind>(kind);
    }

    [[nodiscard]] int Length(const SymbolTable& st) const noexcept;

    [[nodiscard]] inline bool operator==(const Token& rhs) const noexcept {
        return * reinterpret_cast<const uint64_t*>(this) == reinterpret_cast<const uint64_t&>(rhs); // NOLINT
    }

    [[nodiscard]] inline bool operator==(uint64_t rhs) const noexcept {
        return * reinterpret_cast<const uint64_t*>(this) == rhs; // NOLINT
    }

    [[nodiscard]] explicit inline operator bool() const noexcept {
        return * reinterpret_cast<const uint64_t*>(this) != 0; // NOLINT
    }
};

static_assert(sizeof(Token) == 8); // NOLINT

} // namespace fbide::FB::Parser
