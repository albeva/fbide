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
#include "lexer.hpp"
#include "StyleContext.h"
#include "sdk/LexerSdk.hpp"

using namespace Scintilla;
using namespace fbide;

//------------------------------------------------------------------------------
// Life Cycle
//------------------------------------------------------------------------------

Lexer::Lexer() = default;

Lexer::~Lexer() = default;

int Lexer::Version() const {
    return Scintilla::lvOriginal;
}

void Lexer::Release() {
    delete this;
}

//------------------------------------------------------------------------------
// Properties
//------------------------------------------------------------------------------

const char * Lexer::PropertyNames() {
    return nullptr;
}

int Lexer::PropertyType(const char * /*name*/) {
    return -1;
}

const char * Lexer::DescribeProperty(const char * /*name*/) {
    return nullptr;
}

Sci_Position Lexer::PropertySet(const char * /*key*/, const char * /*val*/) {
    return -1;
}

//------------------------------------------------------------------------------
// Words
//------------------------------------------------------------------------------

const char * Lexer::DescribeWordListSets() {
    return nullptr;
}

Sci_Position Lexer::WordListSet(int  /*n*/, const char * /*wl*/) {
    return -1;
}

//------------------------------------------------------------------------------
// Lex
//------------------------------------------------------------------------------

void Lexer::Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument *pAccess) {
    LexAccessor styler(pAccess);
    styler.StartAt(startPos);

    StyleContext sc(startPos, static_cast<Sci_PositionU>(lengthDoc), initStyle, styler);
    for (; sc.More(); sc.Forward()) {
    }

    sc.Complete();
}

//------------------------------------------------------------------------------
// Fold
//------------------------------------------------------------------------------

void Lexer::Fold(Sci_PositionU /* startPos */, Sci_Position /* lengthDoc */, int /* initStyle */, IDocument* /*pAccess*/) {
}

//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

void * Lexer::PrivateCall(int operation, void *pointer) {
    if (operation == SET_LEXER_IFACE && pointer != nullptr) {
        m_iface = reinterpret_cast<ILexerSdk*>(pointer); // NOLINT
        m_iface->Log("Initialize fblexer iface");
    }
    return nullptr;
}

ILexer * Lexer::Factory() {
    return new Lexer(); // NOLINT
}
