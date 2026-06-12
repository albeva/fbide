//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide::markdown {

/// Inline text styling, accumulated from nested markdown spans.
struct MdStyle {
    bool bold          : 1 = false; ///< `**strong**`
    bool italic        : 1 = false; ///< `*emphasis*`
    bool code          : 1 = false; ///< inline `` `code` `` span
    bool strikethrough : 1 = false; ///< `~~deleted~~`

    auto operator==(const MdStyle&) const -> bool = default;
};

/// Kind of an inline fragment within a block.
enum class MdInlineKind : std::uint8_t {
    Text,      ///< Styled run of text.
    Link,      ///< Styled run that is also a hyperlink (`url` set).
    Image,     ///< Embedded image — `text` is the alt label (may be empty),
               ///< `url` is the image source. Nested alt markup is flattened.
    SoftBreak, ///< Source newline inside a paragraph — wraps as a space.
    HardBreak, ///< Explicit `<br>` — forces a new line.
};

/// One inline fragment of a block: styled text, a link, an image, or a break.
struct MdInline {
    MdInlineKind kind = MdInlineKind::Text;
    wxString text; ///< Text / link label / image alt. Empty for breaks.
    wxString url;  ///< Link target or image source. Set for Link / Image.
    MdStyle style; ///< Styling captured at the fragment site.
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

/// One row of a table. The first `MdTable::headerRowCount` rows are the
/// header (visually tinted by the painter); the rest are body rows.
struct MdTableRow {
    std::vector<MdTableCell> cells;
};

/// Base of the block hierarchy. The markdown tree is flattened: list nesting
/// and block-quoting are expressed as integer depths, not nested children, so
/// the document is a linear sequence of polymorphic blocks the layout engine
/// walks. Each concrete kind carries only its own fields, so a plain paragraph
/// no longer drags the table / code / patch payload around.
struct MdBlockBase {
    NO_COPY_AND_MOVE(MdBlockBase)

    explicit MdBlockBase(const MdBlockKind blockKind)
    : kind(blockKind) {}

    virtual ~MdBlockBase() = default;

    /// Checked downcast to a concrete block type. Debug-asserts the kind
    /// matches; callers dispatch on `kind` first, so the cast is always valid.
    template<class T>
    [[nodiscard]] auto as() -> T& {
        wxASSERT(kind == T::kKind);
        return static_cast<T&>(*this);
    }

    template<class T>
    [[nodiscard]] auto as() const -> const T& {
        wxASSERT(kind == T::kKind);
        return static_cast<const T&>(*this);
    }

    MdBlockKind kind;   ///< Discriminant — set by each concrete block's constructor.
    int quoteDepth = 0; ///< Block-quote nesting (0 = not quoted). Common to every kind.
};

/// CRTP helper binding a concrete block to its `MdBlockKind`: supplies the
/// `kKind` discriminant that `as<T>()` asserts on and the constructor that
/// stamps it into the base, so each leaf only declares its own fields.
template<MdBlockKind K>
struct MdBlockOf : MdBlockBase {
    static constexpr auto kKind = K;

    MdBlockOf()
    : MdBlockBase(K) {}
};

/// Run of prose.
struct MdParagraph final : MdBlockOf<MdBlockKind::Paragraph> {
    std::vector<MdInline> inlines; ///< Paragraph content.
};

/// `#`..`######` heading — see `headingLevel`.
struct MdHeading final : MdBlockOf<MdBlockKind::Heading> {
    std::vector<MdInline> inlines; ///< Heading content.
    std::uint8_t headingLevel = 0; ///< 1-6.
};

/// Fenced or indented code block.
struct MdCodeFence final : MdBlockOf<MdBlockKind::CodeFence> {
    wxString codeLang; ///< Fence info string, lowercased.
    wxString codeText; ///< Verbatim code, '\n'-separated.
};

/// One item line of a list — see `list*` fields.
struct MdListItem final : MdBlockOf<MdBlockKind::ListItem> {
    std::vector<MdInline> inlines; ///< Item content.
    int listDepth = 0;             ///< List nesting (1 = top-level list).
    int listOrdinal = 0;           ///< Number for ordered lists.
    bool listOrdered : 1 = false;  ///< Ordered vs bulleted.
    bool listMarker  : 1 = false;  ///< Draw the bullet/number (false for continuation lines).
    bool isTask      : 1 = false;  ///< GFM task list item.
    bool taskChecked : 1 = false;  ///< Task box ticked (`[x]` / `[X]`).
};

/// Horizontal rule (`---`). Carries no content beyond the common fields.
struct MdRule final : MdBlockOf<MdBlockKind::Rule> {};

/// GFM pipe table — see `rows` / `columnAlignment`.
struct MdTable final : MdBlockOf<MdBlockKind::Table> {
    std::vector<MdTableAlignment> columnAlignment; ///< One entry per column.
    std::vector<MdTableRow> rows;                  ///< Header rows first, then body.
    std::uint32_t headerRowCount = 0;              ///< Leading rows in `rows` that are header (usually 1).
};

/// SEARCH/REPLACE proposal — see `patch*` fields. Only produced for a
/// fully-closed block; partial blocks mid-stream are silently consumed.
struct MdPatch final : MdBlockOf<MdBlockKind::Patch> {
    wxString patchTarget;  ///< Optional target path from the `<<<<<<< SEARCH` header.
    wxString patchSearch;  ///< Verbatim SEARCH text.
    wxString patchReplace; ///< Verbatim REPLACE text.
};

/// Parsed markdown document — a flat sequence of polymorphic blocks.
struct MdDoc {
    std::vector<std::unique_ptr<MdBlockBase>> blocks;
};

/// Parse `text` (markdown) into the document model. Tolerates partial input:
/// an unterminated code fence mid-stream still yields a CodeFence block with
/// whatever arrived, so it is safe to call on a streaming reply.
[[nodiscard]] auto parseMarkdown(const wxString& text) -> MdDoc;

/// Resolve the body of the Nth fenced code block in `markdown` by re-parsing
/// the source. Returns an empty string when `index` is out of range. The
/// chat view uses this as a slow-path lookup so the laid-out document
/// doesn't have to keep a duplicate copy of every snippet's text in memory.
[[nodiscard]] auto resolveCodeBlockText(const wxString& markdown, std::size_t index) -> wxString;

/// The inline runs of a block that carries them — `Paragraph`, `Heading` or
/// `ListItem`. Returns an empty list for the other kinds (code / rule / table /
/// patch), so callers that don't know the concrete kind can read prose content
/// uniformly without a downcast.
[[nodiscard]] auto blockInlines(const MdBlockBase& block) -> const std::vector<MdInline>&;

} // namespace fbide::markdown
