//
//  lexer.hpp
//  fblexer
//
//  Created by Albert on 26/10/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once

#include <assert.h>
#include "ILexer.h"
#include "LexAccessor.h"

namespace fbide {
class ILexerSdk;
}

class Lexer final: Scintilla::ILexer {
public:
    Lexer() ;
    virtual ~Lexer() final;
    virtual int SCI_METHOD Version() const final;
    virtual void SCI_METHOD Release() final;
    
    virtual const char * SCI_METHOD PropertyNames();
    virtual int SCI_METHOD PropertyType(const char *name);
    virtual const char * SCI_METHOD DescribeProperty(const char *name);
    virtual Sci_Position SCI_METHOD PropertySet(const char *key, const char *val);
    
    virtual const char * SCI_METHOD DescribeWordListSets() final;
    virtual Sci_Position SCI_METHOD WordListSet(int n, const char *wl) final;
    
    virtual void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess) final;
    virtual void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess) final;

    virtual void * SCI_METHOD PrivateCall(int operation, void *pointer) final;
    static ILexer * Factory();

private:
    fbide::ILexerSdk* m_iface = nullptr;
};
