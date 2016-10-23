//
//  FBLexer.cpp
//  fbide
//
//  Created by Albert on 23/10/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "FBLexer.hpp"

#include <string>
#include <map>

#include "Scintilla.h"
#include "SciLexer.h"
#include "WordList.h"
#include "LexAccessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"
#include "OptionSet.h"
#include "Catalogue.h"


using namespace fbide;


Sci_Position LexerFreeBasic::PropertySet(const char *key, const char *val)
{
    return -1;
}


Sci_Position LexerFreeBasic::WordListSet(int n, const char *wl)
{
    return -1;
}


void LexerFreeBasic::Lex(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument *pAccess)
{
    LexAccessor styler(pAccess);
    styler.StartAt(startPos);
    StyleContext sc(startPos, length, initStyle, styler);
    
    // Can't use sc.More() here else we miss the last character
    for (; ;sc.Forward()) {
        if (!sc.More()) {
            break;
        }
    }
    sc.Complete();
}


void LexerFreeBasic::Fold(Sci_PositionU startPos, Sci_Position length, int initStyle, IDocument *pAccess)
{

}


void LexerFreeBasic::SetupLexer()
{
    auto lexer = new LexerModule{200, LexerFreeBasic::Factory, "freebasic", nullptr};
    Catalogue::AddLexerModule(lexer);
}
