//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "CodeHighlighter.hpp"
#include "Markdown.hpp"

namespace fbide {

/// Font selection for a run — flags and a size delta relative to the body
/// font. The measurer and the painter both resolve a concrete font from it.
struct TextStyle {
    int sizeDelta = 0;          ///< Point size relative to the body font.
    bool bold = false;          ///< Bold typeface.
    bool italic = false;        ///< Italic typeface.
    bool underline = false;     ///< Underlined (links).
    bool strikethrough = false; ///< Struck through.
    bool monospace = false;     ///< Monospace face (inline + block code).

    auto operator==(const TextStyle&) const -> bool = default;
};

/// Measures text for the layout engine. The production implementation wraps a
/// wxDC; tests inject a deterministic fake, so the wrap logic is unit testable
/// without a GUI.
class TextMeasurer {
public:
    TextMeasurer() = default;
    virtual ~TextMeasurer() = default;
    NO_COPY_AND_MOVE(TextMeasurer)

    /// Pixel width of `text` rendered in `style`.
    [[nodiscard]] virtual auto width(const wxString& text, const TextStyle& style) const -> int = 0;
    /// Pixel line height of `style`, leading included.
    [[nodiscard]] virtual auto lineHeight(const TextStyle& style) const -> int = 0;
};

/// Kind of a laid-out line — selects background and decoration when painting.
enum class LineKind : std::uint8_t {
    Prose, ///< Wrapped prose / heading / list item.
    Code,  ///< A code-block line (or its padding) — painted on the code bg.
    Rule,  ///< A horizontal rule — no runs.
};

/// One painted run: a span of same-styled text at a known offset.
struct PaintRun {
    wxString text;   ///< Run text.
    TextStyle style; ///< Font selection.
    wxColour colour; ///< Foreground colour.
    int x = 0;       ///< Left offset from the document's left edge.
    int width = 0;   ///< Measured pixel width.
    int linkId = -1; ///< Index into `LaidOutDoc::links`, or -1 if not a link.
};

/// One laid-out line, ready to paint.
struct PaintLine {
    LineKind kind = LineKind::Prose;
    int y = 0;          ///< Top offset within the document.
    int height = 0;     ///< Line height in pixels.
    int quoteDepth = 0; ///< Block-quote nesting — the painter draws that many bars.
    std::vector<PaintRun> runs;
};

/// A hyperlink target, referenced by `PaintRun::linkId`.
struct LinkTarget {
    wxString url;
};

/// A laid-out fenced code block — its region within the document plus the
/// raw source, so the view can place a toolbar over it and act on the code.
struct LaidCodeBlock {
    wxString code;  ///< Raw fenced code, '\n'-separated.
    wxString lang;  ///< Fence info string (lowercased).
    int y = 0;      ///< Top offset within the document (includes padding).
    int height = 0; ///< Total height including padding strips.
};

/// Colours the layout and painter need that are not carried on code runs.
struct ChatPalette {
    wxColour text;         ///< Body prose colour.
    wxColour link;         ///< Hyperlink colour.
    wxColour codeBg;       ///< Code-block background.
    wxColour inlineCodeBg; ///< Inline `code` background.
    wxColour rule;         ///< Horizontal-rule colour.
};

/// A fully laid-out document — stacked, wrapped lines plus link targets.
struct LaidOutDoc {
    std::vector<PaintLine> lines;
    std::vector<LinkTarget> links;
    std::vector<LaidCodeBlock> codeBlocks; ///< Fenced code-block regions.
    int width = 0;                         ///< Width the document was laid out for.
    int height = 0;                        ///< Total stacked height.
};

/// Highlights a fenced code block — `code` body, `lang` fence tag — into
/// coloured lines. Injected so the layout stays independent of the lexer.
using CodeFenceHighlighter
    = std::function<std::vector<CodeLine>(const wxString& code, const wxString& lang)>;

/// Lay `doc` out into stacked, wrapped lines fitting `width` pixels. Code
/// blocks are highlighted through `highlightFence`; prose colours come from
/// `palette`; all measurement goes through `measurer`.
[[nodiscard]] auto layoutMarkdown(
    const MdDoc& doc,
    int width,
    const TextMeasurer& measurer,
    const ChatPalette& palette,
    const CodeFenceHighlighter& highlightFence
) -> LaidOutDoc;

} // namespace fbide
