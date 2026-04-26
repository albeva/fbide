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

    /// Get bitmap for the given command ID. Returns wxNullBitmap if unknown.
    [[nodiscard]] auto getBitmap(CommandId id) const -> wxBitmap;
    [[nodiscard]] auto getBitmap(SymbolKind kind) const -> wxBitmap;

private:
    [[nodiscard]] auto make(const char* const* xpm) const -> wxBitmap;

    std::unordered_map<CommandId, const char* const*> m_commandIcons;
    std::unordered_map<SymbolKind, const char* const*> m_symbolIcons;
};

} // namespace fbide
