//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide::ai {

/**
 * Drag / hover / horizontal-wheel bookkeeping for the per-code-block
 * horizontal scrollbars in the chat view.
 *
 * Holds no widget state — the chat view owns the per-block scroll
 * offsets (they live on each `LaidMessage`) and the hit-test (it needs
 * the bubble + block geometry). This class owns the ephemeral state
 * that doesn't migrate with bubbles: which thumb is being dragged,
 * which one is hovered, and the fractional accumulator that lets
 * macOS trackpad sub-tick rotations register over time.
 *
 * Geometry constants used by both the paint pass and the hit-test
 * live here as `static constexpr` so the two stay in lock step.
 */
class BlockScrollController final {
public:
    NO_COPY_AND_MOVE(BlockScrollController)

    BlockScrollController() = default;

    /// Track height in pixels.
    static constexpr int kHeight = 6;
    /// Minimum thumb width — keeps the thumb grabbable even on very
    /// long blocks.
    static constexpr int kMinThumb = 24;
    /// Alphas applied to the bubble's foreground colour for the track,
    /// the idle thumb and the active (hovered or dragged) thumb.
    static constexpr unsigned char kTrackAlpha = 60;
    static constexpr unsigned char kThumbAlpha = 160;
    static constexpr unsigned char kThumbActiveAlpha = 220;

    /// Thumb width for a `trackW`-wide track over a block whose
    /// content vs. natural widths are given. Common formula shared by
    /// paint, hit-test and the click-to-jump path.
    [[nodiscard]] static auto thumbWidth(int trackW, int blockContentWidth, int blockNaturalWidth) -> int;

    /// Translate the current pointer-x into a new block scroll offset
    /// using the active drag's start state. `trackW`, `blockContentWidth`
    /// and `blockNaturalWidth` describe the geometry the caller is
    /// dragging across.
    [[nodiscard]] auto translateDrag(int currentMouseX, int trackW, int blockContentWidth, int blockNaturalWidth) const -> int;

    /// Accumulate a horizontal wheel/swipe rotation and return the
    /// pixel delta to scroll by. Returns 0 when the accumulated
    /// fraction hasn't crossed the per-tick threshold yet.
    [[nodiscard]] auto accumulateHorizontalWheel(int wheelDelta, int wheelRotation) -> int;

    /// Axis of a wheel event for the axis-lock check below.
    enum class WheelAxis : std::uint8_t {
        Horizontal,
        Vertical
    };

    /// Idle threshold between events within a single trackpad gesture.
    /// Two events farther apart in time count as belonging to separate
    /// gestures, so the axis lock resets. Tuned low (60 ms) so a quick
    /// switch from vertical to horizontal swipe — or vice versa —
    /// takes effect within one frame; trackpad gestures themselves
    /// emit events at roughly 8 ms cadence, so a real continuous
    /// gesture stays well inside this window.
    static constexpr auto kGestureIdle = std::chrono::milliseconds(60);

    /// Axis-lock check for the wheel handler. The first event of a
    /// fresh gesture (no on-axis event within `kGestureIdle`) adopts
    /// `axis` as the gesture's locked direction and returns `true`.
    /// Subsequent off-axis events return `false` so the caller can
    /// suppress them — keeps a vertical trackpad scroll from
    /// accidentally jiggling a code/patch block sideways and vice
    /// versa.
    ///
    /// Only ACCEPTED events refresh the gesture-idle timer. macOS
    /// emits a long momentum tail after a flick; if off-axis events
    /// refreshed the timer too, the user's next gesture would never
    /// out-wait the previous lock and the off-axis trackpad direction
    /// would stay suppressed indefinitely.
    auto acquireWheelAxis(WheelAxis axis) -> bool;

    // Drag bookkeeping ------------------------------------------------
    void beginDrag(std::size_t messageIndex, std::size_t blockIndex, int startOffset, int startMouseX);
    void endDrag();
    [[nodiscard]] auto isDragging() const -> bool { return m_dragMessageIndex >= 0; }
    [[nodiscard]] auto dragMessageIndex() const -> int { return m_dragMessageIndex; }
    [[nodiscard]] auto dragMessage() const -> std::size_t { return static_cast<std::size_t>(m_dragMessageIndex); }
    [[nodiscard]] auto dragBlock() const -> std::size_t { return m_dragBlockIndex; }

    // Hover bookkeeping -----------------------------------------------
    /// Set / clear the hovered scrollbar. Returns `true` when the hover
    /// actually changed so the caller can decide whether to repaint.
    auto setHover(std::size_t messageIndex, std::size_t blockIndex) -> bool;
    auto clearHover() -> bool;
    [[nodiscard]] auto hoverActive() const -> bool { return m_hoverMessageIndex >= 0; }
    [[nodiscard]] auto hoverMessageIndex() const -> int { return m_hoverMessageIndex; }
    [[nodiscard]] auto hoverMessage() const -> std::size_t { return static_cast<std::size_t>(m_hoverMessageIndex); }
    [[nodiscard]] auto hoverBlock() const -> std::size_t { return m_hoverBlockIndex; }

private:
    int m_dragMessageIndex = -1;
    std::size_t m_dragBlockIndex = 0;
    int m_dragStartOffset = 0;
    int m_dragStartMouseX = 0;
    int m_hoverMessageIndex = -1;
    std::size_t m_hoverBlockIndex = 0;
    int m_hwheelPixelAccum = 0;

    /// Axis-lock state — see `acquireWheelAxis`.
    bool m_hasLockedAxis = false;
    WheelAxis m_lockedAxis = WheelAxis::Horizontal;
    std::chrono::steady_clock::time_point m_lastWheelTime;
};

} // namespace fbide::ai
