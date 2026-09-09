//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "analyses/lexer/Token.hpp"
#include "config/ThemeCategory.hpp"
#include "editor/lexilla/FBSciLexer.hpp"

namespace fbide::tests {

/// Read the shipped `keywords.ini` `[groups]` section into a configured
/// FBSciLexer, ready to lex source. Group keys are the ThemeCategory names
/// (`Keywords`, `KeywordTypes`, ...) — the same file and layout the IDE loads
/// at runtime, so tests exercise the real keyword lists. Returned ILexer5 must
/// be Released by the caller.
inline auto createFbLexer(const wxString& keywordsIniPath) -> Scintilla::ILexer5* {
    auto* lex = FBSciLexer::Create();
    wxFFileInputStream stream(keywordsIniPath);
    if (!stream.IsOk()) {
        return lex;
    }
    wxFileConfig ini(stream);
    ini.SetPath("/groups");
    std::array<std::string, kThemeKeywordGroupsCount> groups;
    for (std::size_t i = 0; i < kThemeKeywordCategories.size(); i++) {
        const auto key = getThemeCategoryName(kThemeKeywordCategories[i]);
        groups[i] = std::string(ini.Read(wxString(key), "").utf8_str());
    }
    FBSciLexer::setKeywords(groups);
    return lex;
}

/// Tokenise `source` via FBSciLexer + StyleLexer through a headless
/// MemoryDocument. The lexer instance is owned by the caller. A trailing
/// newline is appended when missing so per-line resolution (LineState
/// transitions, e.g. AsmState Undetermined → Block) finalizes — single-line
/// inputs without a terminator would otherwise leave the last identifier
/// un-classified.
inline auto tokenise(Scintilla::ILexer5& lex, std::string_view source) -> std::vector<lexer::Token> {
    std::string buf;
    if (source.empty() || source.back() != '\n') {
        buf.assign(source);
        buf.push_back('\n');
        source = buf;
    }
    MemoryDocument doc;
    doc.Set(source);
    lex.Lex(0, doc.Length(), +ThemeCategory::Default, &doc);
    lexer::MemoryDocStyledSource src(doc);
    lexer::StyleLexer adapter(src);
    return adapter.tokenise();
}

} // namespace fbide::tests
