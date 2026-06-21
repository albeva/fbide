//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/**
 * Thin wrappers over platform shell features the file browser exposes
 * through its context menu. Every method takes an absolute filesystem path.
 * Implementations are `#ifdef`-gated per OS.
 */
class SystemShell final {
public:
    /// Reveal `path` in the system file manager, selecting it where the
    /// platform supports it (Windows Explorer, macOS Finder); elsewhere opens
    /// the containing folder.
    static void revealInFileManager(const wxString& path);

    /// Move `path` (file or directory) to the OS recycle bin / trash. Returns
    /// false when the platform has no trash support or the operation failed —
    /// the caller may then fall back to a permanent delete.
    [[nodiscard]] static auto moveToTrash(const wxString& path) -> bool;

    /// Open the native file-properties dialog for `path`. No-op where
    /// unsupported; gate the menu entry on `propertiesSupported`.
    static void showProperties(const wxString& path);

    /// True when `showProperties` is implemented on this platform.
    [[nodiscard]] static auto propertiesSupported() -> bool;

    /// Launch `terminalCommand` with its working directory set to `dir`.
    static void openTerminal(const wxString& dir, const wxString& terminalCommand);
};

} // namespace fbide
