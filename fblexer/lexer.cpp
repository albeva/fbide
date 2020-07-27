//
//  lexer.cpp
//  fblexer
//
//  Created by Albert on 26/10/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "lexer.hpp"
#include "StyleContext.h"
#include "sdk/LexerSdk.hpp"

using namespace Scintilla;
using namespace fbide;

//------------------------------------------------------------------------------
// Life Cycle
//------------------------------------------------------------------------------

Lexer::Lexer() {
}

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

int Lexer::PropertyType(const char *name) {
    return -1;
}

const char * Lexer::DescribeProperty(const char *name) {
    return nullptr;
}

Sci_Position Lexer::PropertySet(const char *key, const char *val) {
    return -1;
}

//------------------------------------------------------------------------------
// Words
//------------------------------------------------------------------------------

const char * Lexer::DescribeWordListSets() {
    return nullptr;
}

Sci_Position Lexer::WordListSet(int n, const char *wl) {
    return -1;
}

//------------------------------------------------------------------------------
// Lex
//------------------------------------------------------------------------------

void Lexer::Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument *pAccess) {
    LexAccessor styler(pAccess);
    styler.StartAt(startPos);

    StyleContext sc(startPos, lengthDoc, initStyle, styler);
    for (; ; sc.Forward()) {
        if (!sc.More()) {
            break;
        }
    }

    sc.Complete();
}

//------------------------------------------------------------------------------
// Fold
//------------------------------------------------------------------------------

void Lexer::Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument *pAccess) {
}

//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

void * Lexer::PrivateCall(int operation, void *pointer) {
    if (operation == SET_LEXER_IFACE && pointer != nullptr) {
        m_iface = (ILexerSdk*)pointer;
        m_iface->Log("Lexer iface received");
    }
    return nullptr;
}

ILexer * Lexer::Factory() {
    return new Lexer();
}
