//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Patch.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {

/// True when `text` ends with a `\n`.
auto endsWithNewline(const wxString& text) -> bool {
    return text.EndsWith("\n");
}

} // namespace

auto fbide::ai::findPatchMatch(
    const std::string_view source,
    const wxString& search,
    const wxString& replacement
) -> PatchMatch {
    if (search.empty()) {
        return {};
    }

    // Convert the search text to UTF-8 once and work in std::string land
    // for the rest of the function. The trailing-newline retry then
    // becomes a `pop_back()` on the byte buffer rather than a fresh
    // wxString-to-UTF-8 round trip — the original code paid up to four
    // `utf8_string()` calls (find + length, twice).
    auto needle = search.utf8_string();

    if (const auto pos = source.find(needle); pos != std::string::npos) {
        return PatchMatch {
            .offset = static_cast<int>(pos),
            .length = static_cast<int>(needle.size()),
            .replacement = replacement,
        };
    }

    // Trailing-newline retry — only when the search has a trailing `\n`
    // to drop. Replacement loses its trailing `\n` only when it also has
    // one (an EOL-less replacement stays as the model wrote it).
    if (needle.empty() || needle.back() != '\n') {
        return {};
    }
    needle.pop_back();

    wxString trimmedReplacement = replacement;
    if (endsWithNewline(replacement)) {
        trimmedReplacement.RemoveLast();
    }

    if (const auto pos = source.find(needle); pos != std::string::npos) {
        return PatchMatch {
            .offset = static_cast<int>(pos),
            .length = static_cast<int>(needle.size()),
            .replacement = std::move(trimmedReplacement),
        };
    }
    return {};
}
