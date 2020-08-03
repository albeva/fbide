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
#ifndef module_hpp
#define module_hpp

#include <cstddef>
#include "Sci_Position.h"
#include "ILexer.h"
#include "LexerModule.h"

#ifdef _WIN32
	#define EXP_DLL __declspec(dllexport)
    #define EXT_LEXER_DECL
#else
	#define EXP_DLL
    #define EXT_LEXER_DECL
#endif

extern "C" {
	EXP_DLL int EXT_LEXER_DECL GetLexerCount();
	EXP_DLL void EXT_LEXER_DECL GetLexerName(unsigned int Index, char* name, int buflength);
	EXP_DLL Scintilla::LexerFactoryFunction EXT_LEXER_DECL GetLexerFactory(unsigned int Index);
}

#endif /* module_hpp */
