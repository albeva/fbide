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
    /// Successful load result — text plus the detected encoding/EOL.
    struct LoadResult {
        wxString text;          ///< Decoded text.
        TextEncoding encoding;  ///< Encoding the bytes were decoded with.
        EolMode eolMode;        ///< Detected (or fallback) EOL mode.
    };

    /// Save outcome.
    enum class SaveResult : std::uint8_t {
        Success,        ///< File written.
        IOError,        ///< Could not open or write the file.
        EncodingError,  ///< Codec rejected text — nothing written.
    };

    /// Load file into memory. Returns nullopt only when the file cannot
    /// be opened or read. Decoding never fails — if the chosen codec
    /// rejects the bytes, the payload is reloaded as ISO-8859-1 so the
    /// user always sees something.
    [[nodiscard]] static auto load(
        const wxString& path,
        TextEncoding defaultEncoding,
        EolMode defaultEol
    ) -> std::optional<LoadResult>;

    /// Load file forcing the given encoding — bypasses encoding detection.
    /// Used for "Reload with Encoding ..." where the user explicitly
    /// overrides auto-detection. Returns nullopt only on I/O failure;
    /// decode falls back to ISO-8859-1 if the forced encoding rejects
    /// the bytes.
    [[nodiscard]] static auto loadWithEncoding(
        const wxString& path,
        TextEncoding encoding,
        EolMode defaultEol
    ) -> std::optional<LoadResult>;

    /// Save text to disk in the given encoding.
    /// Returns:
    ///   Success       — file written.
    ///   IOError       — could not open/write the file (disk full, permissions).
    ///   EncodingError — chosen encoding cannot represent some characters;
    ///                   nothing was written, caller should surface the
    ///                   failure and leave the document dirty.
    [[nodiscard]] static auto save(const wxString& path,
        const wxString& text,
        TextEncoding encoding,
        EolMode eolMode) -> SaveResult;
};

} // namespace fbide
