//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

namespace fbide {

/// Free generator for fresh v4 UUIDs — delegates to Boost's
/// `random_generator`, which keeps a thread-local PRNG so calls from
/// different threads don't contend. Defined out-of-line so the heavy
/// generator headers (`random_generator.hpp` and its `<random>` deps)
/// stay in the implementation file.
[[nodiscard]] auto generateUuid() -> boost::uuids::uuid;

/**
 * Phantom-type-tagged identifier — a strong-typedef wrapper around a
 * `boost::uuids::uuid` (by default) so IDs of different kinds cannot
 * be silently interchanged.
 *
 * The `Tag` template parameter exists solely for type distinction;
 * `IdentifierBase<A>` and `IdentifierBase<B>` are unrelated, non-
 * convertible types even when the underlying value happens to be the
 * same. The tag never needs a definition — a forward-declared class
 * suffices.
 *
 * Defaults: `Underlying = boost::uuids::uuid` — the default-constructed
 * underlying value is the conventional invalid sentinel (nil UUID, or
 * zero for integer specialisations). Explicit conversion to `bool`
 * reports validity.
 *
 * Typical usage:
 *
 *     class Project final {
 *     public:
 *         using Id = IdentifierBase<Project>;          // UUID-backed.
 *         class Node final {
 *         public:
 *             using Id = IdentifierBase<Node>;         // UUID-backed.
 *         };
 *     };
 *     const Project::Id fresh = Project::Id::generate();
 *
 * `std::hash` is partially-specialised below for every instantiation
 * so identifiers can be used as keys in `std::unordered_map` /
 * `std::unordered_set` out of the box, provided the underlying type
 * itself has a `std::hash` specialisation.
 */
template<typename Tag, typename UnderlyingT = boost::uuids::uuid>
class IdentifierBase final {
public:
    using Underlying = UnderlyingT;

    /// Default-construct to the invalid sentinel.
    constexpr IdentifierBase() = default;

    /// Construct from an explicit underlying value. `explicit` so raw
    /// underlying values don't implicitly become identifiers at call
    /// sites.
    constexpr explicit IdentifierBase(Underlying value)
    : m_value(std::move(value)) {}

    /// Allocate a fresh identifier. Only enabled when the underlying
    /// type is `boost::uuids::uuid` — for other underlying types the
    /// caller is expected to supply the value explicitly.
    [[nodiscard]] static auto generate() -> IdentifierBase
        requires std::is_same_v<UnderlyingT, boost::uuids::uuid>
    {
        return IdentifierBase { generateUuid() };
    }

    /// Underlying value; useful for serialisation or hashing. Returned
    /// by const reference so larger underlying types (UUID is 16 bytes)
    /// don't pay a copy on every read.
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
