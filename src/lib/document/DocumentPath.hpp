//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Convert a `wxString` to `std::filesystem::path`. Platform-correct:
/// uses wide chars on Windows and UTF-8 elsewhere — matches the encoding
/// that `wxString` keeps internally on each platform.
[[nodiscard]] auto toFsPath(const wxString& s) -> std::filesystem::path;

/// Convert a `std::filesystem::path` to `wxString`. Inverse of `toFsPath`.
/// Use this only at the boundary where an `fs::path` meets a wx API
/// (dialog title, log message, tab title, `wxFile` constructor, ...).
[[nodiscard]] auto toWxString(const std::filesystem::path& p) -> wxString;

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
