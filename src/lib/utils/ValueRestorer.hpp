//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "NoCopy.hpp"
namespace fbide {

/**
 * RAII guard that saves a snapshot of one or more values on construction
 * and restores them on destruction. Useful for temporary state changes
 * that must be reverted when leaving a scope.
 */
template<std::copyable... Ts>
struct [[nodiscard]] ValueRestorer final {
    NO_COPY_AND_MOVE(ValueRestorer)

    [[nodiscard]] constexpr explicit ValueRestorer(Ts&... values)
    : m_targets { values... }
    , m_values { values... } {}

    constexpr ~ValueRestorer() {
        restore(std::index_sequence_for<Ts...> {});
    }

private:
    template<std::size_t... Is>
    constexpr void restore(std::index_sequence<Is...> /* seq */) {
        ((std::get<Is>(m_targets) = std::move(std::get<Is>(m_values))), ...);
    }

    std::tuple<Ts&...> m_targets;
    std::tuple<std::decay_t<Ts>...> m_values;
};

} // namespace fbide
