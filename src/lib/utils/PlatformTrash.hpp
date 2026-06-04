//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
// NOTE: intentionally does NOT include pch.hpp. This header is also pulled
// into PlatformTrash.mm (Objective-C++), which must not drag the wxWidgets
// precompiled header into a Foundation translation unit. Keep it minimal.
#include <filesystem>

namespace fbide {

/// Move a file or directory to the OS trash / recycle bin so the user can
/// recover it, rather than permanently deleting it. Returns true on success.
///
/// Per-platform implementation: macOS `NSFileManager trashItemAtURL`
/// (PlatformTrash.mm), Windows `SHFileOperation` with `FOF_ALLOWUNDO`, and
/// Linux/other Unix `gio trash` (both in PlatformTrash.cpp).
[[nodiscard]] auto moveToTrash(const std::filesystem::path& path) -> bool;

} // namespace fbide
