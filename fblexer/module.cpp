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
#include "module.hpp"
#include "lexer.hpp"
#include <cassert>
#include <cstring>
using namespace fbide;

int GetLexerCount() {
    return 1;
}

void GetLexerName(unsigned int Index, char* name, int buflength) {
    assert(Index == 0 && "Invalid lexer index");
    #if defined(_MSC_VER)
        strcpy_s(name, buflength, "text/freebasic");
    #else
        strcpy(name, "text/freebasic");
    #endif
}

Scintilla::LexerFactoryFunction GetLexerFactory(unsigned int Index) {
    assert(Index == 0 && "Invalid lexer index");
    return Lexer::Factory;
}
