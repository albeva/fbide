//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Complete editor theme loaded from legacy .fbt files.
class Theme final {
public:
    /// Font style flags matching legacy .fbt format.
    enum class FontStyle : std::uint8_t {
        None = 0,
        Bold = 1,
        Italic = 2,
        Underline = 4,
        Hidden = 8,
    };

    /// Background/foreground colour pair.
    struct ColorPair final {
        wxColour background;
        wxColour foreground;
    };

    /// Brace style — colours plus font style.
    struct BraceStyle final {
        wxColour background;
        wxColour foreground;
        FontStyle fontStyle = FontStyle::None;
    };

    /// Default editor style — global colours, caret, font.
    struct EditorStyle final {
        wxColour background = *wxWHITE;
        wxColour foreground = *wxBLACK;
        wxColour caretColour = *wxBLACK;
        wxColour caretLine { 0xDD, 0xDD, 0xDD };
        wxString fontName;
        int fontSize = 12;
        FontStyle fontStyle = FontStyle::None;
    };

    /// Per-syntax-element style (comment, keyword, string, etc.)
    struct ItemStyle final {
        wxColour foreground;
        wxColour background;
        wxString fontName;
        int fontSize = 0;
        FontStyle fontStyle = FontStyle::None;
        int letterCase = 0;
    };

    /// Style type indices matching legacy .fbt section order.
    enum ItemKind : int {
        Default = 0,
        Comment,
        Number,
        Keyword,
        String,
        Preprocessor,
        Operator,
        Identifier,
        Date,
        StringEol,
        Keyword2,
        Keyword3,
        Keyword4,
        Constant,
        Asm,
    };

    static constexpr int KIND_COUNT = 15;

    /// Load theme from a legacy .fbt file.
    void load(const wxString& path);

    /// Set theme file path
    void setPath(const wxString& path) { m_themePath = path; }

    /// Save theme to a .fbt file.
    void save() const;

    /// Global defaults (background, foreground, caret, font).
    [[nodiscard]] auto getDefault() const -> const EditorStyle& { return m_editor; }
    [[nodiscard]] auto getDefault() -> EditorStyle& { return m_editor; }

    /// Line number margin colours.
    [[nodiscard]] auto getLineNumber() const -> const ColorPair& { return m_lineNumber; }
    [[nodiscard]] auto getLineNumber() -> ColorPair& { return m_lineNumber; }

    /// Selection colours.
    [[nodiscard]] auto getSelection() const -> const ColorPair& { return m_selection; }
    [[nodiscard]] auto getSelection() -> ColorPair& { return m_selection; }

    /// Matching brace style.
    [[nodiscard]] auto getBrace() const -> const BraceStyle& { return m_brace; }
    [[nodiscard]] auto getBrace() -> BraceStyle& { return m_brace; }

    /// Mismatched brace style.
    [[nodiscard]] auto getBadBrace() const -> const BraceStyle& { return m_badBrace; }
    [[nodiscard]] auto getBadBrace() -> BraceStyle& { return m_badBrace; }

    /// Get style entry by index.
    [[nodiscard]] auto getStyle(const ItemKind index) const -> const ItemStyle& { return m_styles[static_cast<size_t>(index)]; }
    [[nodiscard]] auto getStyle(const ItemKind index) -> ItemStyle& { return m_styles[static_cast<size_t>(index)]; }

private:
    wxString m_themePath;

    EditorStyle m_editor {};
    ColorPair m_lineNumber {};
    ColorPair m_selection {};
    BraceStyle m_brace {};
    BraceStyle m_badBrace {};
    std::array<ItemStyle, KIND_COUNT> m_styles {};
};

} // namespace fbide
