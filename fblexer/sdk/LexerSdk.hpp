//
// Created by Albert on 7/27/2020.
//
#pragma once
#include <iostream>

#if EXPORT_LEXER_IFACE
    #define SDK_IMPORT __declspec (dllexport)
#else
     #define SDK_IMPORT __declspec (dllimport)
#endif

namespace fbide {

constexpr int SET_LEXER_IFACE = 1337;

class SDK_IMPORT ILexerSdk {
public:
    virtual void Log(const std::string& message) = 0;
};

}
