//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "BlockScrollController.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {
// Pixels per wheel notch for horizontal scroll routing — matches the
// onMouseWheel path before the extraction.
constexpr int kHorizPxPerNotch = 60;
// 9/10 damper to slow scroll by ~10% without losing carry precision.
constexpr int kDamperNum = 9;
constexpr int kDamperDen = 10;
} // namespace

auto BlockScrollController::thumbWidth(const int trackW, const int blockContentWidth, const int blockNaturalWidth) -> int {
    if (blockNaturalWidth <= 0) {
        return kMinThumb;
    }
    const double ratio = static_cast<double>(blockContentWidth) / static_cast<double>(blockNaturalWidth);
    return std::max(kMinThumb, static_cast<int>(trackW * ratio));
}

auto BlockScrollController::translateDrag(
    const int currentMouseX,
    const int trackW,
    const int blockContentWidth,
    const int blockNaturalWidth
) const -> int {
    const int maxScroll = std::max(0, blockNaturalWidth - blockContentWidth);
    if (maxScroll <= 0) {
        return m_dragStartOffset;
    }
    const int thumbW = thumbWidth(trackW, blockContentWidth, blockNaturalWidth);
    const int travel = std::max(1, trackW - thumbW);
    const int deltaPx = currentMouseX - m_dragStartMouseX;
    return m_dragStartOffset + (deltaPx * maxScroll / travel);
}

auto BlockScrollController::accumulateHorizontalWheel(const int wheelDelta, const int wheelRotation) -> int {
    if (wheelDelta <= 0) {
        return 0;
    }
    const int divisor = wheelDelta * kDamperDen;
    m_hwheelPixelAccum += wheelRotation * kHorizPxPerNotch * kDamperNum;
    const int pixels = m_hwheelPixelAccum / divisor;
    m_hwheelPixelAccum -= pixels * divisor;
    return pixels;
}

auto BlockScrollController::acquireWheelAxis(const WheelAxis axis) -> bool {
    const auto now = std::chrono::steady_clock::now();
    if (!m_hasLockedAxis || (now - m_lastWheelTime) > kGestureIdle) {
        // Fresh gesture — adopt this event's axis as the lock.
        m_lockedAxis = axis;
        m_hasLockedAxis = true;
    }
    m_lastWheelTime = now;
    return m_lockedAxis == axis;
}

void BlockScrollController::beginDrag(const std::size_t messageIndex, const std::size_t blockIndex, const int startOffset, const int startMouseX) {
    m_dragMessageIndex = static_cast<int>(messageIndex);
    m_dragBlockIndex = blockIndex;
    m_dragStartOffset = startOffset;
    m_dragStartMouseX = startMouseX;
}

void BlockScrollController::endDrag() {
    m_dragMessageIndex = -1;
}

auto BlockScrollController::setHover(const std::size_t messageIndex, const std::size_t blockIndex) -> bool {
    if (std::cmp_equal(m_hoverMessageIndex, messageIndex) && m_hoverBlockIndex == blockIndex) {
        return false;
    }
    m_hoverMessageIndex = static_cast<int>(messageIndex);
    m_hoverBlockIndex = blockIndex;
    return true;
}

auto BlockScrollController::clearHover() -> bool {
    if (m_hoverMessageIndex < 0) {
        return false;
    }
    m_hoverMessageIndex = -1;
    return true;
}
