//
// Created by Albert on 7/27/2020.
//
#pragma once
#include <iostream>

namespace fbide {

constexpr int SET_LEXER_IFACE = 1337;

class ILexerSdk {
public:
    virtual void Log(const std::string& message) = 0;
};

}
