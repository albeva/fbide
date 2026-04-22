//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/geobide
//
#pragma once
#include "pch.hpp"
#include "TextEncoding.hpp"

namespace fbide {

/// File I/O for text documents. Replaces wxSTC::LoadFile / SaveFile with
/// explicit encoding + EOL handling. Detection on load uses EncodingDetector;
/// save path strips/adds BOM and converts EOLs before encoding.
class DocumentIO {
public:
    struct LoadResult {
        wxString text;
        TextEncoding encoding;
        EolMode eolMode;
    };

    /// Load file into memory. Pipeline:
    ///   1. Read raw bytes
    ///   2. Detect encoding via EncodingDetector::detect (BOM -> UTF-8 validate -> fallback)
    ///   3. Strip BOM if present
    ///   4. Decode payload to wxString
    ///   5. Detect EOL mode from first 8KB; fall back to `defaultEol`
    /// Returns nullopt if the file can't be opened or decoding fails.
    [[nodiscard]] static auto load(
        const wxString& path,
        TextEncoding defaultEncoding,
        EolMode defaultEol
    ) -> std::optional<LoadResult>;

    /// Load file forcing the given encoding — bypasses encoding detection.
    /// Used for "Reload with Encoding ..." where the user explicitly
    /// overrides auto-detection (e.g. the file was mis-detected). BOM
    /// bytes are still stripped when the chosen encoding expects one.
    /// EOL is detected from the decoded text (falls back to `defaultEol`).
    [[nodiscard]] static auto loadWithEncoding(
        const wxString& path,
        TextEncoding encoding,
        EolMode defaultEol
    ) -> std::optional<LoadResult>;

    /// Save text to disk in the given encoding. Pipeline:
    ///   1. Convert EOLs in `text` to match `eolMode`
    ///   2. Encode via TextEncoding::encode
    ///   3. Prefix BOM if encoding requires it
    ///   4. Write bytes to `path` (truncating)
    /// Returns false if encoding rejects characters unrepresentable in
    /// the target codepage (callers should surface this — e.g. suggest
    /// UTF-8) or if the file could not be opened/written.
    [[nodiscard]] static auto save(const wxString& path,
        const wxString& text,
        TextEncoding encoding,
        EolMode eolMode) -> bool;
};

} // namespace fbide
