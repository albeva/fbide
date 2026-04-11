//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include <utility>
#include "Macros.hpp"

namespace fbide {

/// Non-owning pointer wrapper. Wraps `new` to silence linter warnings
/// while making it explicit that we do not own the pointed-to object
/// (e.g. wxWidgets manages lifetime of windows via parent-child hierarchy).
template<typename T>
class Unowned final {
public:
    /// Construct from raw pointer.
    constexpr explicit Unowned(T* ptr)
    : m_ptr(ptr) {}

    /// Reassign to a different raw pointer.
    constexpr FBIDE_INLINE auto operator=(T* ptr) -> Unowned& {
        m_ptr = ptr;
        return *this;
    }

    /// Access member of pointed-to object.
    constexpr FBIDE_INLINE auto operator->() const -> T* { return m_ptr; }

    /// Dereference pointed-to object.
    constexpr FBIDE_INLINE auto operator*() const -> T& { return *m_ptr; }

    /// Implicit conversion to raw pointer.
    constexpr FBIDE_INLINE operator T*() const { return m_ptr; } // NOLINT(google-explicit-constructor)

    /// Get raw pointer.
    [[nodiscard]] constexpr FBIDE_INLINE auto get() const -> T* { return m_ptr; }

    /// Test if pointer is non-null.
    constexpr FBIDE_INLINE operator bool() const { return m_ptr != nullptr; } // NOLINT(google-explicit-constructor)

private:
    T* m_ptr;
};

/// Create an Unowned<T> by constructing T with the given arguments.
template<typename T, typename... Args>
FBIDE_INLINE auto make_unowned(Args&&... args) -> Unowned<T> {
    return Unowned<T>(new T(std::forward<Args>(args)...));
}

} // namespace fbide
