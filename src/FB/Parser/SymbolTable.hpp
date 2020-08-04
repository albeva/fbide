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

struct Token;
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
    Token* declaration;

    /**
     * Location where symbol is first defined
     */
    Token* definition;

    /**
     * Symbol occurances
     */
    std::vector<Token*> occurances;
};

class SymbolTable {

};

} // namespace fbide::FB::Parser
