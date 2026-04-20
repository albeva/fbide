//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "analyses/lexer/Lexer.hpp"

namespace fbide::tests {

/// Load keyword groups from an INI `.lng` file's `[keywords]` section (kw1..kw4).
/// Each non-empty group is returned as a Code-scoped KeywordGroup.
inline auto loadKeywordGroups(const wxString& path) -> std::vector<lexer::KeywordGroup> {
    std::vector<lexer::KeywordGroup> groups;
    wxFFileInputStream stream(path);
    if (!stream.IsOk()) {
        return groups;
    }
    wxFileConfig ini(stream);
    ini.SetPath("/keywords");
    constexpr std::array tokenKinds {
        lexer::TokenKind::Keyword1, lexer::TokenKind::Keyword2,
        lexer::TokenKind::Keyword3, lexer::TokenKind::Keyword4
    };
    for (std::size_t i = 0; i < tokenKinds.size(); i++) {
        wxString key;
        key.Printf("kw%zu", i + 1);
        auto value = ini.Read(key, "");
        if (!value.IsEmpty()) {
            groups.push_back({ std::move(value), tokenKinds[i], lexer::KeywordScope::Code });
        }
    }
    return groups;
}

} // namespace fbide::testing
