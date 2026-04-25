//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// Lifted from Lexilla's TestDocument (Copyright 2019 by Neil Hodgson, Scintilla
// license). Implementation kept faithful to upstream so future Lexilla updates
// are easy to merge.
//
#include "MemoryDocument.hpp"
#include <algorithm>
#include <cassert>
using namespace fbide;

namespace {

constexpr unsigned char UTF8BytesOfLead[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 00 - 0F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 10 - 1F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 20 - 2F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 30 - 3F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 40 - 4F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 50 - 5F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 60 - 6F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 70 - 7F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 80 - 8F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 90 - 9F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // A0 - AF
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // B0 - BF
    1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // C0 - CF
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // D0 - DF
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // E0 - EF
    4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // F0 - FF
};

auto UnicodeFromUTF8(const unsigned char* us) noexcept -> int {
    assert(us);
    switch (UTF8BytesOfLead[us[0]]) {
    case 1:
        return us[0];
    case 2:
        return ((us[0] & 0x1F) << 6) + (us[1] & 0x3F);
    case 3:
        return ((us[0] & 0xF) << 12) + ((us[1] & 0x3F) << 6) + (us[2] & 0x3F);
    default:
        return ((us[0] & 0x7) << 18) + ((us[1] & 0x3F) << 12) + ((us[2] & 0x3F) << 6) + (us[3] & 0x3F);
    }
}

constexpr auto UTF8IsTrailByte(unsigned char ch) noexcept -> bool {
    return ch >= 0x80 && ch < 0xc0;
}

} // namespace

#if defined(_MSC_VER)
// IDocument interface does not specify noexcept so best to not add it to implementation
#pragma warning(disable: 26440)
#endif

void MemoryDocument::Set(std::string_view sv) {
    m_text = sv;
    m_textStyles.assign(m_text.size() + 1, '\0');
    m_lineStarts.clear();
    m_endStyled = 0;
    m_lineStarts.push_back(0);
    for (std::size_t pos = 0; pos < m_text.length(); pos++) {
        if (m_text[pos] == '\n') {
            m_lineStarts.push_back(static_cast<Sci_Position>(pos + 1));
        }
    }
    if (m_lineStarts.back() != Length()) {
        m_lineStarts.push_back(Length());
    }
    m_lineStates.assign(m_lineStarts.size() + 1, 0);
    m_lineLevels.assign(m_lineStarts.size(), 0x400);
}

auto MemoryDocument::MaxLine() const noexcept -> Sci_Position {
    return static_cast<Sci_Position>(m_lineStarts.size() - 1);
}

int SCI_METHOD MemoryDocument::Version() const {
    return Scintilla::dvRelease4;
}

void SCI_METHOD MemoryDocument::SetErrorStatus(int) {
}

Sci_Position SCI_METHOD MemoryDocument::Length() const {
    return static_cast<Sci_Position>(m_text.length());
}

void SCI_METHOD MemoryDocument::GetCharRange(char* buffer, const Sci_Position position, const Sci_Position lengthRetrieve) const {
    m_text.copy(buffer, static_cast<std::size_t>(lengthRetrieve), static_cast<std::size_t>(position));
}

char SCI_METHOD MemoryDocument::StyleAt(const Sci_Position position) const {
    if (position < 0) {
        return 0;
    }
    return m_textStyles.at(static_cast<std::size_t>(position));
}

Sci_Position SCI_METHOD MemoryDocument::LineFromPosition(const Sci_Position position) const {
    if (position >= Length()) {
        return MaxLine();
    }
    const auto it = std::lower_bound(m_lineStarts.begin(), m_lineStarts.end(), position);
    auto line = it - m_lineStarts.begin();
    if (*it > position) {
        line--;
    }
    return line;
}

Sci_Position SCI_METHOD MemoryDocument::LineStart(const Sci_Position line) const {
    if (line < 0) {
        return 0;
    }
    if (line >= static_cast<Sci_Position>(m_lineStarts.size())) {
        return Length();
    }
    return m_lineStarts.at(static_cast<std::size_t>(line));
}

int SCI_METHOD MemoryDocument::GetLevel(const Sci_Position line) const {
    return m_lineLevels.at(static_cast<std::size_t>(line));
}

int SCI_METHOD MemoryDocument::SetLevel(const Sci_Position line, const int level) {
    if (line == static_cast<Sci_Position>(m_lineLevels.size())) {
        return 0x400;
    }
    return m_lineLevels.at(static_cast<std::size_t>(line)) = level;
}

int SCI_METHOD MemoryDocument::GetLineState(const Sci_Position line) const {
    return m_lineStates.at(static_cast<std::size_t>(line));
}

int SCI_METHOD MemoryDocument::SetLineState(const Sci_Position line, const int state) {
    return m_lineStates.at(static_cast<std::size_t>(line)) = state;
}

void SCI_METHOD MemoryDocument::StartStyling(const Sci_Position position) {
    m_endStyled = position;
}

bool SCI_METHOD MemoryDocument::SetStyleFor(const Sci_Position length, const char style) {
    for (Sci_Position i = 0; i < length; i++) {
        m_textStyles.at(static_cast<std::size_t>(m_endStyled)) = style;
        m_endStyled++;
    }
    return true;
}

bool SCI_METHOD MemoryDocument::SetStyles(const Sci_Position length, const char* styles) {
    assert(styles);
    for (Sci_Position i = 0; i < length; i++) {
        m_textStyles.at(static_cast<std::size_t>(m_endStyled)) = static_cast<char>(styles[i]);
        m_endStyled++;
    }
    return true;
}

void SCI_METHOD MemoryDocument::DecorationSetCurrentIndicator(int) {
    // Not implemented as no way to read decorations
}

void SCI_METHOD MemoryDocument::DecorationFillRange(Sci_Position, int, Sci_Position) {
    // Not implemented as no way to read decorations
}

void SCI_METHOD MemoryDocument::ChangeLexerState(Sci_Position, Sci_Position) {
    // Not implemented as no watcher to trigger
}

int SCI_METHOD MemoryDocument::CodePage() const {
    return 65001; // UTF-8
}

bool SCI_METHOD MemoryDocument::IsDBCSLeadByte(char) const {
    return false; // UTF-8
}

const char* SCI_METHOD MemoryDocument::BufferPointer() {
    return m_text.c_str();
}

int SCI_METHOD MemoryDocument::GetLineIndentation(Sci_Position) {
    // Never actually called - lexers use Accessor::IndentAmount
    return 0;
}

Sci_Position SCI_METHOD MemoryDocument::LineEnd(const Sci_Position line) const {
    const auto maxLine = MaxLine();
    if (line == maxLine || line == maxLine + 1) {
        return static_cast<Sci_Position>(m_text.length());
    }
    assert(line < maxLine);
    auto position = LineStart(line + 1);
    position--; // Back over CR or LF
    if (position > LineStart(line) && m_text.at(static_cast<std::size_t>(position - 1)) == '\r') {
        position--;
    }
    return position;
}

Sci_Position SCI_METHOD MemoryDocument::GetRelativePosition(const Sci_Position positionStart, Sci_Position characterOffset) const {
    auto pos = positionStart;
    if (characterOffset < 0) {
        while (characterOffset < 0) {
            if (pos <= 0) {
                return -1;
            }
            auto previousByte = static_cast<unsigned char>(m_text.at(static_cast<std::size_t>(pos - 1)));
            if (previousByte < 0x80) {
                pos--;
                characterOffset++;
            } else {
                while (pos > 1 && UTF8IsTrailByte(previousByte)) {
                    pos--;
                    previousByte = static_cast<unsigned char>(m_text.at(static_cast<std::size_t>(pos - 1)));
                }
                pos--;
                characterOffset++;
            }
        }
        return pos;
    }
    assert(characterOffset >= 0);
    while (characterOffset > 0) {
        Sci_Position width = 0;
        GetCharacterAndWidth(pos, &width);
        pos += width;
        characterOffset--;
    }
    return pos;
}

int SCI_METHOD MemoryDocument::GetCharacterAndWidth(const Sci_Position position, Sci_Position* pWidth) const {
    if (position < 0 || position >= Length()) {
        if (pWidth != nullptr) {
            *pWidth = 1;
        }
        return '\0';
    }
    const auto leadByte = static_cast<unsigned char>(m_text.at(static_cast<std::size_t>(position)));
    if (leadByte < 0x80) {
        if (pWidth != nullptr) {
            *pWidth = 1;
        }
        return leadByte;
    }
    const auto widthCharBytes = static_cast<int>(UTF8BytesOfLead[leadByte]);
    unsigned char charBytes[] = { leadByte, 0, 0, 0 };
    for (int b = 1; b < widthCharBytes; b++) {
        charBytes[b] = static_cast<unsigned char>(m_text.at(static_cast<std::size_t>(position + b)));
    }
    if (pWidth != nullptr) {
        *pWidth = widthCharBytes;
    }
    return UnicodeFromUTF8(charBytes);
}
