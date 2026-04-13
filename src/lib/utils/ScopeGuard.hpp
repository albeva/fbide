//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include <utility>
#include "NoCopy.hpp"

namespace fbide {

/// RAII guard that calls a callable on destruction.
/// Use via `makeScopeGuard(callable)`.
template<typename Fn>
class [[nodiscard]] ScopeGuard final {
    NO_COPY_AND_MOVE(ScopeGuard)
public:
    constexpr explicit ScopeGuard(Fn fn) : m_fn(std::move(fn)) {}
    constexpr ~ScopeGuard() { m_fn(); }

private:
    Fn m_fn;
};

} // namespace fbide
