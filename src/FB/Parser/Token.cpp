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
#include "Token.hpp"
using namespace fbide::FB::Parser;

int Token::Length(const SymbolTable&  /*st*/) const noexcept {

    switch (GetKind()) {
        case Kind::LineContinuation:
            return 1;
        case Kind::SingleComment:
            return 0;
        case Kind::MultiLineCommentStart:
            return 0;
        case Kind::MultiLineCommentEnd:
            return 0;
        case Kind::Identifier:
            // return st.GetSymbol(symbolId).GetLexer().size();
            return 0;
        case Kind::NumberStart:
            return 1;
        case Kind::NumberEnd:
            return 1;
        case Kind::StringStart:
            return 1;
        case Kind::StringEnd:
            return 1;
        case Kind::ppInclude:
            return 7;
        case Kind::ppMacro:
            return 5;
        case Kind::ppMacroEnd:
            return 8;
        case Kind::ppDefine:
            return 6;
        case Kind::kwConst:
            return 4;
        case Kind::kwDim:
            return 3;
        case Kind::kwVar:
            return 3;
        case Kind::kwShared:
            return 6;
        case Kind::kwEnum:
            return 4;
        case Kind::kwType:
            return 4;
        case Kind::kwFunction:
            return 8;
        case Kind::kwSub:
            return 3;
        case Kind::opAssign:
            return 1;
        case Kind::opComma:
            return 1;
        case Kind::opPeriod:
            return 1;
        case Kind::opBraceOpem:
            return 1;
        case Kind::opBraceClose:
            return 1;
        default:
            return 0;
    }
}
