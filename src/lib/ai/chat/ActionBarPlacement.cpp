//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ActionBarPlacement.hpp"
using namespace fbide;
using namespace fbide::ai;

auto ActionBarPlacement::computePosition(
    const int codeRightDoc,
    const int codeTopDoc,
    const int originY,
    const int viewTopClient,
    const wxSize barSize
) -> Position {
    const int codeTopClient = codeTopDoc - originY;
    const int xClient = codeRightDoc - barSize.GetWidth() - kInset;

    // Attached when the block's top edge is inside the visible area;
    // the bar tracks the block and scrolls with the content. Detached
    // when the top has scrolled above the viewport — pin the bar just
    // below the scroll surface's own top so it stays visible.
    if (codeTopClient >= 0) {
        return { .x = xClient, .y = codeTopClient + kInset };
    }
    return { .x = xClient, .y = viewTopClient + kInset };
}
