//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/**
 * Tracks each editor line's relationship to the last saved baseline.
 *
 * `LineHistory` is the data model behind the editor's "changes" margin.
 * On save (or file load) the caller hands over a snapshot of the current
 * lines; from then on, every text modification is reported in via
 * `applyInsert` / `applyDelete`. The helper maintains a mapping from
 * current line index to its origin in the saved snapshot — `-1` for any
 * line that didn't exist at save time. `stateOf` then derives one of
 * three per-line states (Unchanged / Added / Modified) given the line's
 * current text.
 *
 * Deliberately Scintilla-free so the logic is testable without a GUI.
 * The `Editor` adapts this to / from `EVT_STC_MODIFIED` events.
 *
 * **Threading:** UI thread only. No internal synchronisation.
 */
class LineHistory final {
public:
    NO_COPY_AND_MOVE(LineHistory)

    LineHistory() = default;
    ~LineHistory() = default;

    /// Per-line classification against the saved snapshot.
    enum class State : std::uint8_t {
        Unchanged, ///< Line exists at the same origin and its text matches.
        Added,     ///< Line was inserted since the last snapshot.
        Modified,  ///< Line existed at snapshot but its text has changed.
    };

    /// Replace the baseline. `savedLines` is the current document split
    /// into lines (newlines stripped). `m_originIndex` resets to the
    /// identity mapping so every line is Unchanged immediately after.
    void snapshot(std::vector<wxString> savedLines);

    /// Splice `linesAdded` Added-origin entries into `m_originIndex`
    /// starting at `startLine`. The line previously at `startLine`
    /// shifts to `startLine + linesAdded`. Out-of-range insertions are
    /// clamped to the end.
    void applyInsert(int startLine, int linesAdded);

    /// Erase `linesRemoved` entries from `m_originIndex` starting at
    /// `startLine`. Lines after the deleted range shift up by
    /// `linesRemoved`. Out-of-range deletes are clamped to the end.
    void applyDelete(int startLine, int linesRemoved);

    /// Derive the state of `currentLine`, given its present text.
    /// Returns `Unchanged` for out-of-range lines so callers don't need
    /// to bounds-check before drawing.
    [[nodiscard]] auto stateOf(int currentLine, const wxString& currentLineText) const -> State;

    /// Number of lines currently tracked — always matches the editor's
    /// line count after the corresponding `applyInsert` / `applyDelete`.
    [[nodiscard]] auto lineCount() const -> int;

private:
    std::vector<wxString> m_savedLines; ///< Per-line text at last snapshot.
    std::vector<int> m_originIndex;     ///< current-line → saved index, or -1 for Added.
};

} // namespace fbide
