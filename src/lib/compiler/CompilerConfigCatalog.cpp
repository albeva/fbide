//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerConfigCatalog.hpp"
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
    // Canonical is the inheritance root — anything missing here is the
    // user's last resort, so it gets the platform default (terminal)
    // or the hardcoded template (compile/run). `get_or` only returns
    // the default when the key is *absent*; an explicit empty value is
    // preserved as an empty override.
    const auto& section = cfg.config().at("compiler");
    const auto defaultName = cfg.locale().at("dialogs.settings.compiler").get_or("defaultName", wxString { "Default" });
    return PendingConfig {
        .slug = kCanonicalCompilerSlug,
        .name = defaultName,
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

/// Merge `child` over `canonical`: every field not explicitly set on
/// the child falls through to canonical. Canonical itself is also
/// passed through this function (with no child overrides) so its
/// resolution stays in one place.
auto resolve(const PendingConfig& child, const PendingConfig& canonical) -> ResolvedCompilerConfig {
    const auto pick = [](const std::optional<wxString>& overrideValue,
                          const std::optional<wxString>& fallback) -> wxString {
        if (overrideValue.has_value()) {
            return *overrideValue;
        }
        return fallback.value_or(wxString {});
    };
    return ResolvedCompilerConfig {
        .slug = child.slug,
        .displayName = child.name,
        .path = toFsPath(pick(child.path, canonical.path)),
        .runCommand = pick(child.runCommand, canonical.runCommand),
        .compileCommand = pick(child.compileCommand, canonical.compileCommand),
        .terminal = pick(child.terminal, canonical.terminal),
    };
}

} // namespace

CompilerConfigCatalog::CompilerConfigCatalog(ConfigManager& cfg)
: m_cfg(cfg) {}

void CompilerConfigCatalog::reload() {
    m_configs.clear();

    const auto canonical = parseCanonicalPending(m_cfg);

    std::vector<PendingConfig> users;
    const auto& compilerSection = m_cfg.config().at("compiler");
    if (compilerSection.isTable()) {
        for (const auto& [key, child] : compilerSection.entries()) {
            if (child->isTable()) {
                users.push_back(parseUserPending(key, *child));
            }
        }
    }
    std::ranges::sort(users, {}, [](const auto& pending) { return slugSortKey(pending.slug); });

    m_configs.reserve(1 + users.size());
    m_configs.push_back(resolve(canonical, canonical));
    for (const auto& user : users) {
        m_configs.push_back(resolve(user, canonical));
    }

    // Resolve `compiler.active` once per reload. find() now sees the
    // freshly-rebuilt cache, and the missing-slug warning fires at most
    // once per catalog mutation instead of on every lookup.
    const auto stored = m_cfg.config().get_or("compiler.active", wxString {});
    if (stored.IsEmpty() || stored == kCanonicalCompilerSlug) {
        m_activeSlug = kCanonicalCompilerSlug;
    } else if (find(stored) != nullptr) {
        m_activeSlug = stored;
    } else {
        wxLogWarning(
            "Active compiler configuration '%s' is not defined; "
            "falling back to canonical.",
            stored
        );
        m_activeSlug = kCanonicalCompilerSlug;
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

auto CompilerConfigCatalog::at(int index) const -> const ResolvedCompilerConfig* {
    if (index < 0 || static_cast<std::size_t>(index) >= m_configs.size()) {
        return nullptr;
    }
    return &m_configs[static_cast<std::size_t>(index)];
}

auto CompilerConfigCatalog::indexOf(const wxString& slug) const -> int {
    const auto it = std::ranges::find(m_configs, slug, &ResolvedCompilerConfig::slug);
    return it == m_configs.end() ? -1 : static_cast<int>(it - m_configs.begin());
}

auto CompilerConfigCatalog::resolveByPinnedSlug(const std::optional<wxString>& pinnedSlug) const -> const ResolvedCompilerConfig& {
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
    // `m_activeSlug` was validated against the catalog during reload(),
    // so it's always findable here — but fall back to canonical anyway
    // in case a caller hits this path before reload().
    if (const auto* cfg = find(m_activeSlug)) {
        return *cfg;
    }
    return canonical();
}

auto CompilerConfigCatalog::normalizeForStorage(const wxString& pickedSlug) const -> std::optional<wxString> {
    if (pickedSlug == activeSlug()) {
        return std::nullopt;
    }
    return pickedSlug;
}

namespace {
/// Read-then-bump the monotonic `nextSlugIndex` counter so two
/// allocations in a row produce distinct slugs even before the next
/// reload. Returns the freshly minted `cfg-N` string.
auto allocateSlug(Value& compiler) -> wxString {
    // nextSlugIndex defaults to 1 — first user config is cfg-1.
    const auto next = compiler.get_or("nextSlugIndex", 1);
    compiler["nextSlugIndex"] = next + 1;
    return wxString::Format("cfg-%d", next);
}
} // namespace

auto CompilerConfigCatalog::createFromCanonical(const wxString& displayName) -> wxString {
    auto& compiler = m_cfg.config()["compiler"];
    const auto slug = allocateSlug(compiler);
    compiler[slug]["name"] = displayName;
    reload();
    return slug;
}

auto CompilerConfigCatalog::copy(const wxString& sourceSlug, const wxString& displayName) -> wxString {
    auto& compiler = m_cfg.config()["compiler"];
    const auto slug = allocateSlug(compiler);

    // Copying canonical = a blank user config (canonical's keys are at
    // [compiler] root, not in a child section). Otherwise clone the
    // source section verbatim — clone() handles every leaf and any
    // future nested tables without us re-listing the known field keys.
    if (sourceSlug != kCanonicalCompilerSlug && compiler.contains(sourceSlug)) {
        compiler[slug] = compiler.at(sourceSlug).clone();
    }
    compiler[slug]["name"] = displayName;
    reload();
    return slug;
}

auto CompilerConfigCatalog::remove(const wxString& slug) -> bool {
    if (slug == kCanonicalCompilerSlug) {
        wxLogWarning("Cannot remove canonical default compiler configuration.");
        return false;
    }
    auto& compiler = m_cfg.config()["compiler"];
    if (!compiler.contains(slug)) {
        return false;
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
    if (!compiler.contains(slug)) {
        return false;
    }
    compiler[slug]["name"] = displayName;
    reload();
    return true;
}

auto CompilerConfigCatalog::setOverride(
    const wxString& slug,
    CompilerField field,
    const std::optional<wxString>& value
) -> bool {
    auto& compiler = m_cfg.config()["compiler"];
    if (slug != kCanonicalCompilerSlug && !compiler.contains(slug)) {
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

auto CompilerConfigCatalog::activeSlug() const -> wxString {
    return m_activeSlug;
}
