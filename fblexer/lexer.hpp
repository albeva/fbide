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

class Lexer final: Scintilla::ILexer {
public:
    Lexer() ;
    virtual ~Lexer() final;
    virtual int Version() const final;
    virtual void Release() final;
    
    virtual const char * PropertyNames();
    virtual int PropertyType(const char *name);
    virtual const char * DescribeProperty(const char *name);
    virtual Sci_Position PropertySet(const char *key, const char *val);
    
    virtual const char * DescribeWordListSets() final;
    virtual Sci_Position WordListSet(int n, const char *wl) final;
    
    virtual void Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess) final;
    virtual void Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess) final;

    virtual void * PrivateCall(int operation, void *pointer) final;
    static ILexer * Factory();
};
