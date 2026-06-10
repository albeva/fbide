//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Linux/AppImage-only: publish FBIde's desktop entry, MIME-type definitions
/// and document icons into the user's data dir (~/.local/share) so .bas/.bi/.fbs
/// show FBIde's icons and open with FBIde from the file manager and app menu.
/// A bundled AppImage's own share/ tree is invisible to the system, so the
/// assets must be copied out and the desktop/MIME/icon caches refreshed — the
/// freedesktop analogue of the Windows per-user registry registration.
///
/// Only acts when running as an AppImage ($APPIMAGE is set); a distro/portable
/// install gets these files from its package instead. Deleting the AppImage
/// leaves a dangling menu entry — the accepted self-integration tradeoff; full
/// cleanup is appimaged's job.
class FileAssociationsLinux final {
public:
    /// Integrate on launch when running as an AppImage. Cheap and idempotent:
    /// a stamp file short-circuits repeat runs unless the AppImage changed.
    /// No-op on non-GTK platforms and outside an AppImage.
    static void ensureRegistered();
};

} // namespace fbide
