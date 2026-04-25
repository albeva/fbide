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

/// In-memory implementation of `Scintilla::IDocument`. Holds text, per-byte
/// style bytes, line starts, and per-line state/level. UTF-8 only.
class MemoryDocument final : public Scintilla::IDocument {
    std::string m_text;
    std::string m_textStyles;
    std::vector<Sci_Position> m_lineStarts;
    std::vector<int> m_lineStates;
    std::vector<int> m_lineLevels;
    Sci_Position m_endStyled = 0;

public:
    MemoryDocument() = default;
    MemoryDocument(const MemoryDocument&) = delete;
    MemoryDocument(MemoryDocument&&) = delete;
    MemoryDocument& operator=(const MemoryDocument&) = delete;
    MemoryDocument& operator=(MemoryDocument&&) = delete;
    virtual ~MemoryDocument() = default;

    /// Replace text and reset styles, line starts, line states, line levels.
    void Set(std::string_view sv);

    [[nodiscard]] auto MaxLine() const noexcept -> Sci_Position;

    int SCI_METHOD Version() const override;
    void SCI_METHOD SetErrorStatus(int status) override;
    Sci_Position SCI_METHOD Length() const override;
    void SCI_METHOD GetCharRange(char* buffer, Sci_Position position, Sci_Position lengthRetrieve) const override;
    char SCI_METHOD StyleAt(Sci_Position position) const override;
    Sci_Position SCI_METHOD LineFromPosition(Sci_Position position) const override;
    Sci_Position SCI_METHOD LineStart(Sci_Position line) const override;
    int SCI_METHOD GetLevel(Sci_Position line) const override;
    int SCI_METHOD SetLevel(Sci_Position line, int level) override;
    int SCI_METHOD GetLineState(Sci_Position line) const override;
    int SCI_METHOD SetLineState(Sci_Position line, int state) override;
    void SCI_METHOD StartStyling(Sci_Position position) override;
    bool SCI_METHOD SetStyleFor(Sci_Position length, char style) override;
    bool SCI_METHOD SetStyles(Sci_Position length, const char* styles) override;
    void SCI_METHOD DecorationSetCurrentIndicator(int indicator) override;
    void SCI_METHOD DecorationFillRange(Sci_Position position, int value, Sci_Position fillLength) override;
    void SCI_METHOD ChangeLexerState(Sci_Position start, Sci_Position end) override;
    int SCI_METHOD CodePage() const override;
    bool SCI_METHOD IsDBCSLeadByte(char ch) const override;
    const char* SCI_METHOD BufferPointer() override;
    int SCI_METHOD GetLineIndentation(Sci_Position line) override;
    Sci_Position SCI_METHOD LineEnd(Sci_Position line) const override;
    Sci_Position SCI_METHOD GetRelativePosition(Sci_Position positionStart, Sci_Position characterOffset) const override;
    int SCI_METHOD GetCharacterAndWidth(Sci_Position position, Sci_Position* pWidth) const override;
};

} // namespace fbide
