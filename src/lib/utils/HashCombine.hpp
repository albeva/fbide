//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Boost-style hash combiner.
///
/// The golden-ratio mixing constant is sized to `std::size_t` so the
/// function compiles on both 32-bit and 64-bit targets — the
/// compile-time pick keeps `kMix` representable on either platform.
/// Order-sensitive: `hashCombine(a, b)` differs from `hashCombine(b, a)`
/// in general, so it is safe to fold a sequence of hashes for a tuple-
/// like value.
[[nodiscard]] inline auto hashCombine(const std::size_t seed, const std::size_t value) -> std::size_t {
    constexpr std::size_t kMix = sizeof(std::size_t) >= 8
                                   ? 0x9e3779b97f4a7c15ULL
                                   : 0x9e3779b9UL;
    // Shift constants 6 and 2 are part of the standard boost::hash_combine
    // recipe and aren't meaningful enough to name independently.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    return seed ^ (value + kMix + (seed << 6) + (seed >> 2));
}

} // namespace fbide
