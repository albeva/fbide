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
#include "pch.h"

namespace fbide::FB::Parser {

enum class Kind: uint8_t {
    Invalid,
    SingleComment,
    MultiComment,
    Include,
    Define,
    Macro,
    Identifier, // including unimportant keywords
    Keyword,
    Number,
    String,
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

enum class Scope {
    Normal,
    Define,
    Macro,
    Assembly
};

struct Token {
    Kind     token;     // 8 bits token id
    uint8_t  flags;     // 8 bits of flags
    uint16_t scope:4;   // Scope
    uint16_t misc:12;   // misc
    uint32_t start:22;  // MAX 4MB
    uint32_t len:10;    // MAX 1kb
};

static_assert(sizeof(Token) == 8); // NOLINT

class SourceLexer {
    NON_COPYABLE(SourceLexer)
public:
    SourceLexer();
    ~SourceLexer();


    std::vector<Token> m_tokens{};
};

}
