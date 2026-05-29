//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;
class Document;

/// Owns the main-frame status bar plus everything that writes to it:
/// field layout, cursor / type / config / EOL / encoding cells, click
/// routing to the popup menus.
///
/// Layout flips between **5 fields** (default) and **6 fields** when
/// `commands.configurationInStatusBar` is on. Centralising the layout
/// here keeps the field indices off the public API — `Editor`,
/// `CompilerManager`, and the click handler all go through typed
/// setters / accessors that know which field is which.
class StatusBarHandler final {
public:
    explicit StatusBarHandler(Context& ctx)
    : m_ctx(ctx) {}
    NO_COPY_AND_MOVE(StatusBarHandler)

    /// Create the status bar on `frame`. Binds `wxEVT_LEFT_DOWN` to
    /// the field-click router. Pulls the initial layout from
    /// `commands.configurationInStatusBar`.
    void create(wxFrame* frame);

    /// Re-read the configuration preference and re-flow the field
    /// count + widths. Called once at startup and again from
    /// `UIManager::updateSettings` so toggling the option in the
    /// Settings dialog takes effect without a restart.
    void applyPreference();

    /// True when the configuration field is part of the current layout.
    [[nodiscard]] auto hasConfigurationField() const -> bool { return m_hasConfigField; }

    /// Push the line / column indicator. Called by `Editor` on every
    /// cursor move.
    void setCursor(int line, int column) const;

    /// Push every per-document field (type, EOL, encoding, config).
    /// Called by `Editor::updateStatusBar` after a tab / position
    /// change.
    void setDocumentFields(const Document& doc) const;

    /// Wipe every per-document cell (cursor, type, EOL, encoding, and
    /// the optional configuration cell) — used when there is no active
    /// document.
    void clearDocumentFields() const;

    /// Refresh just the configuration field — called when the catalog
    /// changes (Add / Copy / Remove / Rename / Active toggle) or the
    /// active document's pinned slug changes.
    void refreshConfigurationField() const;

private:
    void onClick(wxMouseEvent& event);

    /// Open the appropriate popup for `doc` based on which field was
    /// clicked. Returns true when a popup was shown.
    auto handleClickAt(const wxPoint& position, Document& doc) -> bool;

    // Field-index helpers — `configurationField()` returns `-1` when
    // the 5-field layout is active so callers can short-circuit.
    //
    // Layout (left → right):
    //   5-field: welcome | cursor | type | eol | encoding
    //   6-field: welcome | cursor | type | config | eol | encoding
    static constexpr int kCursorField = 1;
    static constexpr int kTypeField = 2;
    static constexpr int kConfigFieldWhenPresent = 3;
    static constexpr int kEolFieldWithConfig = 4;
    static constexpr int kEolFieldNoConfig = 3;
    static constexpr int kEncodingFieldWithConfig = 5;
    static constexpr int kEncodingFieldNoConfig = 4;

    [[nodiscard]] static auto cursorField() -> int { return kCursorField; }
    [[nodiscard]] static auto typeField() -> int { return kTypeField; }
    [[nodiscard]] auto configurationField() const -> int { return m_hasConfigField ? kConfigFieldWhenPresent : -1; }
    [[nodiscard]] auto eolField() const -> int { return m_hasConfigField ? kEolFieldWithConfig : kEolFieldNoConfig; }
    [[nodiscard]] auto encodingField() const -> int { return m_hasConfigField ? kEncodingFieldWithConfig : kEncodingFieldNoConfig; }

    Context& m_ctx;
    wxFrame* m_frame = nullptr;
    wxStatusBar* m_bar = nullptr;
    bool m_hasConfigField = false;
};

} // namespace fbide
