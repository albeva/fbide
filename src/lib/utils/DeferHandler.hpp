//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "NoCopy.hpp"

namespace fbide {

/// RAII guard that runs `callback` on scope exit. Use the `DEFER(stmt)`
/// macro for the common case where you want a Go-style deferred
/// statement bound to the enclosing block.
template<std::invocable Callback>
class DeferHandler {
public:
    NO_COPY_AND_MOVE(DeferHandler)

    /// Move-construct from a callable. The callable runs on destruction.
    explicit DeferHandler(Callback&& callback) noexcept
    : m_callback(std::forward<Callback>(callback)) {}

    /// Run the captured callable.
    ~DeferHandler() noexcept {
        m_callback();
    }

private:
    Callback m_callback; ///< Captured callable invoked on scope exit.
};

template<typename F>
DeferHandler(F) -> DeferHandler<F>;

#define DEFER_CONCAT_INTERNAL(x, y) x##y
#define DEFER_CONCAT(x, y) DEFER_CONCAT_INTERNAL(x, y)
#define DEFER(STMT)                                      \
    const DeferHandler DEFER_CONCAT(_defer_, __LINE__) { \
        [&]() { STMT; }                                  \
    }

} // namespace fbide
