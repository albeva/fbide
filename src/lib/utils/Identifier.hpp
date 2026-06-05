//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

namespace fbide {

/// How an `IdentifierBase` mints a fresh value in `generate()`.
enum class IdKind : std::uint8_t {
    /// Monotonic counter (from 1). Process-unique and compact; for in-process
    /// handles that are never serialised.
    Counter,
    /// Time-seeded random value (high 32 bits = Unix-epoch seconds, low 32 =
    /// random), rendered as a short base-62 string. Loosely time-ordered, yet
    /// minted without shared state, so ids created in separate files /
    /// processes don't collide — safe to merge a serialised tree under version
    /// control or to compose child projects. Callers that need a hard
    /// guarantee within a scope re-roll on the rare clash; see
    /// `Project::makeNodeId`.
    Random,
};

namespace detail {
    /// Tag-independent operations on the 64-bit value behind every identifier.
    /// A non-template helper so the heavy `<random>` header and the base-62 codec
    /// stay in `Identifier.cpp` rather than leaking into this (PCH-wide) header.
    struct IdValue {
        /// Fresh id value: a 32-bit Unix timestamp (seconds) in the high half
        /// and a 32-bit random in the low half — loosely time-ordered, non-zero.
        [[nodiscard]] static auto random() -> std::uint64_t;
        /// Next monotonic counter value (starts at 1; process-wide).
        [[nodiscard]] static auto next() -> std::uint64_t;
        /// Base-62 (`0-9A-Za-z`) form — compact and separator-free (≤ 11 chars).
        [[nodiscard]] static auto toString(std::uint64_t value) -> std::string;
        /// Parse a base-62 string. Throws `std::runtime_error` on an invalid
        /// character or 64-bit overflow. Round-trips `toString`.
        [[nodiscard]] static auto fromString(std::string_view text) -> std::uint64_t;
    };
} // namespace detail

/**
 * Phantom-type-tagged identifier — a strong typedef around a `std::uint64_t`
 * so ids of different kinds cannot be silently interchanged.
 *
 * `Tag` exists solely for type distinction: `IdentifierBase<A>` and
 * `IdentifierBase<B>` are unrelated, non-convertible types even when the
 * underlying value matches. The tag never needs a definition — a
 * forward-declared class suffices.
 *
 * `Kind` selects how `generate()` mints a value (see `IdKind`): a `Counter`
 * for in-process-only handles, `Random` (the default) for ids that round-trip
 * through serialisation as base 62.
 *
 * The default-constructed value (`0`) is the invalid sentinel; explicit
 * conversion to `bool` reports validity.
 *
 * Typical usage:
 *
 *     class Project final {
 *     public:
 *         using Id = IdentifierBase<Project, IdKind::Counter>;   // in-process.
 *         class Node final {
 *         public:
 *             using Id = IdentifierBase<Node>;                   // Random; serialised.
 *         };
 *     };
 *     const auto fresh = Project::Node::Id::generate();
 *     const auto same  = Project::Node::Id { fresh.string() };   // base-62 round-trip
 *
 * `std::hash` is partially-specialised below for every instantiation so
 * identifiers can be used as keys in `std::unordered_map` /
 * `std::unordered_set` out of the box.
 */
template<typename Tag, IdKind Kind = IdKind::Random>
class IdentifierBase final {
public:
    /// Default-construct to the invalid sentinel (`0`).
    constexpr IdentifierBase() = default;

    /// Construct from an explicit value. `explicit` so raw integers don't
    /// implicitly become identifiers at call sites.
    constexpr explicit IdentifierBase(std::uint64_t value)
    : m_value(value) {}

    /// Construct from a base-62 string (the serialised form). Throws
    /// `std::runtime_error` on malformed input. `explicit` to match the value
    /// constructor (no implicit string → identifier conversions).
    explicit IdentifierBase(const std::string_view text)
    : m_value(detail::IdValue::fromString(text)) {}

    /// Mint a fresh identifier following the `Kind` policy.
    [[nodiscard]] static auto generate() -> IdentifierBase {
        if constexpr (Kind == IdKind::Counter) {
            return IdentifierBase { detail::IdValue::next() };
        } else {
            return IdentifierBase { detail::IdValue::random() };
        }
    }

    /// Underlying value; useful for hashing.
    [[nodiscard]] constexpr auto value() const -> std::uint64_t { return m_value; }

    /// Base-62 string form of the underlying value. Round-trips with the
    /// `std::string_view` constructor.
    [[nodiscard]] auto string() const -> std::string { return detail::IdValue::toString(m_value); }

    /// Comparable to itself; ordering follows the underlying value.
    constexpr auto operator<=>(const IdentifierBase&) const = default;

    /// Validity probe. `false` only for the default-constructed sentinel.
    explicit constexpr operator bool() const { return m_value != 0; }

private:
    std::uint64_t m_value {};
};

} // namespace fbide

/// Partial specialisation that makes any `IdentifierBase<Tag, Kind>` hashable.
template<typename Tag, fbide::IdKind Kind>
struct std::hash<fbide::IdentifierBase<Tag, Kind>> {
    auto operator()(const fbide::IdentifierBase<Tag, Kind>& id) const noexcept -> size_t {
        return hash<std::uint64_t> {}(id.value());
    }
};
