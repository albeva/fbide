//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ChatSelection.hpp"
#include "markdown/MarkdownLayout.hpp"
using namespace fbide;
using namespace fbide::ai;
using namespace fbide::markdown;

void ChatSelection::begin(const int messageIndex, const SelectionPosition position) {
    m_messageIndex = messageIndex;
    m_selection.anchor = position;
    m_selection.caret = position;
    m_dragging = true;
}

void ChatSelection::extendCaret(const SelectionPosition position) {
    if (m_messageIndex < 0) {
        return;
    }
    m_selection.caret = position;
}

void ChatSelection::dragCaret(const SelectionPosition position) {
    if (!m_dragging || m_messageIndex < 0) {
        return;
    }
    m_selection.caret = position;
}

void ChatSelection::clear() {
    m_messageIndex = -1;
    m_selection.clear();
    m_dragging = false;
}

void ChatSelection::selectWord(
    const int messageIndex,
    const SelectionPosition position,
    const LaidOutDoc& laid
) {
    if (position.lineIndex >= laid.lines.size()) {
        return;
    }
    const auto& line = laid.lines.at(position.lineIndex);
    if (position.runIndex >= line.runs.size()) {
        return;
    }
    const auto& run = line.runs.at(position.runIndex);
    const auto isWord = [](const wxUniChar character) {
        return wxIsalnum(character) || character == '_';
    };
    std::size_t start = position.charInRun;
    while (start > 0 && isWord(run.text.GetChar(start - 1))) {
        start--;
    }
    std::size_t end = position.charInRun;
    while (end < run.text.length() && isWord(run.text.GetChar(end))) {
        end++;
    }
    m_messageIndex = messageIndex;
    m_selection.anchor = { .lineIndex = position.lineIndex, .runIndex = position.runIndex, .charInRun = start };
    m_selection.caret = { .lineIndex = position.lineIndex, .runIndex = position.runIndex, .charInRun = end };
    m_dragging = false;
}

void ChatSelection::selectAll(const int messageIndex, const LaidOutDoc& laid) {
    if (laid.lines.empty()) {
        return;
    }
    const std::size_t lastLine = laid.lines.size() - 1;
    const auto& last = laid.lines.at(lastLine);
    const std::size_t lastRun = last.runs.empty() ? 0 : last.runs.size() - 1;
    const std::size_t lastChar = last.runs.empty() ? 0 : last.runs.at(lastRun).text.length();
    m_messageIndex = messageIndex;
    m_selection.anchor = { .lineIndex = 0, .runIndex = 0, .charInRun = 0 };
    m_selection.caret = { .lineIndex = lastLine, .runIndex = lastRun, .charInRun = lastChar };
    m_dragging = false;
}

auto ChatSelection::captureOffsets(const LaidOutDoc& laid) const -> StableOffsets {
    return {
        .anchor = selectionToOffset(laid, m_selection.anchor),
        .caret = selectionToOffset(laid, m_selection.caret),
    };
}

void ChatSelection::restoreFromOffsets(const LaidOutDoc& laid, const StableOffsets offsets) {
    // Bias the lower offset toward the start of its line and the higher
    // toward the end so the selection's outer edges stick where the
    // user originally clicked — matches the pre-extraction behaviour.
    const bool anchorIsLow = offsets.anchor <= offsets.caret;
    m_selection.anchor = selectionFromOffset(
        laid, offsets.anchor,
        anchorIsLow ? OffsetBias::PreferLineStart : OffsetBias::PreferLineEnd
    );
    m_selection.caret = selectionFromOffset(
        laid, offsets.caret,
        anchorIsLow ? OffsetBias::PreferLineEnd : OffsetBias::PreferLineStart
    );
}
