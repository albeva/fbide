//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

namespace fbide {

/// Compiler-dimension UI state — drives the override behaviour of
/// `UIManager::syncBuildCommands` and the status-bar feedback for
/// long-running compile / run jobs.
///
/// Build-command availability outside a compile (`None`) is sourced
/// from `ProjectBase::getCapabilities()` on the active project, not from
/// this enum. Edit-command availability is driven by the active
/// `Document` via `UIManager::syncDocCommands`.
enum class UIState : std::uint8_t {
    None,      ///< No compile / run in flight; build commands reflect project capabilities.
    Compiling, ///< Compiler process active; build commands frozen, KillProcess enabled.
    Running,   ///< User executable active; build commands frozen, KillProcess enabled.
};

} // namespace fbide
