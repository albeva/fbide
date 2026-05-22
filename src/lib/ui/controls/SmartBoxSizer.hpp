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
 * A `wxBoxSizer` subclass that adjusts per-item borders and outer
 * padding automatically based on visibility.
 *
 * Layout model
 * ------------
 * - `margin` is the per-item border applied via `wxALL` on every
 *   visible item (wxWidgets' standard "border" concept).
 * - `gap` is the per-item border applied to inner items when the
 *   value differs from `margin` — in that case the sizer holds a
 *   single nested `wxBoxSizer`, with `margin` as the outer border
 *   and `gap` as the per-item border inside.
 * - `-1` resolves to `wxSizerFlags::GetDefaultBorder()`.
 *
 * Why the nested form: per-`wxSizerItem` border is a single integer
 * applied to whichever edge flags the item has. With `margin != gap`,
 * a single sizer can't express both "outer = margin" and "between =
 * 2*gap" simultaneously, so the inner sizer carries the gap and the
 * outer carries the margin.
 *
 * Visibility-driven behaviour
 * ---------------------------
 * - If every child is hidden (or none have been added), the sizer
 *   reports a zero minimum size — no margin is added.
 * - When at least one child is visible, the outer border kicks in
 *   and per-item borders are restored.
 * - The recompute runs from `CalcMin`, so any `Show`/`Hide`/`Add`/
 *   `Remove` picked up by a normal `Layout()` call reaches us
 *   without needing explicit notification.
 *
 * `Options::center` applies cross-axis centring to every item
 * (`wxALIGN_CENTRE_VERTICAL` for a horizontal sizer,
 * `wxALIGN_CENTRE_HORIZONTAL` for a vertical one). Items added with
 * `wxEXPAND` are left alone — expanding already saturates the cross
 * axis, so a centre flag would be a no-op or conflicting.
 *
 * `wxEXPAND` and `proportion` from the caller are preserved as-is.
 * Per-item borders supplied by the caller are stripped at insertion
 * time — the sizer owns that decision.
 */
class SmartBoxSizer final : public wxBoxSizer {
public:
    NO_COPY_AND_MOVE(SmartBoxSizer)

    /// Default size for border or a gap.
    static constexpr int DEFAULT_SIZE = 5;

    /// Child item alignment in the container, based on container orientation
    enum class Alignment : std::uint8_t {
        None,    ///< No auto align, child can set it's own alignment
        Leading, ///< Align to left or top edge of the container
        Center,  ///< Align to the middle of the container
        Trailing ///< Align to the right or bottom of the container
    };

    /// Construction options. `-1` for margin or gap means "platform
    /// default" — resolved through `wxSizerFlags::GetDefaultBorder()`.
    struct Options final {
        int gap = -1;                          ///< Per-item border on inner items (when nested).
        Alignment alignment = Alignment::None; ///< Cross-axis alignment applied to every item.
        bool margin = true;                    ///< leave gap sized margin on the edges of the orienation
    };

    SmartBoxSizer(Options options, wxOrientation orientation);

    /// Hooked from `wxBoxSizer::Layout`; reapplies per-item borders
    /// and outer padding based on current visibility, then defers to
    /// the base for the actual minimum-size computation.
    auto CalcMin() -> wxSize override;

    /// Update options. Call view Layout after setting this.
    void setOptions(const Options options) { m_options = options; }

private:
    /// Resolve `-1` to `wxSizerFlags::GetDefaultBorder()`.
    [[nodiscard]] auto resolvedGap() const -> int { return defaultSize(m_options.gap); }
    [[nodiscard]] static auto defaultSize(int value) -> int;

    /// Walk the managed item list (own children in single-sizer
    /// mode, inner sizer's children in nested mode) and update each
    /// item's border + flags from the current visibility state.
    void applyAutoLayout();

    /// Apply the configured cross-axis `Alignment` to `flags` —
    /// strips existing alignment bits and OR's in the orientation-
    /// correct one. No-op when `Alignment::None` or the item is
    /// `wxEXPAND` (which saturates the cross axis).
    [[nodiscard]] auto withAlignment(int flags) const -> int;

    /// Smart box options
    Options m_options;
};

} // namespace fbide
