//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
// Re-exported for existing callers — `toFsPath` / `toWxString` now live in
// `utils/PathConversions.hpp` so they're available everywhere, not just
// after pulling in document/DocumentPath.hpp.
#include "utils/PathConversions.hpp"

namespace fbide {

/// Canonical (tidy) form of a filesystem path: absolute, with `.`/`..` and
/// symlinks resolved for any portion that exists on disk (via
/// `std::filesystem::weakly_canonical`). Used for storing/displaying document
/// paths.
///
/// NOTE: this does NOT normalise case, so it is NOT a reliable identity key.
/// `weakly_canonical`'s case-folding is implementation-defined — MSVC reflects
/// the on-disk casing, MinGW/libc++ keep the input casing. To test whether two
/// paths name the same file (case-insensitive FS, symlinks, 8.3 names) use
/// `std::filesystem::equivalent` instead (see `DocumentManager::findByPath`).
///
/// Non-existing portions are kept verbatim, so this is safe to call on a
/// `Save As` target that has not been written yet. Never throws — on
/// `filesystem_error` (invalid encoding, etc.) the input is returned
/// unchanged.
[[nodiscard]] auto canonicalizePath(const std::filesystem::path& path) -> std::filesystem::path;

} // namespace fbide
