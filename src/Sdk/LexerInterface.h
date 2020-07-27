//
// Created by Albert on 7/27/2020.
//
#pragma once
#include "app_pch.hpp"
#include "LexerSdk.hpp"

namespace fbide {

class SDK_IMPORT LexerInterface final: public ILexerSdk {
    void Log(const std::string& message) final;
};

} // namespace fbide
