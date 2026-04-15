//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FBSciLexer.hpp"
using namespace fbide;

FBSciLexer::FBSciLexer()
    : DefaultLexer("freebasic", SCLEX_FREEBASIC) {}

void SCI_METHOD FBSciLexer::Lex(
    const Sci_PositionU startPos,
    const Sci_Position lengthDoc,
    int /*initStyle*/,
    Scintilla::IDocument* pAccess
) {
    Lexilla::LexAccessor styler(pAccess);

    // For now, style everything as default
    styler.StartAt(startPos);
    styler.StartSegment(startPos);

    for (Sci_PositionU i = startPos; i < startPos + static_cast<Sci_PositionU>(lengthDoc); i++) {
        styler.ColourTo(i, SCE_B_DEFAULT);
    }
    styler.Flush();
}

void SCI_METHOD FBSciLexer::Fold(
    Sci_PositionU /*startPos*/,
    Sci_Position /*lengthDoc*/,
    int /*initStyle*/,
    Scintilla::IDocument* /*pAccess*/
) {
    // Folding not implemented yet
}

auto FBSciLexer::Create() -> Scintilla::ILexer5* {
    return new FBSciLexer();
}
