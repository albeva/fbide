//
//  module.hpp
//  fblexer
//
//  Created by Albert on 26/10/2016.
//  Copyright © 2016 albeva. All rights reserved.
//

#ifndef module_hpp
#define module_hpp

#include <stddef.h>
#include "Sci_Position.h"
#include "ILexer.h"
#include "LexerModule.h"

#ifdef _WIN32
	#define EXP_DLL __declspec(dllexport)
#else
	#define EXP_DLL
#endif

extern "C" {
	EXP_DLL int GetLexerCount();
	EXP_DLL void GetLexerName(unsigned int Index, char* name, int buflength);
	EXP_DLL Scintilla::LexerFactoryFunction GetLexerFactory(unsigned int Index);
}

#endif /* module_hpp */
