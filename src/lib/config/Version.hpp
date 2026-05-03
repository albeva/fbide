//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "cmake/config.hpp"

namespace fbide {

/// Semantic version with an optional pre-release marker. Comparable,
/// parsable from strings, and stamped at compile-time for the FBIde and
/// wxWidgets build versions via the static factories.
///
/// String form is `MAJOR.MINOR.PATCH[.TAG-TWEAK]` — for example
/// `0.5.0`, `0.5.0.alpha-1`, `0.5.0.rc-2`. The tweak is omitted (and
/// stored as 0) when no tag is present. Pre-release sorts BEFORE the
/// matching final release: `0.5.0.alpha-1 < 0.5.0.rc-1 < 0.5.0`.
class [[nodiscard]] Version final {
public:
    /// Re-export the generated enum so callers can write the familiar
    /// `Version::Tag::Alpha` form. The actual definition lives in
    /// the cmake-generated config.hpp (see `fbide::VersionTag`).
    using Tag = VersionTag;

    /// Default-constructed `0.0.0`.
    constexpr Version() noexcept = default;

    /// Get current FBIde version (resolved at compile time from CMake-
    /// generated config.hpp).
    [[nodiscard]] static auto fbide() noexcept -> Version;

    /// Get the legacy fbide-old version this rewrite descends from.
    [[nodiscard]] static constexpr auto oldFbide() noexcept -> Version {
        return { 0, 4, 6 };
    }

    /// Get linked wxWidgets version.
    [[nodiscard]] static constexpr auto wxWidgets() noexcept -> Version {
        return { wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER };
    }

    /// Parse from string `MAJOR.MINOR.PATCH[.TAG[-TWEAK]]`. Unknown tags
    /// or otherwise malformed input collapse to `Tag::None` / 0 — only
    /// strings the project itself emits are parsed, so a tolerant style
    /// is intentional.
    explicit Version(const wxString& version) noexcept;

    /// Construct from components.
    constexpr Version(
        const int major, const int minor, const int patch,
        const Tag tag = Tag::None, const int tweak = 0
    ) noexcept
    : m_major(major)
    , m_minor(minor)
    , m_patch(patch)
    , m_tag(tag)
    , m_tweak(tweak) {}

    /// Format as `MAJOR.MINOR.PATCH` or `MAJOR.MINOR.PATCH.tag-tweak`.
    [[nodiscard]] auto asString() const -> wxString;

    /// Lowercase keyword for the tag (`"alpha"`, `"beta"`, `"rc"`),
    /// empty string when `tag == Tag::None`.
    [[nodiscard]] static auto tagToString(Tag tag) -> wxString;

    /// True for `0.0.0` (uninitialised) — kept loose, the original
    /// implementation had the same behaviour.
    [[nodiscard]] constexpr auto isValid() const noexcept -> bool {
        return m_major > 0 && m_minor > 0 && m_patch > 0;
    }

    [[nodiscard]] constexpr auto getMajor() const noexcept -> int { return m_major; }
    [[nodiscard]] constexpr auto getMinor() const noexcept -> int { return m_minor; }
    [[nodiscard]] constexpr auto getPatch() const noexcept -> int { return m_patch; }
    [[nodiscard]] constexpr auto getTag()   const noexcept -> Tag { return m_tag; }
    [[nodiscard]] constexpr auto getTweak() const noexcept -> int { return m_tweak; }

    /// Lexicographic compare on (major, minor, patch, tagRank, tweak).
    /// `tagRank()` makes `Tag::None` the greatest so a final release
    /// outranks every pre-release at the same numeric triple.
    [[nodiscard]] friend constexpr auto operator<=>(const Version& a, const Version& b) noexcept -> std::strong_ordering {
        if (const auto c = a.m_major <=> b.m_major; c != 0) return c;
        if (const auto c = a.m_minor <=> b.m_minor; c != 0) return c;
        if (const auto c = a.m_patch <=> b.m_patch; c != 0) return c;
        if (const auto c = tagRank(a.m_tag) <=> tagRank(b.m_tag); c != 0) return c;
        return a.m_tweak <=> b.m_tweak;
    }

    [[nodiscard]] friend constexpr auto operator==(const Version& a, const Version& b) noexcept -> bool = default;

private:
    /// Sort key for `Tag` — `None` is the largest so finals beat
    /// pre-releases. Alpha < Beta < ReleaseCandidate < None.
    [[nodiscard]] static constexpr auto tagRank(const Tag tag) noexcept -> int {
        switch (tag) {
        case Tag::Alpha:            return 0;
        case Tag::Beta:             return 1;
        case Tag::ReleaseCandidate: return 2;
        case Tag::None:             return 3;
        }
        return 3;
    }

    int m_major = 0;
    int m_minor = 0;
    int m_patch = 0;
    Tag m_tag = Tag::None;
    int m_tweak = 0;
};

} // namespace fbide
