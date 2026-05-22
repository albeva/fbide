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
 * A `wxBoxSizer` subclass that owns per-item borders so layout code
 * doesn't have to compute them at every Add.
 *
 * Layout model
 * ------------
 * - `gap` is the single border size used for both the inter-item
 *   spacing and (when `margin` is on) the outer padding. `-1`
 *   resolves to `wxSizerFlags::GetDefaultBorder()`.
 * - `margin = true` (default) puts a `gap`-sized border on the
 *   sizer's outer edges so visible items sit inset from the parent.
 * - `margin = false` leaves the outer edges flush — useful when the
 *   surrounding sizer already provides the padding (e.g. a nested
 *   `vbox` inside another `vbox`).
 *
 * Per-item flags applied during `CalcMin` (only visible items):
 * - inter-item gap as the leading-edge border (`wxLEFT` for
 *   horizontal sizers, `wxTOP` for vertical ones) on every
 *   non-first visible item.
 * - outer leading-edge border on the first visible item when
 *   `margin` is on.
 * - outer trailing-edge border on the last visible item when
 *   `margin` is on.
 * - cross-axis border on every visible item when `margin` is on.
 *
 * Visibility-driven behaviour
 * ---------------------------
 * - The recompute runs from `CalcMin`, so any `Show`/`Hide`/`Add`/
 *   `Remove` picked up by a normal `Layout()` pass reaches us
 *   without needing explicit notification.
 * - With no visible children the sizer reports a zero minimum size
 *   — no items, no margin, no work.
 *
 * `Options::alignment` applies cross-axis alignment to every visible
 * item. Items added with `wxEXPAND` are left alone — expanding
 * already saturates the cross axis.
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
    [[nodiscard]] auto resolvedGap() const;

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
