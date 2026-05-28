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

/// Canonical form of a filesystem path used for identity comparisons across
/// the document manager.
///
/// Returns an absolute path with `.`/`..` resolved and symlinks resolved for
/// any portion that exists on disk (via `std::filesystem::weakly_canonical`).
/// On case-insensitive filesystems (macOS, Windows) the result reflects the
/// on-disk casing — so `fbgfx.bi` and `FBGFX.bi` canonicalize to the same
/// path, eliminating the duplicate-tab bug.
///
/// Non-existing portions are kept verbatim, so this is safe to call on a
/// `Save As` target that has not been written yet. Never throws — on
/// `filesystem_error` (invalid encoding, etc.) the input is returned
/// unchanged.
[[nodiscard]] auto canonicalizePath(const std::filesystem::path& path) -> std::filesystem::path;

} // namespace fbide
