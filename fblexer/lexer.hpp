//
//  lexer.hpp
//  fblexer
//
//  Created by Albert on 26/10/2016.
//  Copyright © 2016 albeva. All rights reserved.
//

#ifndef lexer_hpp
#define lexer_hpp

#include <assert.h>
#include <map>
#include <iostream>

#include "ILexer.h"
#include "LexAccessor.h"
#include "StyleContext.h"


class Lexer : Scintilla::ILexer
{
public:
    
    Lexer() ;
    
    virtual ~Lexer() final;
    
    virtual int Version() const final { return Scintilla::lvOriginal; }
    
    virtual void Release() final { delete this; }
    
    virtual const char * PropertyNames() final
    {
        return nullptr;
    }
    
    virtual int PropertyType(const char *name) final
    {
        return -1;
    }
    
    virtual const char * DescribeProperty(const char *name) final
    {
        return nullptr;
    }
    
    virtual Sci_Position PropertySet(const char *key, const char *val) final
    {
        return -1;
    }
    
    virtual const char * DescribeWordListSets() final
    {
        return nullptr;
    }
    
    virtual Sci_Position WordListSet(int n, const char *wl) final
    {
        return -1;
    }
    
    virtual void Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess) final;
    
    virtual void Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess) final;
    
    virtual void * PrivateCall(int operation, void *pointer) final;
    
    static ILexer * Factory();
};


#endif /* lexer_hpp */
