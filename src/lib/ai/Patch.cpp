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

/// Find `needle` in `haystack` as raw UTF-8 bytes, returning the byte
/// offset or `std::string::npos`.
auto findUtf8(const std::string_view haystack, const wxString& needle) -> std::size_t {
    const auto needleUtf8 = needle.utf8_string();
    if (needleUtf8.empty()) {
        return std::string::npos;
    }
    return haystack.find(needleUtf8);
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

    if (const auto pos = findUtf8(source, search); pos != std::string::npos) {
        return PatchMatch {
            .offset = static_cast<int>(pos),
            .length = static_cast<int>(search.utf8_string().size()),
            .replacement = replacement,
        };
    }

    // Trailing-newline retry — only when the search has a trailing `\n`
    // to drop. Replacement loses its trailing `\n` only when it also has
    // one (an EOL-less replacement stays as the model wrote it).
    if (!endsWithNewline(search)) {
        return {};
    }
    wxString trimmedSearch = search;
    trimmedSearch.RemoveLast();
    wxString trimmedReplacement = replacement;
    if (endsWithNewline(replacement)) {
        trimmedReplacement.RemoveLast();
    }

    if (const auto pos = findUtf8(source, trimmedSearch); pos != std::string::npos) {
        return PatchMatch {
            .offset = static_cast<int>(pos),
            .length = static_cast<int>(trimmedSearch.utf8_string().size()),
            .replacement = std::move(trimmedReplacement),
        };
    }
    return {};
}
