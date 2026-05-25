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

/// Split `text` into a vector of lines (without trailing newlines).
/// A trailing `\n` does not produce an empty final element — matches
/// the line semantics callers expect from "patch this many lines".
auto splitLines(std::string_view text) -> std::vector<std::string_view> {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text.at(i) == '\n') {
            out.emplace_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    if (start < text.size()) {
        out.emplace_back(text.substr(start));
    }
    return out;
}

/// Trim leading and trailing ASCII whitespace from `line`.
auto trimWhitespace(std::string_view line) -> std::string_view {
    const auto front = line.find_first_not_of(" \t\r");
    if (front == std::string_view::npos) {
        return {};
    }
    const auto back = line.find_last_not_of(" \t\r");
    return line.substr(front, back - front + 1);
}

/// Walk `source` line by line, looking for a contiguous run whose
/// whitespace-normalised lines match `needleLines` exactly. Returns
/// the byte range covering those source lines on success, or
/// `{-1, 0}` on miss. The range starts at the run's first byte and
/// ends just past the run's last `\n` (inclusive of the terminator,
/// when present), so an exact-replacement still preserves EOL.
struct LineMatch {
    int offset = -1;
    int length = 0;
};
auto findLineRunMatch(std::string_view source, const std::vector<std::string_view>& needleLines) -> LineMatch {
    if (needleLines.empty()) {
        return {};
    }
    const auto sourceLines = splitLines(source);
    if (sourceLines.size() < needleLines.size()) {
        return {};
    }

    // Pre-trim the needle once; the source is trimmed lazily inside
    // the loop so a long source with no candidate prefix bails fast.
    std::vector<std::string_view> needleTrimmed;
    needleTrimmed.reserve(needleLines.size());
    for (const auto& line : needleLines) {
        needleTrimmed.emplace_back(trimWhitespace(line));
    }

    for (std::size_t start = 0; start + needleLines.size() <= sourceLines.size(); ++start) {
        bool match = true;
        for (std::size_t offset = 0; offset < needleLines.size(); ++offset) {
            if (trimWhitespace(sourceLines.at(start + offset)) != needleTrimmed.at(offset)) {
                match = false;
                break;
            }
        }
        if (!match) {
            continue;
        }
        // Compute the byte range — pointer arithmetic into the
        // original source view, since splitLines returned subviews.
        const auto firstLine = sourceLines.at(start);
        const auto* sourceBegin = source.data();
        const auto runStart = static_cast<std::size_t>(firstLine.data() - sourceBegin);
        const auto& lastLine = sourceLines.at(start + needleLines.size() - 1);
        const auto runLast = static_cast<std::size_t>(lastLine.data() - sourceBegin) + lastLine.size();
        // Include the trailing newline when there is one — keeps the
        // EOL discipline intact when the replacement also ends in `\n`.
        const auto withEol = (runLast < source.size() && source.at(runLast) == '\n') ? runLast + 1 : runLast;
        return LineMatch {
            .offset = static_cast<int>(runStart),
            .length = static_cast<int>(withEol - runStart),
        };
    }
    return {};
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
            .kind = MatchKind::Exact,
        };
    }

    // Trailing-newline retry — only when the search has a trailing `\n`
    // to drop. Replacement loses its trailing `\n` only when it also has
    // one (an EOL-less replacement stays as the model wrote it).
    if (!needle.empty() && needle.back() == '\n') {
        auto trimmedNeedle = needle;
        trimmedNeedle.pop_back();
        wxString trimmedReplacement = replacement;
        if (endsWithNewline(replacement)) {
            trimmedReplacement.RemoveLast();
        }
        if (const auto pos = source.find(trimmedNeedle); pos != std::string::npos) {
            return PatchMatch {
                .offset = static_cast<int>(pos),
                .length = static_cast<int>(trimmedNeedle.size()),
                .replacement = std::move(trimmedReplacement),
                .kind = MatchKind::TrimmedNewline,
            };
        }
    }

    // Whitespace-normalised line match — line-by-line comparison with
    // leading/trailing whitespace stripped. Catches the common
    // "indentation drift" misses (model used 4 spaces, file uses 2)
    // and trailing-space drift. Replacement is inserted verbatim;
    // upstream callers see `MatchKind::NormalizedWhitespace` and can
    // hint the model to be more careful about indentation next time.
    const auto needleLines = splitLines(needle);
    const auto lineMatch = findLineRunMatch(source, needleLines);
    if (lineMatch.offset >= 0) {
        return PatchMatch {
            .offset = lineMatch.offset,
            .length = lineMatch.length,
            .replacement = replacement,
            .kind = MatchKind::NormalizedWhitespace,
        };
    }
    return {};
}
