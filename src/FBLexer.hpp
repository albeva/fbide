//
//  FBLexer.hpp
//  fbide
//
//  Created by Albert on 23/10/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "ILexer.h"


namespace fbide {
    
    /**
     * Scintilla Lexer for FreeBASIC
     */
    class LexerFreeBasic : public ILexer
    {
    public:
        
        LexerFreeBasic() {}
        
        
        virtual ~LexerFreeBasic() {}
        
        
        void Release()
        {
            delete this;
        }
        
        
        int Version() const
        {
            return lvOriginal;
        }
        
        
        const char * PropertyNames()
        {
            return nullptr;
        }
        
        
        int PropertyType(const char *name)
        {
            return -1;
        }
        
        
        const char * DescribeProperty(const char *name)
        {
            return nullptr;
        }
        
        
        Sci_Position PropertySet(const char *key, const char *val);
        
        
        const char * DescribeWordListSets()
        {
            return nullptr;
        }
        
        
        Sci_Position WordListSet(int n, const char *wl);
        
        
        void Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument *pAccess);
        
        
        void Fold(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument *pAccess);
        
        
        void * PrivateCall(int, void *)
        {
            return 0;
        }
        
        
        static ILexer *Factory()
        {
            return new LexerFreeBasic();
        }
        
        
        static void SetupLexer();
    };
    
}
