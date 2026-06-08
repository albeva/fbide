//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "markdown/CodeFence.hpp"
using namespace fbide;
using namespace fbide::markdown;

auto fbide::markdown::isFreeBasicTag(const wxString& lang) -> bool {
    return lang == "freebasic" || lang == "fb" || lang == "basic" || lang == "bas";
}

auto fbide::markdown::plainCodeLines(const wxString& code) -> std::vector<CodeLine> {
    const wxColour fg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    std::vector<CodeLine> lines;
    CodeLine current;
    wxString segment;
    for (const wxUniChar ch : code) {
        if (ch == '\n') {
            if (!segment.empty()) {
                current.push_back({ .text = segment, .colour = fg });
            }
            lines.push_back(std::move(current));
            current = {};
            segment.clear();
        } else {
            segment += ch;
        }
    }
    if (!segment.empty()) {
        current.push_back({ .text = segment, .colour = fg });
    }
    lines.push_back(std::move(current));
    if (lines.size() > 1 && lines.back().empty()) {
        lines.pop_back();
    }
    return lines;
}
