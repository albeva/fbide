//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "Macros.hpp"

namespace fbide {

/// Non-owning pointer wrapper. Wraps `new` to silence linter warnings
/// while making it explicit that we do not own the pointed-to object
/// (e.g. wxWidgets manages lifetime of windows via parent-child hierarchy).
template<typename T>
class Unowned final {
public:
    /// Default construct to nullptr.
    constexpr Unowned() = default;

    /// Construct from nullptr.
    constexpr Unowned(std::nullptr_t) {} // NOLINT(google-explicit-constructor)

    /// Construct from raw pointer.
    constexpr explicit Unowned(T* ptr)
    : m_ptr(ptr) {}

    /// Copy construct.
    constexpr Unowned(const Unowned&) = default;

    /// Move construct.
    constexpr Unowned(Unowned&& other) noexcept
    : m_ptr(std::exchange(other.m_ptr, nullptr)) {}

    /// Copy assign.
    constexpr auto operator=(const Unowned&) -> Unowned& = default;

    /// Move assign.
    constexpr auto operator=(Unowned&& other) noexcept -> Unowned& {
        m_ptr = std::exchange(other.m_ptr, nullptr);
        return *this;
    }

    /// Assign from raw pointer.
    constexpr auto operator=(T* ptr) -> Unowned& {
        m_ptr = ptr;
        return *this;
    }

    /// Assign from nullptr.
    constexpr auto operator=(std::nullptr_t) -> Unowned& {
        m_ptr = nullptr;
        return *this;
    }

    ~Unowned() = default;

    /// Access member of pointed-to object.
    constexpr FBIDE_INLINE auto operator->() const -> T* { return m_ptr; }

    /// Dereference pointed-to object.
    constexpr FBIDE_INLINE auto operator*() const -> T& { return *m_ptr; }

    /// Implicit conversion to raw pointer.
    constexpr FBIDE_INLINE operator T*() const { return m_ptr; } // NOLINT(google-explicit-constructor)

    /// Get raw pointer.
    [[nodiscard]] constexpr FBIDE_INLINE auto get() const -> T* { return m_ptr; }

    /// Three-way comparison with another Unowned.
    constexpr auto operator<=>(const Unowned&) const = default;

    /// Compare with nullptr.
    constexpr FBIDE_INLINE auto operator==(std::nullptr_t) const -> bool { return m_ptr == nullptr; }

    /// Materialize this into a unique_ptr, which takes ownership of the held pointer
    constexpr FBIDE_INLINE auto toUniquePtr() const -> std::unique_ptr<T> { return std::unique_ptr<T>(m_ptr); }

private:
    T* m_ptr = nullptr;
};

/// Create an Unowned<T> by constructing T with the given arguments.
template<typename T, typename... Args>
FBIDE_INLINE auto make_unowned(Args&&... args) -> Unowned<T> {
    return Unowned<T>(new T(std::forward<Args>(args)...));
}

} // namespace fbide
