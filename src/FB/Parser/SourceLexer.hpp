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
#include "Token.hpp"
#include "SymbolTable.hpp"

namespace fbide::FB::Parser {

class Identifier;
class SymbolTable;
class Type;

class Symbol {
    /**
     * Symbol name
     */
    std::string id;

    /**
     * Table symbol belongs to
     */
    SymbolTable* table;

    /**
     * Type of the symbol
     */
    Type* type;

    /**
     * Location where symbol is first declared
     */
    Identifier* declaration;

    /**
     * Location where symbol is first defined
     */
    Identifier* definition;

    /**
     * Symbol occurances
     */
     std::vector<Identifier*> occurances;
};

class Identifier {
    struct Location {
        uint32_t pos:22;
        uint32_t file:10;
    };
    Location location;
    Symbol* symbol;
};

class SourceLexer {
    NON_COPYABLE(SourceLexer)
public:
    SourceLexer();
    ~SourceLexer();

    // void Insert(int pos, char) noexcept;        // single char
    // void Insert(int pos, const char*) noexcept; // insert range
    // void Remove(int pos, len) noexcept;         // remove
    
    std::vector<Token> m_tokens{};
    std::unordered_map<uint32_t, Identifier> m_identifiers;
};

} // namespace fbide::FB::Parser
