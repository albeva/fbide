//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Markdown.hpp"
#include <md4c.h>
using namespace fbide;

namespace {

/// Mutable state threaded through the md4c callbacks as `userdata`. md4c is a
/// SAX-style parser — it streams enter/leave/text events; this builder turns
/// that event stream into the flat `MdDoc`.
struct Builder {
    MdDoc doc;

    // Inline styling. Spans nest, so each style is a depth counter rather
    // than a flag — `**a *b* a**` leaves bold active across the inner span.
    int em = 0;       ///< `*emphasis*` depth.
    int strong = 0;   ///< `**strong**` depth.
    int codeSpan = 0; ///< inline `` `code` `` depth.
    int del = 0;      ///< `~~strikethrough~~` depth.

    // Current link span (md4c spans never nest links).
    bool inLink = false;
    wxString linkUrl;

    // Current image span. md4c emits the alt text as inline events between
    // the IMG enter/leave; the builder diverts those into `imageAlt` and
    // emits a single Image inline at leave time. Style is captured at the
    // enter so the surrounding emphasis applies to the resulting fragment.
    bool inImage = false;
    wxString imageAlt;
    wxString imageSrc;
    MdStyle imageStyle;

    // Open lists, outermost first. Each tracks the next ordinal to hand out.
    struct ListCtx {
        bool ordered;
        int next;
    };
    std::vector<ListCtx> lists;

    int quoteDepth = 0; ///< Current block-quote nesting.

    // The block currently being assembled.
    bool open = false;
    MdBlock cur;
    bool inCodeBlock = false; ///< Inside MD_BLOCK_CODE — text is verbatim code.

    // Table state — non-null only while a table is being assembled. md4c
    // emits cells via TH/TD blocks, and inline events inside a cell route
    // to `cellInlines` instead of `cur.inlines`. `inHeader` is true between
    // MD_BLOCK_THEAD enter/leave so we know which rows to mark as header.
    bool inHeader = false;
    std::vector<MdInline>* cellInlines = nullptr;

    /// Current accumulated inline style.
    [[nodiscard]] auto style() const -> MdStyle {
        return { .bold = strong > 0,
            .italic = em > 0,
            .code = codeSpan > 0,
            .strikethrough = del > 0 };
    }

    /// Open a fresh block of `kind`, carrying the current quote depth. Any
    /// block still open is flushed first — markdown blocks never overlap.
    void begin(const MdBlockKind kind) {
        end();
        cur = MdBlock {};
        cur.kind = kind;
        cur.quoteDepth = quoteDepth;
        open = true;
    }

    /// Push the open block (if any) into the document.
    void end() {
        if (open) {
            doc.blocks.push_back(std::move(cur));
            open = false;
        }
    }

    /// Where the next inline fragment should land — a table cell when
    /// inside one, otherwise the open block's own inline list.
    [[nodiscard]] auto inlineSink() -> std::vector<MdInline>* {
        if (cellInlines != nullptr) {
            return cellInlines;
        }
        if (!open) {
            return nullptr;
        }
        return &cur.inlines;
    }

    /// Append a text run — to the code body inside a fence, to the image
    /// alt buffer inside an image span, otherwise as a styled (and possibly
    /// link-tagged) inline fragment in the current sink (table cell or
    /// block paragraph).
    void addText(const wxString& text) {
        if (inCodeBlock) {
            if (open) {
                cur.codeText += text;
            }
            return;
        }
        if (inImage) {
            imageAlt += text;
            return;
        }
        auto* sink = inlineSink();
        if (sink == nullptr) {
            return;
        }
        sink->push_back({
            .kind = inLink ? MdInlineKind::Link : MdInlineKind::Text,
            .text = text,
            .url = inLink ? linkUrl : wxString {},
            .style = style(),
        });
    }

    /// Append a soft or hard line break (ignored inside code and image alt).
    void addBreak(const MdInlineKind kind) {
        if (inCodeBlock || inImage) {
            return;
        }
        auto* sink = inlineSink();
        if (sink == nullptr) {
            return;
        }
        sink->push_back({ .kind = kind, .text = {}, .url = {}, .style = {} });
    }

    /// Emit a single Image inline into the current sink with the alt text
    /// accumulated between MD_SPAN_IMG enter/leave.
    void emitImage() {
        auto* sink = inlineSink();
        if (sink == nullptr) {
            return;
        }
        sink->push_back({
            .kind = MdInlineKind::Image,
            .text = imageAlt,
            .url = imageSrc,
            .style = imageStyle,
        });
    }
};

/// Translate md4c's `MD_ALIGN` to our local enum so the public model
/// doesn't leak md4c types.
[[nodiscard]] auto translateAlign(const MD_ALIGN align) -> MdTableAlignment {
    switch (align) {
    case MD_ALIGN_LEFT:
        return MdTableAlignment::Left;
    case MD_ALIGN_CENTER:
        return MdTableAlignment::Center;
    case MD_ALIGN_RIGHT:
        return MdTableAlignment::Right;
    case MD_ALIGN_DEFAULT:
    default:
        return MdTableAlignment::Default;
    }
}

/// Wrap an md4c (pointer, length) string — never NUL-terminated — as wxString.
auto mdStr(const MD_CHAR* text, const MD_SIZE size) -> wxString {
    return wxString::FromUTF8(text, size);
}

/// Wrap an md4c attribute's verbatim text as wxString. Attribute substrings
/// split out entities; for the fields used here (code-fence info string) the
/// verbatim form is what is wanted.
auto attrStr(const MD_ATTRIBUTE& attr) -> wxString {
    return wxString::FromUTF8(attr.text, attr.size);
}

/// Decode the handful of HTML entities that turn up in chat prose. md4c keeps
/// no entity table, so anything else is passed through verbatim.
auto decodeEntity(const wxString& entity) -> wxString {
    if (entity == "&amp;") {
        return "&";
    }
    if (entity == "&lt;") {
        return "<";
    }
    if (entity == "&gt;") {
        return ">";
    }
    if (entity == "&quot;") {
        return "\"";
    }
    if (entity == "&apos;") {
        return "'";
    }
    if (entity == "&nbsp;") {
        return { wxUniChar(0x00A0) };
    }
    // Numeric: &#1234; or &#x12AB;
    if (entity.StartsWith("&#") && entity.EndsWith(";")) {
        const wxString digits = entity.Mid(2, entity.size() - 3);
        long value = 0;
        const bool hex = digits.StartsWith("x") || digits.StartsWith("X");
        if ((hex ? digits.Mid(1).ToLong(&value, 16) : digits.ToLong(&value)) && value > 0) {
            return { wxUniChar(static_cast<int>(value)) };
        }
    }
    return entity;
}

} // namespace

// md4c stores plain C function pointers; give the callbacks C language
// linkage so the assignment is well-formed. `static` keeps them file-local.
extern "C" {

static int mdEnterBlock(const MD_BLOCKTYPE type, void* detail, void* userData) {
    auto& builder = *static_cast<Builder*>(userData);
    switch (type) {
    case MD_BLOCK_QUOTE:
        builder.quoteDepth++;
        break;
    case MD_BLOCK_UL:
        builder.lists.push_back({ .ordered = false, .next = 1 });
        break;
    case MD_BLOCK_OL: {
        const auto* d = static_cast<MD_BLOCK_OL_DETAIL*>(detail);
        builder.lists.push_back({ .ordered = true, .next = static_cast<int>(d->start) });
        break;
    }
    case MD_BLOCK_LI: {
        // Open the item block now: tight lists put the text straight in the
        // LI (no inner MD_BLOCK_P), so there must be a block to receive it.
        builder.begin(MdBlockKind::ListItem);
        if (!builder.lists.empty()) {
            auto& list = builder.lists.back();
            builder.cur.listDepth = static_cast<int>(builder.lists.size());
            builder.cur.listOrdered = list.ordered;
            builder.cur.listOrdinal = list.next++;
            builder.cur.listMarker = true;
        }
        const auto* liDetail = static_cast<MD_BLOCK_LI_DETAIL*>(detail);
        if (liDetail != nullptr && liDetail->is_task != 0) {
            builder.cur.isTask = true;
            builder.cur.taskChecked = liDetail->task_mark == 'x' || liDetail->task_mark == 'X';
        }
        break;
    }
    case MD_BLOCK_HR:
        builder.begin(MdBlockKind::Rule);
        builder.end();
        break;
    case MD_BLOCK_H: {
        const auto* hDetail = static_cast<MD_BLOCK_H_DETAIL*>(detail);
        builder.begin(MdBlockKind::Heading);
        builder.cur.headingLevel = hDetail->level;
        break;
    }
    case MD_BLOCK_CODE: {
        const auto* codeDetail = static_cast<MD_BLOCK_CODE_DETAIL*>(detail);
        builder.begin(MdBlockKind::CodeFence);
        builder.cur.codeLang = attrStr(codeDetail->lang).Lower();
        builder.inCodeBlock = true;
        break;
    }
    case MD_BLOCK_P:
        if (builder.open && builder.cur.kind == MdBlockKind::ListItem) {
            // Loose-list item: the paragraph fills the already-open item
            // block. A second paragraph in the same item gets a line break.
            if (!builder.cur.inlines.empty()) {
                builder.addBreak(MdInlineKind::HardBreak);
            }
        } else {
            builder.begin(MdBlockKind::Paragraph);
        }
        break;
    case MD_BLOCK_TABLE:
        builder.begin(MdBlockKind::Table);
        break;
    case MD_BLOCK_THEAD:
        builder.inHeader = true;
        break;
    case MD_BLOCK_TBODY:
        builder.inHeader = false;
        break;
    case MD_BLOCK_TR:
        if (builder.open && builder.cur.kind == MdBlockKind::Table) {
            builder.cur.rows.push_back({});
            if (builder.inHeader) {
                builder.cur.headerRowCount++;
            }
        }
        break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        if (builder.open && builder.cur.kind == MdBlockKind::Table
            && !builder.cur.rows.empty()) {
            auto& row = builder.cur.rows.back();
            row.cells.push_back({});
            builder.cellInlines = &row.cells.back().inlines;
            // Column alignment is captured from header-row cells. md4c
            // also reports it on body cells, but the header is the
            // canonical source per GFM.
            if (builder.inHeader && detail != nullptr) {
                const auto* td = static_cast<MD_BLOCK_TD_DETAIL*>(detail);
                builder.cur.columnAlignment.push_back(translateAlign(td->align));
            }
        }
        break;
    default:
        // MD_BLOCK_DOC needs no action.
        break;
    }
    return 0;
}

static int mdLeaveBlock(const MD_BLOCKTYPE type, void* /*detail*/, void* userData) {
    auto& builder = *static_cast<Builder*>(userData);
    switch (type) {
    case MD_BLOCK_QUOTE:
        if (builder.quoteDepth > 0) {
            builder.quoteDepth--;
        }
        break;
    case MD_BLOCK_UL:
    case MD_BLOCK_OL:
        if (!builder.lists.empty()) {
            builder.lists.pop_back();
        }
        break;
    case MD_BLOCK_CODE:
        builder.inCodeBlock = false;
        builder.end();
        break;
    case MD_BLOCK_LI:
    case MD_BLOCK_H:
        builder.end();
        break;
    case MD_BLOCK_P:
        // A list item's paragraph stays open — the item is closed by LI.
        if (builder.open && builder.cur.kind == MdBlockKind::Paragraph) {
            builder.end();
        }
        break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        builder.cellInlines = nullptr;
        break;
    case MD_BLOCK_THEAD:
    case MD_BLOCK_TBODY:
        builder.inHeader = false;
        break;
    case MD_BLOCK_TABLE:
        builder.end();
        break;
    default:
        break;
    }
    return 0;
}

static int mdEnterSpan(const MD_SPANTYPE type, void* detail, void* userData) {
    auto& builder = *static_cast<Builder*>(userData);
    switch (type) {
    case MD_SPAN_EM:
        builder.em++;
        break;
    case MD_SPAN_STRONG:
        builder.strong++;
        break;
    case MD_SPAN_CODE:
        builder.codeSpan++;
        break;
    case MD_SPAN_DEL:
        builder.del++;
        break;
    case MD_SPAN_A: {
        const auto* d = static_cast<MD_SPAN_A_DETAIL*>(detail);
        builder.inLink = true;
        builder.linkUrl = attrStr(d->href);
        break;
    }
    case MD_SPAN_IMG: {
        const auto* d = static_cast<MD_SPAN_IMG_DETAIL*>(detail);
        builder.inImage = true;
        builder.imageAlt.clear();
        builder.imageSrc = attrStr(d->src);
        builder.imageStyle = builder.style();
        break;
    }
    default:
        // LaTeX and wiki links are not rendered specially.
        break;
    }
    return 0;
}

static int mdLeaveSpan(const MD_SPANTYPE type, void* /*detail*/, void* userData) {
    auto& builder = *static_cast<Builder*>(userData);
    switch (type) {
    case MD_SPAN_EM:
        builder.em = std::max(0, builder.em - 1);
        break;
    case MD_SPAN_STRONG:
        builder.strong = std::max(0, builder.strong - 1);
        break;
    case MD_SPAN_CODE:
        builder.codeSpan = std::max(0, builder.codeSpan - 1);
        break;
    case MD_SPAN_DEL:
        builder.del = std::max(0, builder.del - 1);
        break;
    case MD_SPAN_A:
        builder.inLink = false;
        builder.linkUrl.clear();
        break;
    case MD_SPAN_IMG:
        builder.emitImage();
        builder.inImage = false;
        builder.imageAlt.clear();
        builder.imageSrc.clear();
        builder.imageStyle = {};
        break;
    default:
        break;
    }
    return 0;
}

static int mdText(const MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userData) {
    auto& builder = *static_cast<Builder*>(userData);
    switch (type) {
    case MD_TEXT_NORMAL:
    case MD_TEXT_CODE:
    case MD_TEXT_HTML: // MD_FLAG_NOHTML routes raw HTML here as plain text.
    case MD_TEXT_LATEXMATH:
        builder.addText(mdStr(text, size));
        break;
    case MD_TEXT_ENTITY:
        builder.addText(decodeEntity(mdStr(text, size)));
        break;
    case MD_TEXT_NULLCHAR:
        builder.addText(wxString(wxUniChar(0xFFFD)));
        break;
    case MD_TEXT_BR:
        builder.addBreak(MdInlineKind::HardBreak);
        break;
    case MD_TEXT_SOFTBR:
        builder.addBreak(MdInlineKind::SoftBreak);
        break;
    default:
        break;
    }
    return 0;
}

} // extern "C"

namespace {

// Marker lines for the SEARCH/REPLACE proposal block. Recognised in a
// pre-pass before md4c sees the input — md4c would otherwise treat the
// `=======` separator as an H2 setext underline. Convention is the
// Aider / Cursor style: exact characters, each on its own line. The
// header may carry an optional target path after a single space.
const wxString kSearchPrefix = "<<<<<<< SEARCH";
const wxString kSeparatorMarker = "=======";
const wxString kReplacePrefix = ">>>>>>> REPLACE";

/// True when `line` (with surrounding whitespace already trimmed) opens
/// a SEARCH block. `target` is filled with any text after the prefix.
auto matchSearchMarker(const wxString& line, wxString& target) -> bool {
    if (!line.StartsWith(kSearchPrefix)) {
        return false;
    }
    const std::size_t prefixLen = kSearchPrefix.size();
    if (line.size() == prefixLen) {
        target.clear();
        return true;
    }
    if (line[prefixLen] != ' ') {
        return false;
    }
    target = line.Mid(prefixLen + 1);
    target.Trim();
    return true;
}

auto matchSeparator(const wxString& line) -> bool {
    return line == kSeparatorMarker;
}

auto matchReplaceMarker(const wxString& line) -> bool {
    if (!line.StartsWith(kReplacePrefix)) {
        return false;
    }
    return line.size() == kReplacePrefix.size() || line[kReplacePrefix.size()] == ' ';
}

/// Strip a single trailing `\r` from `line` so CRLF input matches markers.
void stripCr(wxString& line) {
    if (!line.empty() && line[line.length() - 1] == '\r') {
        line.RemoveLast();
    }
}

/// Run md4c over a plain markdown segment — the original body of
/// `parseMarkdown`. Used per segment between Patch blocks; called as a
/// helper rather than recursively from the top-level entry point.
auto parseSegment(const wxString& text) -> MdDoc {
    Builder builder;

    MD_PARSER parser {};
    parser.abi_version = 0;
    parser.flags = MD_FLAG_NOHTML | MD_FLAG_PERMISSIVEAUTOLINKS
                 | MD_FLAG_STRIKETHROUGH | MD_FLAG_COLLAPSEWHITESPACE
                 | MD_FLAG_TABLES | MD_FLAG_TASKLISTS;
    parser.enter_block = mdEnterBlock;
    parser.leave_block = mdLeaveBlock;
    parser.enter_span = mdEnterSpan;
    parser.leave_span = mdLeaveSpan;
    parser.text = mdText;

    const auto utf8 = text.utf8_string();
    md_parse(utf8.c_str(), static_cast<MD_SIZE>(utf8.size()), &parser, &builder);

    // md4c closes every open block at end of input, so an unterminated fence
    // is already flushed; this is a belt-and-braces guard.
    builder.end();
    return std::move(builder.doc);
}

} // namespace

auto fbide::parseMarkdown(const wxString& text) -> MdDoc {
    // Pre-scan for SEARCH/REPLACE proposal blocks line by line. Markdown
    // between proposals is parsed by md4c per segment; a fully-closed
    // proposal emits an MdBlockKind::Patch. A proposal that is still
    // open at end-of-input (mid-stream partial) is silently dropped —
    // the next chunk reparse will pick it up once the closing marker
    // arrives.
    MdDoc result;
    wxString mdAccum; // accumulated markdown between proposals

    const auto flushMarkdown = [&] {
        if (mdAccum.empty()) {
            return;
        }
        MdDoc segment = parseSegment(mdAccum);
        for (auto& block : segment.blocks) {
            result.blocks.push_back(std::move(block));
        }
        mdAccum.clear();
    };

    enum class State : std::uint8_t { Markdown,
        InSearch,
        InReplace };
    State state = State::Markdown;
    wxString patchTarget;
    wxString patchSearch;
    wxString patchReplace;

    std::size_t pos = 0;
    while (pos < text.length()) {
        // Slice one logical line from `pos`. `hasNewline` distinguishes
        // a complete line from a trailing partial one at end-of-input.
        const std::size_t newline = text.find('\n', pos);
        const bool hasNewline = newline != wxString::npos;
        const std::size_t lineEnd = hasNewline ? newline : text.length();
        wxString line = text.Mid(pos, lineEnd - pos);
        stripCr(line);

        const wxString suffix = hasNewline ? wxString("\n") : wxString();

        switch (state) {
        case State::Markdown: {
            wxString candidateTarget;
            if (matchSearchMarker(line, candidateTarget)) {
                flushMarkdown(); // emit pending prose before the proposal
                patchTarget = candidateTarget;
                patchSearch.clear();
                patchReplace.clear();
                state = State::InSearch;
            } else {
                mdAccum += line;
                mdAccum += suffix;
            }
            break;
        }
        case State::InSearch:
            if (matchSeparator(line)) {
                state = State::InReplace;
            } else {
                patchSearch += line;
                patchSearch += suffix;
            }
            break;
        case State::InReplace:
            if (matchReplaceMarker(line)) {
                MdBlock block;
                block.kind = MdBlockKind::Patch;
                block.patchTarget = patchTarget;
                block.patchSearch = patchSearch;
                block.patchReplace = patchReplace;
                result.blocks.push_back(std::move(block));
                patchTarget.clear();
                patchSearch.clear();
                patchReplace.clear();
                state = State::Markdown;
            } else {
                patchReplace += line;
                patchReplace += suffix;
            }
            break;
        }

        pos = hasNewline ? newline + 1 : text.length();
    }

    flushMarkdown();
    // A still-open proposal at EOF is dropped on purpose — partial mid-stream.
    return result;
}

auto fbide::resolveCodeBlockText(const wxString& markdown, const std::size_t index) -> wxString {
    const auto doc = parseMarkdown(markdown);
    std::size_t seen = 0;
    for (const auto& block : doc.blocks) {
        if (block.kind != MdBlockKind::CodeFence) {
            continue;
        }
        if (seen == index) {
            return block.codeText;
        }
        seen++;
    }
    return {};
}
