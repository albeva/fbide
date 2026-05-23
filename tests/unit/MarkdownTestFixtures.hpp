//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// Shared, deterministic test fixtures for the markdown layout / document
// unit tests. Pulled out so both `MarkdownLayoutTests` and
// `MarkdownDocumentTests` share one source of truth for their fake
// measurer, fake highlighter and a stable palette.
//
#pragma once
#include "markdown/MarkdownLayout.hpp"

namespace fbide::tests {

/// Fixed glyph metrics used by the fake measurer below. Named so the
/// tidy magic-number check doesn't trip on every literal.
constexpr int kFakeProseCharWidth = 10;
constexpr int kFakeMonoCharWidth = 8;
constexpr int kFakeBaseLineHeight = 20;

/// RGB channels for the fake palette. Picked so each colour is visibly
/// distinct from the next under a debugger but otherwise arbitrary.
constexpr unsigned char kFakeLinkBlue = 200;
constexpr unsigned char kFakeCodeBgChannel = 240;
constexpr unsigned char kFakeInlineCodeBgChannel = 230;
constexpr unsigned char kFakeRuleChannel = 200;

/// Deterministic measurer: every glyph is a fixed width, line height
/// tracks the size delta. Proportional text is 10 px / char, monospace
/// 8 px / char. Stable across runs so wrap-point assertions don't
/// depend on the host's installed fonts.
class FakeMeasurer final : public TextMeasurer {
public:
    [[nodiscard]] auto width(const wxString& text, const TextStyle& style) const -> int override {
        return static_cast<int>(text.length())
             * (style.monospace ? kFakeMonoCharWidth : kFakeProseCharWidth);
    }
    [[nodiscard]] auto lineHeight(const TextStyle& style) const -> int override {
        return kFakeBaseLineHeight + style.sizeDelta;
    }
};

/// Trivial fence highlighter — one black run per code line, '\n'-split,
/// with the trailing blank line dropped (as the real highlighter does).
inline auto splitHighlight(const wxString& code, const wxString& /*lang*/) -> std::vector<CodeLine> {
    std::vector<CodeLine> lines;
    CodeLine current;
    wxString segment;
    for (const wxUniChar ch : code) {
        if (ch == '\n') {
            if (!segment.empty()) {
                current.push_back({ .text = segment, .colour = wxColour(0, 0, 0) });
            }
            lines.push_back(current);
            current.clear();
            segment.clear();
        } else {
            segment += ch;
        }
    }
    if (!segment.empty()) {
        current.push_back({ .text = segment, .colour = wxColour(0, 0, 0) });
    }
    lines.push_back(current);
    if (lines.size() > 1 && lines.back().empty()) {
        lines.pop_back();
    }
    return lines;
}

/// Stable palette for layout tests. Colours are arbitrary but distinct
/// so painter-side debugging is easier; layout assertions don't depend
/// on the values.
inline auto fakePalette() -> MarkdownPalette {
    return { .text = wxColour(0, 0, 0),
        .link = wxColour(0, 0, kFakeLinkBlue),
        .codeBg = wxColour(kFakeCodeBgChannel, kFakeCodeBgChannel, kFakeCodeBgChannel),
        .inlineCodeBg = wxColour(kFakeInlineCodeBgChannel, kFakeInlineCodeBgChannel, kFakeInlineCodeBgChannel),
        .rule = wxColour(kFakeRuleChannel, kFakeRuleChannel, kFakeRuleChannel) };
}

} // namespace fbide::tests
