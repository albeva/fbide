//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

namespace fbide {

/// Unified UI state that determines which menus and toolbar items are enabled.
/// Set via UIManager::setState(). Higher states take precedence.
enum class UIState {
    None,                   // No document open — everything disabled
    FocusedUnknownFile,     // Non-compilable document focused (HTML, text)
    FocusedValidSourceFile, // FreeBASIC document focused — can compile/run
    Compiling,              // Compiler running — run menus disabled
    Running,                // Executable running — run menus disabled
};

} // namespace fbide
