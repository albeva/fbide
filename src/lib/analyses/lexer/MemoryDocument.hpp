//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// Lifted from Lexilla's TestDocument (Copyright 2019 by Neil Hodgson, Scintilla
// license). Renamed and namespaced for use as a headless Scintilla::IDocument
// implementation in fbide. Used to drive FBSciLexer outside the editor (formatter,
// AutoIndent, parity tests).
//
#pragma once
#include "pch.hpp"
#include <string>
#include <string_view>
// clang-format off
#include "ILexer.h"
// clang-format on

namespace fbide {

// Scintilla's IDocument has virtual functions but no virtual destructor.
// Patching the vendored header would change vtable ABI vs the prebuilt
// Lexilla in static-wxWidgets builds, so suppress the warning here.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

/**
 * In-memory implementation of `Scintilla::IDocument`. Holds text,
 * per-byte style bytes, line starts, and per-line state / level.
 * UTF-8 only.
 *
 * Used to drive `FBSciLexer` outside the editor (formatter,
 * AutoIndent, parity tests, intellisense worker).
 *
 * Lifted from Lexilla's `TestDocument` (Neil Hodgson, Scintilla
 * license) and renamed/namespaced for fbide use.
 */
class MemoryDocument final : public Scintilla::IDocument {
    std::string m_text;                   ///< UTF-8 source text.
    std::string m_textStyles;             ///< One style byte per source byte.
    std::vector<Sci_Position> m_lineStarts; ///< Byte offset of each line start.
    std::vector<int> m_lineStates;        ///< Per-line lexer state (Scintilla `LineState`).
    std::vector<int> m_lineLevels;        ///< Per-line fold level.
    Sci_Position m_endStyled = 0;         ///< Watermark — bytes through this position have been styled.

public:
    /// Default-constructed empty document.
    MemoryDocument() = default;
    MemoryDocument(const MemoryDocument&) = delete;
    MemoryDocument(MemoryDocument&&) = delete;
    MemoryDocument& operator=(const MemoryDocument&) = delete;
    MemoryDocument& operator=(MemoryDocument&&) = delete;
    /// Trivial destructor (Scintilla::IDocument deliberately has no virtual dtor).
    ~MemoryDocument() = default;

    /// Replace text and reset styles, line starts, line states, line levels.
    void Set(std::string_view sv);

    /// Highest line index (0-based). 0 for empty input.
    [[nodiscard]] auto MaxLine() const noexcept -> Sci_Position;

    /// Scintilla IDocument version (returns the constant fbide expects).
    int SCI_METHOD Version() const override;
    /// IDocument hook — error reporting (no-op).
    void SCI_METHOD SetErrorStatus(int status) override;
    /// Total document length in bytes.
    Sci_Position SCI_METHOD Length() const override;
    /// Copy `lengthRetrieve` bytes into `buffer` starting at `position`.
    void SCI_METHOD GetCharRange(char* buffer, Sci_Position position, Sci_Position lengthRetrieve) const override;
    /// Style byte at `position`.
    char SCI_METHOD StyleAt(Sci_Position position) const override;
    /// Resolve `position` to a 0-based line index.
    Sci_Position SCI_METHOD LineFromPosition(Sci_Position position) const override;
    /// Byte offset of the start of `line`.
    Sci_Position SCI_METHOD LineStart(Sci_Position line) const override;
    /// Fold level of `line`.
    int SCI_METHOD GetLevel(Sci_Position line) const override;
    /// Set fold level of `line`. Returns the previous value.
    int SCI_METHOD SetLevel(Sci_Position line, int level) override;
    /// Per-line lexer state.
    int SCI_METHOD GetLineState(Sci_Position line) const override;
    /// Set per-line lexer state. Returns the previous value.
    int SCI_METHOD SetLineState(Sci_Position line, int state) override;
    /// IDocument hook — begin styling at `position`. Updates `m_endStyled`.
    void SCI_METHOD StartStyling(Sci_Position position) override;
    /// IDocument hook — apply one `style` byte to the next `length` bytes.
    bool SCI_METHOD SetStyleFor(Sci_Position length, char style) override;
    /// IDocument hook — copy `length` style bytes from `styles`.
    bool SCI_METHOD SetStyles(Sci_Position length, const char* styles) override;
    /// IDocument hook — decoration indicator (no-op).
    void SCI_METHOD DecorationSetCurrentIndicator(int indicator) override;
    /// IDocument hook — decoration fill (no-op).
    void SCI_METHOD DecorationFillRange(Sci_Position position, int value, Sci_Position fillLength) override;
    /// IDocument hook — lexer state transition (no-op).
    void SCI_METHOD ChangeLexerState(Sci_Position start, Sci_Position end) override;
    /// Code page — returns SC_CP_UTF8.
    int SCI_METHOD CodePage() const override;
    /// IDocument hook — DBCS lead-byte check (always false; UTF-8 only).
    bool SCI_METHOD IsDBCSLeadByte(char ch) const override;
    /// Pointer to the start of the byte buffer.
    const char* SCI_METHOD BufferPointer() override;
    /// Indentation (leading whitespace columns) of `line`.
    int SCI_METHOD GetLineIndentation(Sci_Position line) override;
    /// Byte offset of the end of `line` (exclusive of trailing newline).
    Sci_Position SCI_METHOD LineEnd(Sci_Position line) const override;
    /// Move `characterOffset` UTF-8 characters from `positionStart`.
    Sci_Position SCI_METHOD GetRelativePosition(Sci_Position positionStart, Sci_Position characterOffset) const override;
    /// Decode one UTF-8 character at `position`; write its byte width to `pWidth`.
    int SCI_METHOD GetCharacterAndWidth(Sci_Position position, Sci_Position* pWidth) const override;
};

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

} // namespace fbide
