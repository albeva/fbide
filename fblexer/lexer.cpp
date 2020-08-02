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
#include <string>
#include "lexer.hpp"
#include "StyleContext.h"
#include "sdk/LexerSdk.hpp"

using namespace Scintilla;
using namespace fbide;

//------------------------------------------------------------------------------
// Life Cycle
//------------------------------------------------------------------------------

Lexer::Lexer() = default;

Lexer::~Lexer() = default;

int Lexer::Version() const {
    return Scintilla::lvOriginal;
}

void Lexer::Release() {
    delete this;
}

//------------------------------------------------------------------------------
// Properties
//------------------------------------------------------------------------------

const char * Lexer::PropertyNames() {
    return nullptr;
}

int Lexer::PropertyType(const char * /*name*/) {
    return -1;
}

const char * Lexer::DescribeProperty(const char * /*name*/) {
    return nullptr;
}

Sci_Position Lexer::PropertySet(const char * /*key*/, const char * /*val*/) {
    return -1;
}

//------------------------------------------------------------------------------
// Words
//------------------------------------------------------------------------------

const char * Lexer::DescribeWordListSets() {
    return nullptr;
}

Sci_Position Lexer::WordListSet(int n, const char * wl) {
    if (n >= KEYWORD_GROUPS_COUNT) {
        LogError("Invalid worldlist group index " + std::to_string(n));
        return -1;
    }
    m_wordLists.at(n).Set(wl);
    return 0;
}

//------------------------------------------------------------------------------
// Lex
//------------------------------------------------------------------------------

void Lexer::Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument *pAccess) {
    LexAccessor styler(pAccess);
    styler.StartAt(startPos);
    CleanMLCommentsFrom(startPos);

    StyleContext sc(startPos, static_cast<Sci_PositionU>(lengthDoc), initStyle, styler);
    for (;; sc.Forward()) {
        auto state = (FBStyle) sc.state;
        switch (state) {
            case FBStyle::Default:
                if (sc.Match('\'')) {
                    sc.SetState((int) FBStyle::Comment);
                }
                if (sc.Match('/', '\'')) {
                    sc.SetState((int) FBStyle::MultilineComment);
                    ToggleMLComment(sc.currentPos, true);
                    sc.Forward();
                }
                break;
            case FBStyle::Comment:
                if (sc.atLineEnd) {
                    sc.SetState((int) FBStyle::Default);
                }
                break;
            case FBStyle::MultilineComment:
                if (sc.Match('\'', '/')) {
                    ToggleMLComment(sc.currentPos, false);
                    auto isInComment = IsInMLComment(sc.currentPos);
                    sc.Forward();
                    if (!isInComment) {
                        sc.ForwardSetState((int)FBStyle::Default);
                    } else {
                        pAccess->ChangeLexerState(sc.currentPos, pAccess->Length());
                    }
                } else if (sc.Match('/', '\'')) {
                    ToggleMLComment(sc.currentPos, true);
                    sc.Forward();
                }
                break;
//            case FBStyle::String:
//                break;
//            case FBStyle::Number:
//                break;
//            case FBStyle::Preprocessor:
//                break;
//            case FBStyle::Operator:
//                break;
//            case FBStyle::Identifier:
//                break;
            default:
                break;
        }

        if (!sc.More()) {
            break;
        }
    }

    sc.Complete();
}

void Lexer::ToggleMLComment(Sci_PositionU pos, bool toggle) {
    auto iter = m_multilineStack.crbegin();
    for (; iter != m_multilineStack.crend(); ++iter) {
        if (iter->first < pos) {
            break;
        }
    }
    m_multilineStack.emplace(iter.base(), std::pair{pos, toggle});
}

void Lexer::CleanMLCommentsFrom(Sci_PositionU pos) {
    if (pos > 0) {
        for (auto iter = m_multilineStack.crbegin(); iter != m_multilineStack.crend(); ++iter) {
            if (iter->first < pos) {
                m_multilineStack.erase(iter.base(), m_multilineStack.end());
                return;
            }
        }
    }
    m_multilineStack.clear();
}

bool Lexer::IsInMLComment(Sci_Position pos) const {
    int level = 0;
    for (const auto& e: m_multilineStack) {
        if (e.first > pos) {
            return level > 0;
        }
        if (e.second) {
            level++;
        } else {
            level--;
        }
    }
    return level != 0;
}

//------------------------------------------------------------------------------
// Fold
//------------------------------------------------------------------------------

void Lexer::Fold(Sci_PositionU /* startPos */, Sci_Position /* lengthDoc */, int /* initStyle */, IDocument* /*pAccess*/) {
}

//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

void Lexer::LogError(const std::string& error) {
    if (m_iface != nullptr) {
        m_iface->Log("ERROR: " + error);
    }
}

void * Lexer::PrivateCall(int operation, void *pointer) {
    if (operation == SET_LEXER_IFACE && pointer != nullptr) {
        m_iface = reinterpret_cast<ILexerSdk*>(pointer); // NOLINT
        m_iface->Log("Initialize fblexer iface");
    }
    return nullptr;
}

ILexer * Lexer::Factory() {
    return new Lexer(); // NOLINT
}

