//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Inline text styling, accumulated from nested markdown spans.
struct MdStyle {
    bool bold = false;          ///< `**strong**`
    bool italic = false;        ///< `*emphasis*`
    bool code = false;          ///< inline `` `code` `` span
    bool strikethrough = false; ///< `~~deleted~~`

    auto operator==(const MdStyle&) const -> bool = default;
};

/// Kind of an inline fragment within a block.
enum class MdInlineKind : std::uint8_t {
    Text,      ///< Styled run of text.
    Link,      ///< Styled run that is also a hyperlink (`url` set).
    SoftBreak, ///< Source newline inside a paragraph — wraps as a space.
    HardBreak, ///< Explicit `<br>` — forces a new line.
};

/// One inline fragment of a block: styled text, a link, or a line break.
struct MdInline {
    MdInlineKind kind = MdInlineKind::Text;
    wxString text; ///< Text / link label. Empty for breaks.
    wxString url;  ///< Link target. Set only when `kind == Link`.
    MdStyle style; ///< Styling for Text / Link fragments.
};

/// Block kind in the flattened document model.
enum class MdBlockKind : std::uint8_t {
    Paragraph, ///< Run of prose.
    Heading,   ///< `#`..`######` — see `headingLevel`.
    CodeFence, ///< Fenced or indented code — see `codeLang` / `codeText`.
    ListItem,  ///< One item line of a list — see `list*` fields.
    Rule,      ///< Horizontal rule (`---`). Carries no content.
    Table,     ///< GFM pipe table — see `rows` / `columnAlignment`.
    Patch,     ///< SEARCH/REPLACE proposal — see `patch*` fields. Only
               ///< produced for a fully-closed block; partial blocks
               ///< mid-stream are silently consumed.
};

/// Per-column text alignment, derived from the GFM separator row's `:`
/// markers. `Default` means the layout picks (left for the time being).
enum class MdTableAlignment : std::uint8_t {
    Default,
    Left,
    Center,
    Right,
};

/// One cell of a table row. Inline content is parsed with the same rules
/// as a paragraph, so `code`, **bold**, *italic*, [links] in cells all
/// reuse the existing inline pipeline.
struct MdTableCell {
    std::vector<MdInline> inlines;
};

/// One row of a table. The first `MdBlock::headerRowCount` rows are the
/// header (visually tinted by the painter); the rest are body rows.
struct MdTableRow {
    std::vector<MdTableCell> cells;
};

/// One block of the document. The markdown tree is flattened: list nesting
/// and block-quoting are expressed as integer depths, not nested children,
/// so the document is a simple linear sequence the layout engine can walk.
struct MdBlock {
    MdBlockKind kind = MdBlockKind::Paragraph;
    std::vector<MdInline> inlines;                 ///< Paragraph / Heading / ListItem content.
    unsigned headingLevel = 0;                     ///< Heading: 1-6.
    wxString codeLang;                             ///< CodeFence: fence info string, lowercased.
    wxString codeText;                             ///< CodeFence: verbatim code, '\n'-separated.
    int quoteDepth = 0;                            ///< Block-quote nesting (0 = not quoted).
    int listDepth = 0;                             ///< List nesting (0 = not in a list).
    bool listOrdered = false;                      ///< ListItem: ordered vs bulleted.
    int listOrdinal = 0;                           ///< ListItem: number for ordered lists.
    bool listMarker = false;                       ///< ListItem: draw the bullet/number
                                                   ///< (false for an item's continuation lines).
    std::vector<MdTableAlignment> columnAlignment; ///< Table: one entry per column.
    std::vector<MdTableRow> rows;                  ///< Table: header rows first, then body.
    std::size_t headerRowCount = 0;                ///< Table: number of leading rows in `rows`
                                                   ///< that are header (usually 1).
    wxString patchTarget;                          ///< Patch: optional target path from
                                                   ///< the `<<<<<<< SEARCH` header.
    wxString patchSearch;                          ///< Patch: verbatim SEARCH text.
    wxString patchReplace;                         ///< Patch: verbatim REPLACE text.
};

/// Parsed markdown document — a flat sequence of blocks.
struct MdDoc {
    std::vector<MdBlock> blocks;
};

/// Parse `text` (markdown) into the document model. Tolerates partial input:
/// an unterminated code fence mid-stream still yields a CodeFence block with
/// whatever arrived, so it is safe to call on a streaming reply.
[[nodiscard]] auto parseMarkdown(const wxString& text) -> MdDoc;

} // namespace fbide
