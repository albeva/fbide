//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "TextEncoding.hpp"

namespace fbide {

/// Detects text encoding and line-ending style from raw byte buffers.
/// Stateless — all entry points are static. Wraps wxWidgets BOM and
/// UTF-8 validation primitives in a project-specific result shape that
/// makes detection transparent to the caller (unlike wxConvAuto).
class EncodingDetector {
public:
    struct DetectionResult {
        TextEncoding encoding;
        bool hadBom;
    };

    /// Full detection pipeline: BOM sniff → UTF-8 validate → fallback.
    [[nodiscard]] static auto detect(const void* bytes, std::size_t len, TextEncoding fallback) -> DetectionResult;

    /// BOM-based detection only. Returns the encoding indicated by the
    /// BOM, or nullopt if no BOM is present or buffer is too short.
    /// Wraps wxConvAuto::DetectBOM.
    [[nodiscard]] static auto detectBom(const void* bytes, std::size_t len) -> std::optional<TextEncoding>;

    /// True if the byte sequence is a valid UTF-8 encoding.
    /// Empty buffer is considered valid.
    [[nodiscard]] static auto isValidUtf8(const void* bytes, std::size_t len) -> bool;

    /// True if the byte sequence is valid UTF-8 AND contains at least one
    /// non-ASCII byte. Pure-ASCII buffers return false since they are
    /// ambiguous with many single-byte encodings (Windows-1252, CP437...).
    [[nodiscard]] static auto isLikelyUtf8(const void* bytes, std::size_t len) -> bool;

    /// EOL-mode detection based on line-break counts in the given text.
    /// Returns nullopt if the text has no line breaks or ties between styles.
    [[nodiscard]] static auto detectEol(const wxString& text) -> std::optional<EolMode>;
};

} // namespace fbide
