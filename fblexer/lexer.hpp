//
//  lexer.hpp
//  fblexer
//
//  Created by Albert on 26/10/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
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
