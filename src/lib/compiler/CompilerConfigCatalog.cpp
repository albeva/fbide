//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerConfigCatalog.hpp"
#include <unordered_set>
#include "config/ConfigManager.hpp"
#include "config/Value.hpp"

using namespace fbide;

namespace {
/// Per-section snapshot used during `reload()`. Holds each overridable
/// field as an `optional<wxString>` so we can distinguish "key absent
/// (inherit)" from "key present, value empty (override with empty)".
struct PendingConfig {
    wxString slug;
    wxString name;
    wxString base; ///< Empty for canonical; otherwise the parent slug.
    std::optional<wxString> path;
    std::optional<wxString> runCommand;
    std::optional<wxString> compileCommand;
    std::optional<wxString> terminal;
};

/// Convert a `wxString` path to `std::filesystem::path` without round-
/// tripping through the C-locale narrowing that the default ctor would
/// perform. On Windows this preserves non-ASCII paths via UTF-16; on
/// POSIX it preserves UTF-8 bytes verbatim.
auto wxToPath(const wxString& str) -> std::filesystem::path {
#ifdef __WXMSW__
    return std::filesystem::path { str.ToStdWstring() };
#else
    return std::filesystem::path { str.utf8_string() };
#endif
}

/// Pull a leaf value out of a section without applying inheritance.
/// `nullopt` means the key was absent (inherit); a present key — even
/// with an empty value — comes back as a populated optional.
auto readOverride(const Value& section, const wxString& key) -> std::optional<wxString> {
    const auto& leaf = section.at(key);
    if (!leaf) {
        return std::nullopt;
    }
    return leaf.value_or(wxString {});
}

auto parseCanonicalPending(const Value& compilerSection) -> PendingConfig {
    // Canonical has no notion of inheritance — missing and empty are both
    // treated as "this is the value, which happens to be empty".
    return PendingConfig {
        .slug = kCanonicalCompilerSlug,
        .name = "Default",
        .base = wxString {},
        .path = compilerSection.get_or("path", wxString {}),
        .runCommand = compilerSection.get_or("runCommand", wxString {}),
        .compileCommand = compilerSection.get_or("compileCommand", wxString {}),
        .terminal = compilerSection.get_or("terminal", wxString {}),
    };
}

auto parseUserPending(const wxString& slug, const Value& section) -> PendingConfig {
    return PendingConfig {
        .slug = slug,
        .name = section.get_or("name", wxString {}),
        .base = section.get_or("base", wxString {}),
        .path = readOverride(section, "path"),
        .runCommand = readOverride(section, "runCommand"),
        .compileCommand = readOverride(section, "compileCommand"),
        .terminal = readOverride(section, "terminal"),
    };
}

/// Sort key for user slugs — `cfg-N` strings sort by their integer
/// suffix so `cfg-10` comes after `cfg-2`. Slugs that don't fit the
/// pattern fall back to lexicographic order, after all numeric entries.
auto slugSortKey(const wxString& slug) -> std::pair<long, wxString> {
    constexpr auto kPrefix = "cfg-";
    if (slug.StartsWith(kPrefix)) {
        long number = 0;
        if (slug.Mid(std::char_traits<char>::length(kPrefix)).ToLong(&number)) {
            return { number, slug };
        }
    }
    // Push non-conforming slugs after every numeric one — gives a stable
    // tail when a hand-edited config sneaks in.
    return { std::numeric_limits<long>::max(), slug };
}

/// Walk the base chain for `start`, taking each missing field from the
/// first ancestor that supplies it. On a cycle or orphan, the walk
/// breaks and any still-missing fields are filled from the canonical
/// pending entry.
auto resolve(
    const PendingConfig& start,
    const std::unordered_map<wxString, PendingConfig>& byslug
) -> ResolvedCompilerConfig {
    auto path = start.path;
    auto runCommand = start.runCommand;
    auto compileCommand = start.compileCommand;
    auto terminal = start.terminal;

    std::unordered_set<wxString> visited { start.slug };
    wxString current = start.base;
    while (!current.IsEmpty()) {
        if (visited.contains(current)) {
            wxLogWarning(
                "Compiler configuration '%s' has a cyclic base chain at '%s'; "
                "falling back to canonical for unresolved fields.",
                start.slug, current
            );
            break;
        }
        const auto it = byslug.find(current);
        if (it == byslug.end()) {
            wxLogWarning(
                "Compiler configuration '%s' references unknown base '%s'; "
                "falling back to canonical for unresolved fields.",
                start.slug, current
            );
            break;
        }
        visited.insert(current);
        const auto& parent = it->second;
        if (!path) {
            path = parent.path;
        }
        if (!runCommand) {
            runCommand = parent.runCommand;
        }
        if (!compileCommand) {
            compileCommand = parent.compileCommand;
        }
        if (!terminal) {
            terminal = parent.terminal;
        }
        current = parent.base;
    }

    // Canonical fallback for any field still unspecified — covers both
    // the normal "chain terminated cleanly" case (current was empty) and
    // the cycle / orphan break above.
    const auto& canonical = byslug.at(kCanonicalCompilerSlug);
    return ResolvedCompilerConfig {
        .slug = start.slug,
        .displayName = start.name,
        .path = wxToPath(path.value_or(canonical.path.value_or(wxString {}))),
        .runCommand = runCommand.value_or(canonical.runCommand.value_or(wxString {})),
        .compileCommand = compileCommand.value_or(canonical.compileCommand.value_or(wxString {})),
        .terminal = terminal.value_or(canonical.terminal.value_or(wxString {})),
    };
}

} // namespace

CompilerConfigCatalog::CompilerConfigCatalog(ConfigManager& cfg)
: m_cfg(cfg) {}

void CompilerConfigCatalog::reload() {
    m_configs.clear();

    const auto& compilerSection = m_cfg.config().at("compiler");

    // First pass — collect every section's overrides as PendingConfig so
    // the resolution pass can walk base chains without re-reading
    // ConfigManager.
    std::unordered_map<wxString, PendingConfig> pending;
    std::vector<wxString> userSlugs;

    pending.emplace(kCanonicalCompilerSlug, parseCanonicalPending(compilerSection));

    if (compilerSection.isTable()) {
        for (const auto& [key, child] : compilerSection.entries()) {
            if (child->isTable()) {
                pending.emplace(key, parseUserPending(key, *child));
                userSlugs.push_back(key);
            }
        }
    }

    std::ranges::sort(userSlugs, {}, slugSortKey);

    m_configs.reserve(1 + userSlugs.size());
    m_configs.push_back(resolve(pending.at(kCanonicalCompilerSlug), pending));
    for (const auto& slug : userSlugs) {
        m_configs.push_back(resolve(pending.at(slug), pending));
    }
}

auto CompilerConfigCatalog::canonical() const -> const ResolvedCompilerConfig& {
    return m_configs.front();
}

auto CompilerConfigCatalog::find(const wxString& slug) const -> const ResolvedCompilerConfig* {
    for (const auto& cfg : m_configs) {
        if (cfg.slug == slug) {
            return &cfg;
        }
    }
    return nullptr;
}

auto CompilerConfigCatalog::all() const -> std::span<const ResolvedCompilerConfig> {
    return m_configs;
}

auto CompilerConfigCatalog::activeSlug() const -> wxString {
    auto stored = m_cfg.config().get_or("compiler.active", wxString {});
    if (stored.IsEmpty()) {
        return kCanonicalCompilerSlug;
    }
    if (find(stored) == nullptr) {
        wxLogWarning(
            "Active compiler configuration '%s' is not defined; "
            "falling back to canonical.",
            stored
        );
        return kCanonicalCompilerSlug;
    }
    return stored;
}
