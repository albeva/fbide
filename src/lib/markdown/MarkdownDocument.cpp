//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "markdown/MarkdownDocument.hpp"
using namespace fbide::markdown;

auto MarkdownDocument::setMarkdown(
    const wxString& markdown,
    const int contentWidth,
    const TextMeasurer& measurer,
    const MarkdownPalette& palette,
    const CodeFenceHighlighter& highlightFence,
    const ImageResolver& resolveImage,
    const bool wrapCodeBlocks
) -> bool {
    // Cache check — same content + same width AND same wrap mode means the
    // previous layout is reusable. Saves both the markdown parse and the
    // layout pass on hot paths like streaming-tick relayouts.
    if (contentWidth == m_width && wrapCodeBlocks == m_wrapCodeBlocks && markdown == m_markdown) {
        return false;
    }
    m_markdown = markdown;
    m_width = contentWidth;
    m_wrapCodeBlocks = wrapCodeBlocks;
    m_laid = layoutMarkdown(parseMarkdown(markdown), contentWidth, measurer, palette, highlightFence, resolveImage, wrapCodeBlocks);
    return true;
}

void MarkdownDocument::clear() {
    m_markdown.clear();
    m_laid = LaidOutDoc {};
    m_width = -1;
}
