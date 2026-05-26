//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "Uuid.hpp"

namespace fbide {

/**
 * Phantom-type-tagged value for strong-typedef identifiers.
 *
 * The `Tag` template parameter exists solely to distinguish IDs of
 * different kinds at the type level — `IdentifierBase<A>` and
 * `IdentifierBase<B>` are unrelated, non-convertible types even when
 * their underlying values are the same. The tag may be incomplete or
 * forward-declared; it never needs a definition because the template
 * does not depend on its size or members.
 *
 * Defaults: `Underlying = Uuid` — the conventional invalid sentinel is
 * the default-constructed underlying value (nil UUID for `Uuid`, zero
 * for integral types). Explicit conversion to `bool` reports validity.
 *
 * Typical usage:
 *
 *     class Project final {
 *     public:
 *         using Id = IdentifierBase<Project>;          // Uuid-backed.
 *         class Node final {
 *         public:
 *             using Id = IdentifierBase<Node>;         // Uuid-backed.
 *         };
 *     };
 *
 * `std::hash` is partially-specialised below for every instantiation,
 * so identifiers can be used as keys in `std::unordered_map` /
 * `std::unordered_set` out of the box, provided the underlying type
 * itself has a `std::hash` specialisation.
 */
template<typename Tag, typename UnderlyingT = Uuid>
class IdentifierBase final {
public:
    using Underlying = UnderlyingT;

    /// Default-construct to the invalid sentinel (default-constructed
    /// underlying value — nil UUID, zero integer, etc.).
    constexpr IdentifierBase() = default;

    /// Construct from an explicit underlying value. `explicit` so raw
    /// underlying values do not implicitly become identifiers at call
    /// sites.
    constexpr explicit IdentifierBase(Underlying value)
    : m_value(std::move(value)) {}

    /// Underlying value; useful for serialisation or hashing. Returned
    /// by const reference so larger underlying types (e.g. `Uuid` at
    /// 16 bytes) don't pay a copy on every read.
    [[nodiscard]] constexpr auto value() const -> const Underlying& { return m_value; }

    /// Comparable to itself; ordering follows the underlying value.
    constexpr auto operator<=>(const IdentifierBase&) const = default;

    /// Validity probe. `false` only when the underlying value equals
    /// its default-constructed sentinel.
    explicit constexpr operator bool() const { return m_value != Underlying {}; }

private:
    Underlying m_value {};
};

} // namespace fbide

/// Partial specialisation that makes any `IdentifierBase<Tag, Underlying>`
/// hashable. Forwards to `std::hash<Underlying>` so the hash quality
/// follows whatever the underlying type's hash provides.
template<typename Tag, typename Underlying>
struct std::hash<fbide::IdentifierBase<Tag, Underlying>> {
    auto operator()(const fbide::IdentifierBase<Tag, Underlying>& id) const noexcept -> size_t {
        return hash<Underlying> {}(id.value());
    }
};
