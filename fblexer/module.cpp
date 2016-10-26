//
//  module.cpp
//  fblexer
//
//  Created by Albert on 26/10/2016.
//  Copyright © 2016 albeva. All rights reserved.
//

#include "module.hpp"
#include "lexer.hpp"
#include <assert.h>
#include <string.h>


int GetLexerCount()
{
    return 1;
}


void GetLexerName(unsigned int Index, char* name, int buflength)
{
    assert(Index == 0 && "Invalid lexer index");
    strcpy(name, "fbide-freebasic");
}


Scintilla::LexerFactoryFunction GetLexerFactory(unsigned int Index)
{
    assert(Index == 0 && "Invalid lexer index");
    return Lexer::Factory;
}
