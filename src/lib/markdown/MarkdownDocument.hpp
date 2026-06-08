//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "markdown/MarkdownLayout.hpp"

namespace fbide::markdown {

/**
 * Headless container for a single rendered markdown document.
 *
 * Bundles the source text, the parsed `MdDoc` and the laid-out
 * `LaidOutDoc` together with the width they were laid out for, and
 * caches the layout — `setMarkdown(text, width, …)` is a no-op when
 * the text and width are unchanged from the previous call. Hosts
 * (the chat view, the future `MarkdownView`) keep one of these per
 * document they want to display and call `setMarkdown` whenever the
 * source or available width changes.
 *
 * Owns nothing GUI. Pure state + invalidation logic. Movable, not
 * copyable — moving is cheap (vector swaps) and copying a laid-out
 * doc rarely makes sense.
 */
class MarkdownDocument {
public:
    MarkdownDocument() = default;
    MarkdownDocument(MarkdownDocument&&) noexcept = default;
    auto operator=(MarkdownDocument&&) noexcept -> MarkdownDocument& = default;
    MarkdownDocument(const MarkdownDocument&) = delete;
    auto operator=(const MarkdownDocument&) -> MarkdownDocument& = delete;
    ~MarkdownDocument() = default;

    /// Replace content. Returns true if `markdown` or `contentWidth`
    /// differ from the cached state and a re-parse + re-layout was
    /// performed; false when the cached layout was reusable. Callers
    /// use the return value to decide whether to invalidate paint.
    ///
    /// `isCollapsed`, when set, is consulted per fenced code / patch
    /// block to decide whether to lay it out collapsed (a single
    /// summary strip). The cache key includes only the markdown text
    /// and width — callers that change collapse state independently of
    /// the source text must call `invalidate()` to force a re-layout.
    auto setMarkdown(
        const wxString& markdown,
        int contentWidth,
        const TextMeasurer& measurer,
        const MarkdownPalette& palette,
        const CodeFenceHighlighter& highlightFence,
        const ImageResolver& resolveImage = {},
        bool wrapCodeBlocks = true,
        const BlockCollapsedQuery& isCollapsed = {},
        const LanguageDisplayResolver& resolveLanguageDisplay = {}
    ) -> bool;

    /// Drop the cached state — `setMarkdown` after `clear` always
    /// rebuilds even if the text is the same.
    void clear();

    /// Force the next `setMarkdown` to re-parse + re-layout even if its
    /// inputs match the cached state. Use after palette / highlighter /
    /// wrap-mode changes invalidate the cached output without changing
    /// the source text. Cheaper than `clear()` — keeps the stored
    /// markdown so the next `setMarkdown` can pass it straight through.
    void invalidate() { m_width = -1; }

    [[nodiscard]] auto markdown() const -> const wxString& { return m_markdown; }
    [[nodiscard]] auto laid() const -> const LaidOutDoc& { return m_laid; }
    [[nodiscard]] auto width() const -> int { return m_width; }
    [[nodiscard]] auto height() const -> int { return m_laid.height; }

private:
    wxString m_markdown;
    LaidOutDoc m_laid;
    int m_width = -1;
    bool m_wrapCodeBlocks = true;
};

} // namespace fbide::markdown
