//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

namespace fbide {

/**
 * Phantom-type-tagged integer for strong-typedef identifiers.
 *
 * The `Tag` template parameter exists solely to distinguish IDs of
 * different kinds at the type level — `IdentifierBase<A>` and
 * `IdentifierBase<B>` are unrelated, non-convertible types even when
 * their underlying integers are the same. The tag may be incomplete or
 * forward-declared; it never needs a definition because the template
 * does not depend on its size or members.
 *
 * Defaults: `Underlying = std::size_t`; the default-constructed value
 * is `0`, which is the conventional **invalid sentinel**. Explicit
 * conversion to `bool` reports validity (`true` for any non-zero
 * value).
 *
 * Typical usage:
 *
 *     class Project final {
 *     public:
 *         using Id = IdentifierBase<Project>;
 *         class Node final {
 *         public:
 *             using Id = IdentifierBase<Node>;
 *         };
 *     };
 *
 * `std::hash` is partially-specialised below for every instantiation,
 * so identifiers can be used as keys in `std::unordered_map` /
 * `std::unordered_set` out of the box.
 */
template<typename Tag, std::integral UnderlyingT = std::size_t>
class IdentifierBase final {
public:
    using Underlying = UnderlyingT;

    /// Default-construct to the invalid sentinel (`0`).
    constexpr IdentifierBase() = default;

    /// Construct from an explicit underlying value. `explicit` so plain
    /// integers do not implicitly become identifiers at call sites.
    constexpr explicit IdentifierBase(const Underlying value)
    : m_value(value) {}

    /// Underlying integer; useful for serialisation or hashing.
    [[nodiscard]] constexpr auto value() const -> Underlying { return m_value; }

    /// Comparable to itself; ordering follows the underlying integer.
    constexpr auto operator<=>(const IdentifierBase&) const = default;

    /// Validity probe. `false` only for the invalid sentinel (`0`).
    explicit constexpr operator bool() const { return m_value != 0; }

private:
    Underlying m_value = 0;
};

} // namespace fbide

/// Partial specialisation that makes any `IdentifierBase<Tag, Underlying>`
/// hashable. Forwards to `std::hash<Underlying>` so the hash quality
/// follows whatever the underlying integer hash provides.
template<typename Tag, std::integral Underlying>
struct std::hash<fbide::IdentifierBase<Tag, Underlying>> {
    auto operator()(const fbide::IdentifierBase<Tag, Underlying>& id) const noexcept -> size_t {
        return hash<Underlying> {}(id.value());
    }
};
