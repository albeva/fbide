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
};

/// One block of the document. The markdown tree is flattened: list nesting
/// and block-quoting are expressed as integer depths, not nested children,
/// so the document is a simple linear sequence the layout engine can walk.
struct MdBlock {
    MdBlockKind kind = MdBlockKind::Paragraph;
    std::vector<MdInline> inlines; ///< Paragraph / Heading / ListItem content.
    unsigned headingLevel = 0;     ///< Heading: 1-6.
    wxString codeLang;             ///< CodeFence: fence info string, lowercased.
    wxString codeText;             ///< CodeFence: verbatim code, '\n'-separated.
    int quoteDepth = 0;            ///< Block-quote nesting (0 = not quoted).
    int listDepth = 0;             ///< List nesting (0 = not in a list).
    bool listOrdered = false;      ///< ListItem: ordered vs bulleted.
    int listOrdinal = 0;           ///< ListItem: number for ordered lists.
    bool listMarker = false;       ///< ListItem: draw the bullet/number
                                   ///< (false for an item's continuation lines).
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
