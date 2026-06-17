//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide::reformat {

/// Formatting options controlling how a parse tree is rendered back to text.
struct FormatOptions {
    std::size_t tabSize = 4; ///< Indent width in spaces.
    bool anchoredPP = false; ///< When true, preprocessor directives anchor at column 0.
    bool reIndent = true;    ///< Apply structural indentation.
    bool reFormat = true;    ///< Apply inter-token spacing + blank-line policy.
};

} // namespace fbide::reformat
