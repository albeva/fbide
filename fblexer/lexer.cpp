//
//  lexer.cpp
//  fblexer
//
//  Created by Albert on 26/10/2016.
//  Copyright © 2016 albeva. All rights reserved.
//

#include "lexer.hpp"
using namespace Scintilla;

Lexer::Lexer()
{
}


Lexer::~Lexer()
{
}


void Lexer::Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument *pAccess)
{
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


void Lexer::Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument *pAccess)
{
}


void * Lexer::PrivateCall(int operation, void *pointer)
{
    return pointer;
}


ILexer * Lexer::Factory()
{
    return new Lexer();
}

