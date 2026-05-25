//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiTypes.hpp"
using namespace fbide;
using namespace fbide::ai;

auto fbide::ai::joinSystem(const std::vector<AiContent>& blocks) -> wxString {
    wxString out;
    for (const auto& block : blocks) {
        if (block.text.empty()) {
            continue;
        }
        if (!out.empty()) {
            out += "\n\n";
        }
        out += block.text;
    }
    return out;
}
