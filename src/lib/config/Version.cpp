//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Version.hpp"
using namespace fbide;

auto Version::fbide() noexcept -> Version {
    return {
        cmake::project.major,
        cmake::project.minor,
        cmake::project.patch,
        cmake::project.tag,
        cmake::project.tweak,
    };
}

namespace {

/// Map a tag word (case-insensitive) to its enum case. Unknown / empty
/// input returns `Tag::None`, matching the parser's tolerant style.
auto parseTag(const wxString& word) -> Version::Tag {
    if (word.IsSameAs("alpha", false)) {
        return Version::Tag::Alpha;
    }
    if (word.IsSameAs("beta", false)) {
        return Version::Tag::Beta;
    }
    if (word.IsSameAs("rc", false)) {
        return Version::Tag::ReleaseCandidate;
    }
    return Version::Tag::None;
}

} // namespace

Version::Version(const wxString& version) noexcept {
    // Split on '.': head is the numeric triple, optional 4th segment is
    // the tag (with embedded "-tweak"). Anything that fails to parse
    // leaves the field at its default-constructed value.
    const wxArrayString parts = wxSplit(version, '.');
    if (parts.size() > 0) {
        parts[0].ToInt(&m_major);
    }
    if (parts.size() > 1) {
        parts[1].ToInt(&m_minor);
    }
    if (parts.size() > 2) {
        parts[2].ToInt(&m_patch);
    }
    if (parts.size() > 3) {
        const wxString& suffix = parts[3];
        const auto dash = suffix.Find('-');
        wxString tagWord;
        if (dash == wxNOT_FOUND) {
            tagWord = suffix;
        } else {
            tagWord = suffix.Mid(0, static_cast<std::size_t>(dash));
            const wxString tweakWord = suffix.Mid(static_cast<std::size_t>(dash) + 1);
            tweakWord.ToInt(&m_tweak);
        }
        m_tag = parseTag(tagWord);
        if (m_tag == Tag::None) {
            // Unknown tag word — drop the tweak too so the value
            // round-trips to a clean numeric-only string.
            m_tweak = 0;
        }
    }
}

auto Version::tagToString(const Tag tag) -> wxString {
    switch (tag) {
    case Tag::None:             return {};
    case Tag::Alpha:            return "alpha";
    case Tag::Beta:             return "beta";
    case Tag::ReleaseCandidate: return "rc";
    }
    return {};
}

auto Version::asString() const -> wxString {
    if (m_tag == Tag::None) {
        return wxString::Format("%d.%d.%d", m_major, m_minor, m_patch);
    }
    return wxString::Format(
        "%d.%d.%d.%s-%d",
        m_major, m_minor, m_patch,
        tagToString(m_tag),
        m_tweak
    );
}
