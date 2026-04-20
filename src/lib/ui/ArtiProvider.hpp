//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "command/CommandId.hpp"
#include <unordered_map>

namespace fbide {

/// Maps command IDs to toolbar/menu bitmaps.
/// Decoupled from wxArtProvider — owned directly by UIManager.
class ArtiProvider final {
public:
    NO_COPY_AND_MOVE(ArtiProvider)

    ArtiProvider();

    /// Get bitmap for the given command ID. Returns wxNullBitmap if unknown.
    [[nodiscard]] auto getBitmap(CommandId id) const -> wxBitmap;

private:
    std::unordered_map<CommandId, const char* const*> m_icons;
};

} // namespace fbide
