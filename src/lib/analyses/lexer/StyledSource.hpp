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

    [[nodiscard]] virtual auto length() const -> Sci_PositionU = 0;
    [[nodiscard]] virtual auto styleAt(Sci_PositionU pos) const -> ThemeCategory = 0;
    virtual void getCharRange(char* buffer, Sci_PositionU pos, Sci_PositionU len) const = 0;

    [[nodiscard]] virtual auto lineFromPosition(Sci_PositionU pos) const -> Sci_Position = 0;
    [[nodiscard]] virtual auto lineState(Sci_Position line) const -> FBSciLexer::LineState = 0;
};

/// Adapter over `MemoryDocument` (or anything implementing `Scintilla::IDocument`).
class MemoryDocStyledSource final : public IStyledSource {
    MemoryDocument& m_doc;

public:
    explicit MemoryDocStyledSource(MemoryDocument& doc)
    : m_doc(doc) {}

    [[nodiscard]] auto length() const -> Sci_PositionU override {
        return static_cast<Sci_PositionU>(m_doc.Length());
    }
    [[nodiscard]] auto styleAt(const Sci_PositionU pos) const -> ThemeCategory override {
        return static_cast<ThemeCategory>(m_doc.StyleAt(static_cast<Sci_Position>(pos)));
    }
    void getCharRange(char* buffer, const Sci_PositionU pos, const Sci_PositionU len) const override {
        m_doc.GetCharRange(buffer, static_cast<Sci_Position>(pos), static_cast<Sci_Position>(len));
    }
    [[nodiscard]] auto lineFromPosition(const Sci_PositionU pos) const -> Sci_Position override {
        return m_doc.LineFromPosition(static_cast<Sci_Position>(pos));
    }
    [[nodiscard]] auto lineState(const Sci_Position line) const -> FBSciLexer::LineState override {
        return FBSciLexer::LineState::fromInt(m_doc.GetLineState(line));
    }
};

/// Adapter over `wxStyledTextCtrl`. Used by the editor for on-type transforms.
/// Out-of-line because it needs `<wx/stc/stc.h>` symbols.
class WxStcStyledSource final : public IStyledSource {
    wxStyledTextCtrl& m_stc;

public:
    explicit WxStcStyledSource(wxStyledTextCtrl& stc)
    : m_stc(stc) {}

    [[nodiscard]] auto length() const -> Sci_PositionU override;
    [[nodiscard]] auto styleAt(Sci_PositionU pos) const -> ThemeCategory override;
    void getCharRange(char* buffer, Sci_PositionU pos, Sci_PositionU len) const override;
    [[nodiscard]] auto lineFromPosition(Sci_PositionU pos) const -> Sci_Position override;
    [[nodiscard]] auto lineState(Sci_Position line) const -> FBSciLexer::LineState override;
};

} // namespace fbide::lexer
