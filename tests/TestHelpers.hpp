//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "analyses/lexer/KeywordTables.hpp"
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/Token.hpp"
#include "config/ThemeCategory.hpp"
#include "editor/lexilla/FBSciLexer.hpp"

namespace fbide::tests {

/// Read a `.lng` file's `[keywords]` section (kw1..kw4) into a configured
/// FBSciLexer, ready to lex source. Returned ILexer5 must be Released by the
/// caller. Slot 6 (KeywordPP) is seeded from the canonical ppKeywords() table
/// so PP block detection (`#ifdef`/`#endif`) works without depending on the
/// .lng file shipping a KeywordPP group.
inline auto createFbLexer(const wxString& kwIniPath) -> Scintilla::ILexer5* {
    auto* lex = FBSciLexer::Create();
    wxFFileInputStream stream(kwIniPath);
    if (!stream.IsOk()) {
        return lex;
    }
    wxFileConfig ini(stream);
    ini.SetPath("/keywords");
    for (std::size_t i = 0; i < 4; i++) {
        wxString key;
        key.Printf("kw%zu", i + 1);
        const auto value = ini.Read(key, "").Lower();
        lex->WordListSet(static_cast<int>(i), value.utf8_str());
    }
    // KeywordPP slot — seed from ppKeywords() so #ifdef/#endif/etc. style
    // as KeywordPP, letting StyleLexer fill kwKind=PpIfDef etc.
    std::string pp;
    for (const auto& [text, _] : lexer::ppKeywords()) {
        if (!pp.empty()) pp += ' ';
        pp += text;
    }
    constexpr std::size_t ppSlot = indexOfKeywordGroup(ThemeCategory::KeywordPP);
    lex->WordListSet(static_cast<int>(ppSlot), pp.c_str());
    return lex;
}

/// Tokenise `source` via FBSciLexer + StyleLexer through a headless
/// MemoryDocument. The lexer instance is owned by the caller.
inline auto tokenise(Scintilla::ILexer5& lex, std::string_view source) -> std::vector<lexer::Token> {
    MemoryDocument doc;
    doc.Set(source);
    lex.Lex(0, doc.Length(), +ThemeCategory::Default, &doc);
    lexer::MemoryDocStyledSource src(doc);
    lexer::StyleLexer adapter(src);
    return adapter.tokenise();
}

} // namespace fbide::tests
