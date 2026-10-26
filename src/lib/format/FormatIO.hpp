//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "document/TextEncoding.hpp"

namespace fbide {

/// Look up an encoding by its editor name (`UTF-8`, `Windows-1252`, …),
/// ignoring ASCII case. Used by `fbide format --encoding <name>`.
[[nodiscard]] auto parseEncodingName(std::string_view name) -> std::optional<TextEncoding>;

/// Every accepted encoding name, comma-separated — for help and error text.
[[nodiscard]] auto encodingNames() -> std::string;

/// Encode `text` in `encoding`, the encoding's BOM (if any) first — the same
/// bytes `DocumentIO::save` would write. Nullopt when some character cannot
/// be represented in `encoding`.
[[nodiscard]] auto encodeWithBom(const wxString& text, TextEncoding encoding) -> std::optional<std::string>;

} // namespace fbide
