//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "LineHistory.hpp"
using namespace fbide;

void LineHistory::snapshot(std::vector<wxString> savedLines) {
    m_savedLines = std::move(savedLines);
    // Identity mapping — every current line came from its own position.
    m_originIndex.resize(m_savedLines.size());
    for (std::size_t index = 0; index < m_originIndex.size(); index++) {
        m_originIndex[index] = static_cast<int>(index);
    }
}

void LineHistory::applyInsert(int startLine, int linesAdded) {
    if (linesAdded <= 0) {
        return;
    }
    const auto size = static_cast<int>(m_originIndex.size());
    // Clamp `startLine` past the end → append. Negative indices land at 0
    // so we never index before the buffer; both are defensive bounds and
    // shouldn't fire from Editor's own modify path.
    const int insertAt = std::clamp(startLine, 0, size);
    m_originIndex.insert(
        m_originIndex.begin() + insertAt,
        static_cast<std::size_t>(linesAdded),
        -1 // Added-since-snapshot — no origin in m_savedLines.
    );
}

void LineHistory::applyDelete(int startLine, int linesRemoved) {
    if (linesRemoved <= 0) {
        return;
    }
    const auto size = static_cast<int>(m_originIndex.size());
    if (size == 0) {
        return;
    }
    // Clamp both endpoints so a wider-than-available delete just drops
    // everything from `startLine` onward.
    const int from = std::clamp(startLine, 0, size);
    const int to = std::min(from + linesRemoved, size);
    m_originIndex.erase(
        m_originIndex.begin() + from,
        m_originIndex.begin() + to
    );
}

auto LineHistory::stateOf(int currentLine, const wxString& currentLineText) const -> State {
    if (currentLine < 0 || currentLine >= static_cast<int>(m_originIndex.size())) {
        return State::Unchanged;
    }
    const int origin = m_originIndex[static_cast<std::size_t>(currentLine)];
    if (origin < 0) {
        return State::Added;
    }
    // A valid origin index always lies within `m_savedLines` — the
    // insert / delete plumbing keeps the two in sync by construction.
    if (m_savedLines[static_cast<std::size_t>(origin)] == currentLineText) {
        return State::Unchanged;
    }
    return State::Modified;
}

auto LineHistory::lineCount() const -> int {
    return static_cast<int>(m_originIndex.size());
}
