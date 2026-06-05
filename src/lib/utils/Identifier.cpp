//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Identifier.hpp"
#include <chrono>
#include <random>

using namespace fbide;

namespace {
/// Digit set for base-62 encoding. Order fixes the value of each character.
constexpr std::string_view kBase62Alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr std::uint64_t kBase = 62;
} // namespace

auto detail::IdValue::random() -> std::uint64_t {
    // Layout: high 32 bits = seconds since the Unix epoch, low 32 bits =
    // random. The timestamp loosely time-orders ids (and gives base-62 strings
    // a shared prefix per era); the random half provides within-second
    // uniqueness (Project::makeNodeId re-rolls on the rare clash). The
    // timestamp keeps the high bits set, so the value is never the `0` sentinel
    // in practice — the loop only matters at the epoch.
    static thread_local std::mt19937_64 engine { std::random_device {}() };
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    )
                             .count();
    const std::uint64_t high = static_cast<std::uint64_t>(static_cast<std::uint32_t>(seconds)) << 32U;
    std::uint64_t value = high | static_cast<std::uint32_t>(engine());
    while (value == 0) {
        value = high | static_cast<std::uint32_t>(engine());
    }
    return value;
}

auto detail::IdValue::next() -> std::uint64_t {
    // Process-wide monotonic counter; `0` is reserved as the invalid sentinel.
    // UI-thread only, so no synchronisation is needed.
    static std::uint64_t counter = 0;
    return ++counter;
}

auto detail::IdValue::toString(std::uint64_t value) -> wxString {
    if (value == 0) {
        return "0";
    }
    // Emit least-significant digit first, prepending so the result reads
    // most-significant first. A 64-bit value is at most 11 digits, so the
    // front-insert cost is irrelevant.
    std::string out;
    while (value > 0) {
        out.insert(out.begin(), kBase62Alphabet[static_cast<std::size_t>(value % kBase)]);
        value /= kBase;
    }
    return wxString::FromUTF8(out);
}

auto detail::IdValue::fromString(const wxString& text) -> std::uint64_t {
    if (text.empty()) {
        throw std::runtime_error("IdValue::fromString: empty input");
    }
    std::uint64_t value = 0;
    for (const char chr : text.utf8_string()) { // base-62 is ASCII
        const auto digit = kBase62Alphabet.find(chr);
        if (digit == std::string_view::npos) {
            throw std::runtime_error("IdValue::fromString: invalid character");
        }
        // Guard the multiply-add against 64-bit overflow before applying it.
        if (value > (UINT64_MAX - digit) / kBase) {
            throw std::runtime_error("IdValue::fromString: value out of range");
        }
        value = value * kBase + digit;
    }
    return value;
}
