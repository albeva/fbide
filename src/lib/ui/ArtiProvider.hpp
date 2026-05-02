//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "command/CommandId.hpp"

namespace fbide {
enum class SymbolKind : std::uint8_t;

/// Maps command IDs to toolbar/menu bitmaps.
/// Decoupled from wxArtProvider — owned directly by UIManager.
class ArtiProvider final {
public:
    NO_COPY_AND_MOVE(ArtiProvider)

    ArtiProvider();

    /// Bitmap for the given command ID. Returns `wxNullBitmap` if unknown.
    [[nodiscard]] auto getBitmap(CommandId id) const -> wxBitmap;
    /// Bitmap for the given symbol kind. Returns `wxNullBitmap` if unknown.
    [[nodiscard]] auto getBitmap(SymbolKind kind) const -> wxBitmap;

private:
    /// Build a `wxBitmap` from raw XPM data.
    [[nodiscard]] auto make(const char* const* xpm) const -> wxBitmap;

    std::unordered_map<CommandId, const char* const*> m_commandIcons;  ///< CommandId → XPM mapping.
    std::unordered_map<SymbolKind, const char* const*> m_symbolIcons;  ///< SymbolKind → XPM mapping.
};

} // namespace fbide
