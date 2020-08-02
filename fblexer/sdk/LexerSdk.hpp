/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include <iostream>

namespace fbide {

#define FB_STYLE(_) \
    _( Default      ) \
    _( Comment      ) \
    _( String       ) \
    _( Number       ) \
    _( Keyword1     ) \
    _( Keyword2     ) \
    _( Keyword3     ) \
    _( Keyword4     ) \
    _( Preprocessor ) \
    _( Operator     ) \
    _( Identifier   )

enum class FBStyle {
    #define FB_STYLE_ENUM(Nr) Nr,
    FB_STYLE(FB_STYLE_ENUM)
    #undef FB_STYLE_ENUM
};

constexpr int SET_LEXER_IFACE = 1337;

class ILexerSdk {
public:
    ILexerSdk(const ILexerSdk&) = delete;
    ILexerSdk(ILexerSdk&&) = delete;
    ILexerSdk& operator=(const ILexerSdk&) = delete;
    ILexerSdk& operator=(ILexerSdk&&) = delete;

    ILexerSdk() = default;
    virtual ~ILexerSdk() = default;
    virtual void Log(const std::string& message) = 0;
};

}
