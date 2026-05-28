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
#include "utils/PathConversions.hpp"

using namespace fbide;

namespace {
/// Fallback templates applied to canonical fields when the corresponding
/// key is missing from `[compiler]`. Shipped config files set these
/// explicitly per platform; the constants are a safety net for hand-
/// edited or corrupt configs. An EMPTY value in the config is *not*
/// replaced — that's a deliberate override (e.g. "no terminal").
constexpr auto kDefaultCompileTemplate = R"("<$fbc>" "<$file>")";
constexpr auto kDefaultRunTemplate = R"(<$terminal> "<$file>" <$param>)";

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

auto parseCanonicalPending(ConfigManager& cfg) -> PendingConfig {
    // Canonical is the bottom of the inheritance chain — anything missing
    // here is the user's last resort, so it gets the platform default
    // (terminal) or the hardcoded template (compile/run). `get_or` only
    // returns the default when the key is *absent*; an explicit empty
    // value is preserved as an empty override.
    const auto& section = cfg.config().at("compiler");
    return PendingConfig {
        .slug = kCanonicalCompilerSlug,
        .name = "Default",
        .base = wxString {},
        .path = section.get_or("path", wxString {}),
        .runCommand = section.get_or("runCommand", wxString { kDefaultRunTemplate }),
        .compileCommand = section.get_or("compileCommand", wxString { kDefaultCompileTemplate }),
        .terminal = cfg.getTerminalLauncher(),
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
        .path = toFsPath(path.value_or(canonical.path.value_or(wxString {}))),
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

    pending.emplace(kCanonicalCompilerSlug, parseCanonicalPending(m_cfg));

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

auto CompilerConfigCatalog::resolveByPinnedSlug(const std::optional<wxString>& pinnedSlug) const
    -> const ResolvedCompilerConfig& {
    if (pinnedSlug.has_value()) {
        if (const auto* cfg = find(*pinnedSlug)) {
            return *cfg;
        }
        wxLogWarning(
            "Document pinned to compiler configuration '%s', which is not defined; "
            "using active configuration.",
            *pinnedSlug
        );
    }
    if (const auto* cfg = find(activeSlug())) {
        return *cfg;
    }
    // activeSlug() already guarantees a canonical fallback, but be
    // defensive — find on a freshly-reloaded catalog never misses
    // "default", so this is unreachable in practice.
    return canonical();
}

auto CompilerConfigCatalog::normalizeForStorage(const wxString& pickedSlug) const
    -> std::optional<wxString> {
    if (pickedSlug == activeSlug()) {
        return std::nullopt;
    }
    return pickedSlug;
}

namespace {
auto compilerFieldKey(CompilerField field) -> wxString {
    switch (field) {
    case CompilerField::Path:
        return "path";
    case CompilerField::CompileCommand:
        return "compileCommand";
    case CompilerField::RunCommand:
        return "runCommand";
    case CompilerField::Terminal:
        return "terminal";
    }
    return wxString {};
}
} // namespace

auto CompilerConfigCatalog::createFromCanonical(const wxString& displayName) -> wxString {
    auto& compiler = m_cfg.config()["compiler"];
    // nextSlugIndex defaults to 1 — first user config is cfg-1.
    auto next = compiler.get_or("nextSlugIndex", 1);
    auto slug = wxString::Format("cfg-%d", next);
    compiler["nextSlugIndex"] = next + 1;
    compiler[slug]["name"] = displayName;
    reload();
    return slug;
}

auto CompilerConfigCatalog::copy(const wxString& sourceSlug, const wxString& displayName) -> wxString {
    auto& compiler = m_cfg.config()["compiler"];
    auto next = compiler.get_or("nextSlugIndex", 1);
    auto slug = wxString::Format("cfg-%d", next);
    compiler["nextSlugIndex"] = next + 1;

    auto& dest = compiler[slug];
    dest["name"] = displayName;
    if (sourceSlug != kCanonicalCompilerSlug) {
        const auto& source = compiler.at(sourceSlug);
        if (source.isTable()) {
            for (const auto& [key, child] : source.entries()) {
                if (key == "name") {
                    continue; // displayName already written
                }
                if (child->isString()) {
                    dest[key] = child->value_or(wxString {});
                }
            }
        }
    }
    reload();
    return slug;
}

auto CompilerConfigCatalog::remove(const wxString& slug) -> bool {
    if (slug == kCanonicalCompilerSlug) {
        wxLogWarning("Cannot remove canonical default compiler configuration.");
        return false;
    }
    auto& compiler = m_cfg.config()["compiler"];
    if (!static_cast<bool>(compiler.at(slug))) {
        return false;
    }
    // Re-parent any user config whose `base=` was the removed slug.
    // Walk the in-memory entries directly; this catches every child
    // before we erase the target.
    if (compiler.isTable()) {
        for (const auto& [otherSlug, child] : compiler.entries()) {
            if (otherSlug == slug || !child->isTable()) {
                continue;
            }
            const auto baseValue = child->get_or("base", wxString {});
            if (baseValue == slug) {
                compiler[otherSlug].erase("base");
            }
        }
    }
    if (compiler.get_or("active", wxString {}) == slug) {
        compiler.erase("active");
    }
    compiler.erase(slug);
    reload();
    return true;
}

auto CompilerConfigCatalog::rename(const wxString& slug, const wxString& displayName) -> bool {
    if (slug == kCanonicalCompilerSlug) {
        return false;
    }
    auto& compiler = m_cfg.config()["compiler"];
    if (!static_cast<bool>(compiler.at(slug))) {
        return false;
    }
    compiler[slug]["name"] = displayName;
    reload();
    return true;
}

auto CompilerConfigCatalog::setBase(const wxString& slug, const wxString& newBaseSlug) -> bool {
    if (slug == kCanonicalCompilerSlug) {
        wxLogWarning("Cannot set a base for the canonical default configuration.");
        return false;
    }
    if (slug == newBaseSlug) {
        wxLogWarning("Refusing to set '%s' as its own base.", slug);
        return false;
    }
    // Reject if newBaseSlug is a descendant of slug — would create a cycle.
    const auto bases = validBasesFor(slug);
    if (newBaseSlug != kCanonicalCompilerSlug
        && std::ranges::find(bases, newBaseSlug) == bases.end()) {
        wxLogWarning("Refusing to set '%s' as base of '%s' — would create a cycle.", newBaseSlug, slug);
        return false;
    }
    auto& compiler = m_cfg.config()["compiler"];
    if (newBaseSlug == kCanonicalCompilerSlug || newBaseSlug.IsEmpty()) {
        compiler[slug].erase("base");
    } else {
        compiler[slug]["base"] = newBaseSlug;
    }
    reload();
    return true;
}

auto CompilerConfigCatalog::setOverride(
    const wxString& slug,
    CompilerField field,
    const std::optional<wxString>& value
) -> bool {
    auto& compiler = m_cfg.config()["compiler"];
    if (slug != kCanonicalCompilerSlug && !static_cast<bool>(compiler.at(slug))) {
        return false;
    }
    auto& section = (slug == kCanonicalCompilerSlug) ? compiler : compiler[slug];
    const auto key = compilerFieldKey(field);
    if (!value.has_value()) {
        section.erase(key);
    } else {
        section[key] = *value;
    }
    reload();
    return true;
}

void CompilerConfigCatalog::setActiveSlug(const wxString& slug) {
    auto& compiler = m_cfg.config()["compiler"];
    if (slug == kCanonicalCompilerSlug || slug.IsEmpty()) {
        compiler.erase("active");
    } else {
        compiler["active"] = slug;
    }
    reload();
}

auto CompilerConfigCatalog::validBasesFor(const wxString& slug) const -> std::vector<wxString> {
    // A slug's valid bases are every catalog slug except the slug
    // itself and every descendant (transitive child) of it. Canonical
    // default is always a valid base.
    std::unordered_set<wxString> excluded { slug };

    // Build a child-of map: parent slug → list of children.
    std::unordered_map<wxString, std::vector<wxString>> childrenOf;
    for (const auto& cfg : m_configs) {
        if (cfg.slug == kCanonicalCompilerSlug) {
            continue;
        }
        const auto baseValue = m_cfg.config().at("compiler").at(cfg.slug).get_or("base", wxString {});
        const auto parent = baseValue.IsEmpty() ? wxString { kCanonicalCompilerSlug } : baseValue;
        childrenOf[parent].push_back(cfg.slug);
    }

    // BFS from slug downward to mark all descendants.
    std::vector<wxString> queue { slug };
    while (!queue.empty()) {
        auto current = queue.back();
        queue.pop_back();
        auto it = childrenOf.find(current);
        if (it == childrenOf.end()) {
            continue;
        }
        for (const auto& child : it->second) {
            if (excluded.insert(child).second) {
                queue.push_back(child);
            }
        }
    }

    std::vector<wxString> out;
    out.reserve(m_configs.size());
    for (const auto& cfg : m_configs) {
        if (!excluded.contains(cfg.slug)) {
            out.push_back(cfg.slug);
        }
    }
    return out;
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
