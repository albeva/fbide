//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
// Self-contained (no PCH): this header is also pulled in by MacFileIcons.mm,
// which is built with SKIP_PRECOMPILE_HEADERS.
#include <wx/bitmap.h>
#include <wx/string.h>

namespace fbide {

/// Native macOS file-type icon (NSWorkspace) for `ext` — a lowercase extension
/// with no leading dot; empty yields the generic document icon. Rendered at
/// `logicalPx` square, scaled to the main display's backing factor so it stays
/// crisp on Retina. macOS only — defined in MacFileIcons.mm.
[[nodiscard]] auto nativeFileIcon(const wxString& ext, int logicalPx) -> wxBitmap;

/// Native macOS folder icon, rendered like `nativeFileIcon`.
[[nodiscard]] auto nativeFolderIcon(int logicalPx) -> wxBitmap;

} // namespace fbide
