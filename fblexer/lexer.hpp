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

#include <cassert>
#include "ILexer.h"
#include "LexAccessor.h"

namespace fbide {
class ILexerSdk;
}

class Lexer final: Scintilla::ILexer {
public:
    Lexer(const Lexer&) = delete;
    Lexer(Lexer&&) = delete;
    Lexer& operator=(const Lexer&) = delete;
    Lexer& operator=(Lexer&&) = delete;

    Lexer() ;
    virtual ~Lexer() final;
    [[nodiscard]] int SCI_METHOD Version() const final;
    void SCI_METHOD Release() final;
    
    const char * SCI_METHOD PropertyNames() final;
    int SCI_METHOD PropertyType(const char *name) final;
    const char * SCI_METHOD DescribeProperty(const char *name) final;
    Sci_Position SCI_METHOD PropertySet(const char *key, const char *val) final;
    
    const char * SCI_METHOD DescribeWordListSets() final;
    Sci_Position SCI_METHOD WordListSet(int n, const char *wl) final;
    
    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess) final;
    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess) final;

    void * SCI_METHOD PrivateCall(int operation, void *pointer) final;
    static ILexer * Factory();

private:
    fbide::ILexerSdk* m_iface = nullptr;
};
