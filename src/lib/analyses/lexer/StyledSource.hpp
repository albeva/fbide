//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
// clang-format off
#include "ILexer.h"
// clang-format on
#include "MemoryDocument.hpp"
#include "config/ThemeCategory.hpp"
#include "editor/lexilla/FBSciLexer.hpp"

class wxStyledTextCtrl;

namespace fbide::lexer {

/**
 * Read-only view over a styled source: bytes + per-byte style +
 * per-line state.
 *
 * The style-walking `Lexer` talks to this interface so it works
 * against both a headless `MemoryDocument` (formatter, AutoIndent,
 * tests) and a live `wxStyledTextCtrl` (in-editor on-type
 * transforms).
 */
class IStyledSource {
public:
    virtual ~IStyledSource() = default;

    /// Total document length in bytes.
    [[nodiscard]] virtual auto length() const -> Sci_PositionU = 0;
    /// Style category at byte offset `pos`.
    [[nodiscard]] virtual auto styleAt(Sci_PositionU pos) const -> ThemeCategory = 0;
    /// Copy `len` bytes into `buffer` starting at `pos`.
    virtual void getCharRange(char* buffer, Sci_PositionU pos, Sci_PositionU len) const = 0;

    /// Resolve `pos` to a 0-based line index.
    [[nodiscard]] virtual auto lineFromPosition(Sci_PositionU pos) const -> Sci_Position = 0;
    /// Per-line lexer state for `line`.
    [[nodiscard]] virtual auto lineState(Sci_Position line) const -> FBSciLexer::LineState = 0;
};

/// Adapter over `MemoryDocument` (or anything implementing `Scintilla::IDocument`).
class MemoryDocStyledSource final : public IStyledSource {
    MemoryDocument& m_doc; ///< Underlying document.

public:
    /// Wrap `doc`. Reference must outlive this adapter.
    explicit MemoryDocStyledSource(MemoryDocument& doc)
    : m_doc(doc) {}

    /// @copydoc IStyledSource::length
    [[nodiscard]] auto length() const -> Sci_PositionU override {
        return static_cast<Sci_PositionU>(m_doc.Length());
    }
    /// @copydoc IStyledSource::styleAt
    [[nodiscard]] auto styleAt(const Sci_PositionU pos) const -> ThemeCategory override {
        return static_cast<ThemeCategory>(m_doc.StyleAt(static_cast<Sci_Position>(pos)));
    }
    /// @copydoc IStyledSource::getCharRange
    void getCharRange(char* buffer, const Sci_PositionU pos, const Sci_PositionU len) const override {
        m_doc.GetCharRange(buffer, static_cast<Sci_Position>(pos), static_cast<Sci_Position>(len));
    }
    /// @copydoc IStyledSource::lineFromPosition
    [[nodiscard]] auto lineFromPosition(const Sci_PositionU pos) const -> Sci_Position override {
        return m_doc.LineFromPosition(static_cast<Sci_Position>(pos));
    }
    /// @copydoc IStyledSource::lineState
    [[nodiscard]] auto lineState(const Sci_Position line) const -> FBSciLexer::LineState override {
        return FBSciLexer::LineState::fromInt(m_doc.GetLineState(line));
    }
};

/// Adapter over `wxStyledTextCtrl`. Used by the editor for on-type transforms.
/// Out-of-line because it needs `<wx/stc/stc.h>` symbols.
class WxStcStyledSource final : public IStyledSource {
    wxStyledTextCtrl& m_stc; ///< Underlying editor widget.

public:
    /// Wrap `stc`. Reference must outlive this adapter.
    explicit WxStcStyledSource(wxStyledTextCtrl& stc)
    : m_stc(stc) {}

    /// @copydoc IStyledSource::length
    [[nodiscard]] auto length() const -> Sci_PositionU override;
    /// @copydoc IStyledSource::styleAt
    [[nodiscard]] auto styleAt(Sci_PositionU pos) const -> ThemeCategory override;
    /// @copydoc IStyledSource::getCharRange
    void getCharRange(char* buffer, Sci_PositionU pos, Sci_PositionU len) const override;
    /// @copydoc IStyledSource::lineFromPosition
    [[nodiscard]] auto lineFromPosition(Sci_PositionU pos) const -> Sci_Position override;
    /// @copydoc IStyledSource::lineState
    [[nodiscard]] auto lineState(Sci_Position line) const -> FBSciLexer::LineState override;
};

} // namespace fbide::lexer
