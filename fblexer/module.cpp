//
//  module.cpp
//  fblexer
//
//  Created by Albert on 26/10/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "module.hpp"
#include "lexer.hpp"
#include <cassert>
#include <cstring>

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
