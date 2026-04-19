//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <cmake/config.hpp>

namespace fbide {

class [[nodiscard]] Version final {
public:
    // plumbing
    constexpr Version() noexcept = default;

    /// Get current fbide version
    [[nodiscard]] static constexpr auto fbide() -> Version {
        return { cmake::project.major, cmake::project.minor, cmake::project.patch };
    }

    /// Get old fbide version
    [[nodiscard]] static constexpr auto oldFbide() -> Version {
        return { 0, 4, 6 };
    }

    /// Get wxWidgets version
    [[nodiscard]] static constexpr auto wxWidgets() -> Version {
        return { wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER };
    }

    /// Create from string
    explicit Version(const wxString& version) noexcept;

    /// Create from components
    constexpr Version(const int major, const int minor, const int patch) noexcept
    : m_major(major)
    , m_minor(minor)
    , m_patch(patch) {}

    /// Get as string
    [[nodiscard]] auto asString() const -> wxString;

    /// Is this a valid version?
    [[nodiscard]] constexpr auto isValid() const noexcept -> bool {
        return m_major > 0 && m_minor > 0 && m_patch > 0;
    }

    /// Get version bits
    [[nodiscard]] constexpr auto getMajor() const noexcept -> int { return m_major; }
    [[nodiscard]] constexpr auto getMinor() const noexcept -> int { return m_minor; }
    [[nodiscard]] constexpr auto getPatch() const noexcept -> int { return m_patch; }

    /// Compare versions
    [[nodiscard]] constexpr auto operator<=>(const Version& other) const noexcept -> auto = default;

private:
    int m_major = 0, m_minor = 0, m_patch = 0;
};

} // namespace fbide
