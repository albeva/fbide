//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

namespace fbide {

/**
 * RFC 4122 version-4 (random) universally unique identifier — 128 bits
 * of binary data with conventional `8-4-4-4-12` hex text representation.
 *
 * `IdentifierBase` uses `Uuid` as its default underlying type so
 * `Project::Id`, `Project::Node::Id`, etc. survive serialisation
 * round-trips (`.fbp` files, future workspace formats) and remain
 * unambiguous when project files are merged in version control.
 *
 * Storage is a flat 16-byte array; the implementation file delegates
 * generation, parsing, and canonical formatting to Boost.UUID so the
 * RFC 4122 details (variant bits, version stamping, hex encoding) live
 * in a battle-tested library. Boost is a **private** dependency of
 * `fbide_lib` — it does not appear in this header so callers don't
 * inherit Boost includes.
 *
 * Default-constructed `Uuid` is the **nil UUID** (all bytes zero) —
 * the conventional invalid sentinel; `bool(uuid)` reports validity.
 * Generate fresh values via `generate()`; parse / serialise via
 * `parse()` / `toString()`.
 *
 * Threading: `generate()` is safe to call from any thread — the
 * underlying Boost generator keeps a thread-local PRNG seeded from
 * the system entropy source.
 */
class Uuid final {
public:
    /// 128 bits / 8 = 16 bytes per UUID per RFC 4122.
    static constexpr std::size_t kBytes = 16;
    using Bytes = std::array<std::uint8_t, kBytes>;

    /// Default-construct to the nil UUID.
    constexpr Uuid() = default;

    /// Construct from explicit raw bytes — useful when reading a
    /// binary representation or in tests.
    constexpr explicit Uuid(const Bytes& bytes)
    : m_bytes(bytes) {}

    /// Generate a random v4 UUID.
    [[nodiscard]] static auto generate() -> Uuid;

    /// Parse the canonical `8-4-4-4-12` hex form (lower or upper case,
    /// brace-less). Returns `nullopt` on malformed input.
    [[nodiscard]] static auto parse(std::string_view text) -> std::optional<Uuid>;

    /// Canonical `8-4-4-4-12` lower-case hex string.
    [[nodiscard]] auto toString() const -> std::string;

    /// Raw byte view.
    [[nodiscard]] constexpr auto bytes() const -> const Bytes& { return m_bytes; }

    /// Equality / ordering follow bytewise comparison of the underlying
    /// 128-bit value.
    constexpr auto operator<=>(const Uuid&) const = default;

    /// Validity probe — `false` only for the nil UUID.
    explicit constexpr operator bool() const {
        return std::ranges::any_of(m_bytes, [](const std::uint8_t byte) { return byte != 0; });
    }

private:
    Bytes m_bytes {};
};

} // namespace fbide

/// Hash for use as a key in `std::unordered_map` / `std::unordered_set`.
/// Folds the 16 bytes via two pointer-width reads + the Boost
/// `hash_combine` mixing recipe — adequate for ID-table lookups,
/// not cryptographic. Defined inline so this header is self-contained
/// without dragging in Boost transitively.
template<>
struct std::hash<fbide::Uuid> {
    auto operator()(const fbide::Uuid& uuid) const noexcept -> size_t {
        // Boost `hash_combine` constants — the magic number is the
        // golden-ratio reciprocal scaled to 32 bits and is published
        // as part of Boost's well-known mixing recipe.
        constexpr std::size_t kHashCombineMagic = 0x9e3779b9;
        constexpr unsigned kHashCombineShiftLeft = 6;
        constexpr unsigned kHashCombineShiftRight = 2;

        const auto& bytes = uuid.bytes();
        std::size_t high = 0;
        std::size_t low = 0;
        std::memcpy(&high, bytes.data(), sizeof(std::size_t));
        std::memcpy(&low, std::next(bytes.data(), sizeof(std::size_t)), sizeof(std::size_t));
        return high ^ (low + kHashCombineMagic + (high << kHashCombineShiftLeft) + (high >> kHashCombineShiftRight));
    }
};
