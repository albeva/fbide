//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"
#include "WordList.h"
#include "LexAccessor.h"
#include "DefaultLexer.h"

namespace fbide {

/// Custom Scintilla lexer for FreeBASIC.
class FBSciLexer final : public Lexilla::DefaultLexer {
public:
    FBSciLexer();

    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
        int initStyle, Scintilla::IDocument* pAccess) override;

    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc,
        int initStyle, Scintilla::IDocument* pAccess) override;

    /// Factory method for Scintilla.
    static auto Create() -> Scintilla::ILexer5*;
};

} // namespace fbide
