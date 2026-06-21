//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ConfigManager.hpp"
#include "utils/PathConversions.hpp"
using namespace fbide;
namespace fs = std::filesystem;

namespace {
// ---------------------------------------------------------------------------
// wxString <-> std::filesystem::path conversion
//
// Everything inside this TU works in `fs::path`. Conversions to/from
// `wxString` happen only at the public API boundary and at wx interop
// points (wxFFile streams, wxLog formatting) via `toFsPath` / `toWxString`
// from `utils/PathConversions.hpp`.
// ---------------------------------------------------------------------------

void dismissSplash() {
    auto node = wxTopLevelWindows.GetFirst();
    while (node) {
        const auto next = node->GetNext();
        if (auto* splash = wxDynamicCast(node->GetData(), wxSplashScreen)) {
            splash->Hide();
            splash->Destroy();
        }
        node = next;
    }
    wxYield();
}

/// Show a fatal-error dialog in plain English and terminate. Used for
/// missing files we cannot meaningfully run without (config / layout /
/// locale) — locale strings aren't available at this point, so the
/// message is hard-coded.
[[noreturn]] void fatalAndExit(const wxString& message) {
    dismissSplash();
    wxMessageBox(message, "FBIde - fatal error", wxOK | wxICON_ERROR);
    std::exit(1);
}
} // namespace

namespace {
/// Sentinel filename. When present in the IDE resources directory, that
/// directory is treated as read-only (e.g. inside a macOS .app bundle,
/// AppImage, or signed Windows install) and writable artefacts —
/// per-category `.local.ini` overlays and theme copies — are routed to
/// `wxStandardPaths::GetUserDataDir()` instead of next to the bundle.
constexpr auto kReadOnlySentinel = "READONLY";

/// True when the IDE resources directory carries the read-only sentinel.
[[nodiscard]] auto hasReadOnlySentinel(const fs::path& dir) -> bool {
    std::error_code ec;
    return fs::exists(dir / kReadOnlySentinel, ec);
}

/// True when `lhs` and `rhs` refer to the same filesystem entry.
/// Follows symlinks on both sides via `std::filesystem::equivalent`, so
/// editing a config file through a symlink still matches the loaded
/// canonical path.
[[nodiscard]] auto samePath(const fs::path& lhs, const fs::path& rhs) -> bool {
    if (lhs.empty() || rhs.empty()) {
        return false;
    }
    std::error_code ec;
    return fs::equivalent(lhs, rhs, ec);
}
} // namespace

// ---------------------------------------------------------------------------
// INI <-> Value tree
// ---------------------------------------------------------------------------
namespace {

/// Recursively copy a wxFileConfig subtree into a Value node. `cfg` is
/// already positioned at the group to import.
void importGroup(wxFileConfig& cfg, Value& node) {
    // Entries (leaves) — GetFirst/NextEntry preserve INI order.
    wxString entryName;
    long entryCookie = 0;
    auto hasEntry = cfg.GetFirstEntry(entryName, entryCookie);
    while (hasEntry) {
        wxString leaf;
        cfg.Read(entryName, &leaf);
        node[entryName] = leaf;
        hasEntry = cfg.GetNextEntry(entryName, entryCookie);
    }

    // Groups — recurse.
    wxString groupName;
    long groupCookie = 0;
    auto hasGroup = cfg.GetFirstGroup(groupName, groupCookie);
    while (hasGroup) {
        const auto oldPath = cfg.GetPath();
        cfg.SetPath(groupName);
        importGroup(cfg, node[groupName]);
        cfg.SetPath(oldPath);
        hasGroup = cfg.GetNextGroup(groupName, groupCookie);
    }
}

/// Walk a Value subtree and write every leaf into a wxFileConfig under
/// the given path prefix. `cfg` is positioned at the root on entry.
void exportGroup(const Value& node, const wxString& path, wxFileConfig& cfg) {
    for (const auto& [key, child] : node.entries()) {
        const auto subPath = path.empty() ? key : (path + "/" + key);
        if (child->isTable()) {
            exportGroup(*child, subPath, cfg);
        } else {
            const auto leaf = child->as<wxString>().value_or("");
            cfg.Write(subPath, leaf);
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Get info
// ---------------------------------------------------------------------------

namespace {
/// Enumerate regular files directly under `base` whose extension is in
/// `exts` (with leading dot, e.g. `".ini"`). Returns absolute paths
/// sorted by full path. Symlinks are followed via `directory_iterator`
/// defaults, matching the symlink-aware semantics elsewhere in this TU.
[[nodiscard]] auto enumerate(const fs::path& base, std::initializer_list<std::string_view> exts = { ".ini" }) -> std::vector<fs::path> {
    std::vector<fs::path> files;
    std::error_code ec;
    const fs::directory_iterator it { base, ec };
    if (ec) {
        return files;
    }
    for (const auto& entry : it) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        const auto ext = entry.path().extension().string();
        for (const auto sv : exts) {
            if (ext == sv) {
                files.emplace_back(entry.path());
                break;
            }
        }
    }
    std::ranges::sort(files);
    return files;
}
} // namespace

auto ConfigManager::getAllLanguages() const -> std::vector<wxString> {
    const auto paths = enumerate(m_ideDir / "locales");
    std::vector<wxString> out;
    out.reserve(paths.size());
    for (const auto& path : paths) {
        out.push_back(toWxString(path));
    }
    return out;
}

auto ConfigManager::getAllThemes() const -> std::vector<wxString> {
    auto bundle = enumerate(m_ideDir / "themes", { ".ini", ".fbt" });
    if (!m_readOnlyIde) {
        std::vector<wxString> out;
        out.reserve(bundle.size());
        for (const auto& path : bundle) {
            out.push_back(toWxString(path));
        }
        return out;
    }
    auto user = enumerate(m_userDataDir / "themes", { ".ini", ".fbt" });

    // Two-dir merge — user entries shadow bundle entries on basename
    // collision. `std::unordered_map` because the pch ships unordered
    // but not ordered map; sort by basename in a separate pass.
    std::unordered_map<std::string, fs::path> byName;
    for (auto& path : bundle) {
        byName[path.filename().string()] = std::move(path);
    }
    for (auto& path : user) {
        byName[path.filename().string()] = std::move(path);
    }
    std::vector<fs::path> merged;
    merged.reserve(byName.size());
    for (auto& value : std::views::values(byName)) {
        merged.push_back(std::move(value));
    }
    std::ranges::sort(merged, [](const fs::path& lhs, const fs::path& rhs) {
        return lhs.filename() < rhs.filename();
    });
    std::vector<wxString> out;
    out.reserve(merged.size());
    for (const auto& path : merged) {
        out.push_back(toWxString(path));
    }
    return out;
}

auto ConfigManager::themesWriteDir() const -> wxString {
    const auto dir = (m_readOnlyIde ? m_userDataDir : m_ideDir) / "themes";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return toWxString(dir);
}

auto ConfigManager::historyPath() const -> fs::path {
    const auto& dir = m_readOnlyIde ? m_userDataDir : m_ideDir;
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir / "history.local.ini";
}

auto ConfigManager::themePath(const wxString& relPath) const -> wxString {
    if (relPath.empty()) {
        return relPath;
    }
    const auto rel = toFsPath(relPath);
    if (rel.is_absolute()) {
        return toWxString(rel.lexically_normal());
    }
    // READONLY: user override at `<UserDataDir>/<rel>` wins. Probed
    // before the generic `absolute()` walk so a same-named bundle theme
    // never shadows the user's edit.
    if (m_readOnlyIde) {
        std::error_code ec;
        const auto candidate = (m_userDataDir / rel).lexically_normal();
        if (fs::exists(candidate, ec)) {
            return toWxString(candidate);
        }
    }
    return toWxString(absolutePath(rel));
}

auto ConfigManager::getPlatformConfigFileName() -> wxString {
#ifdef __WXMSW__
    return "config_win.ini";
#elifdef __WXOSX__
    return "config_macos.ini";
#else
    return "config_linux.ini";
#endif
}

#if !defined(__WXMSW__) && !defined(__WXOSX__)
namespace {
/// A Linux terminal emulator and the flag that makes it run a command line
/// (empty when the command is passed positionally, e.g. kitty/foot).
struct TerminalSpec {
    const char* binary;
    const char* execFlag;
};

/// Candidates probed in PATH, in priority order: the distro-neutral alternatives
/// symlink first (honours the user's chosen default on Debian/Ubuntu), then the
/// common desktop and standalone emulators.
constexpr TerminalSpec kLinuxTerminals[] = {
    { "x-terminal-emulator", "-e" }, // Debian/Ubuntu alternatives symlink
    { "gnome-terminal", "--" },      // -e is deprecated; -- passes the command
    { "konsole", "-e" },
    { "xfce4-terminal", "-x" },
    { "tilix", "-e" },
    { "ptyxis", "--" },
    { "kitty", "" },
    { "alacritty", "-e" },
    { "wezterm", "-e" },
    { "foot", "" },
    { "xterm", "-e" },
};

auto isExecutableInPath(const wxString& name) -> bool {
    wxString pathEnv;
    if (!wxGetEnv("PATH", &pathEnv)) {
        return false;
    }
    wxStringTokenizer tok(pathEnv, wxString(wxPATH_SEP));
    while (tok.HasMoreTokens()) {
        const wxFileName candidate(tok.GetNextToken(), name);
        if (candidate.FileExists() && candidate.IsFileExecutable()) {
            return true;
        }
    }
    return false;
}

/// First installed terminal, resolved once. Falls back to the first entry when
/// none are found, so the behaviour matches the old hard-coded default.
auto detectLinuxTerminal() -> const TerminalSpec& {
    static const TerminalSpec& spec = [] -> const TerminalSpec& {
        for (const auto& candidate : kLinuxTerminals) {
            if (isExecutableInPath(candidate.binary)) {
                return candidate;
            }
        }
        return kLinuxTerminals[0];
    }();
    return spec;
}
} // namespace
#endif

auto ConfigManager::getTerminal() -> wxString {
#ifdef __WXMSW__
    return "cmd.exe";
#elifdef __WXOSX__
    return "open -a Terminal";
#else
    return detectLinuxTerminal().binary;
#endif
}

auto ConfigManager::getTerminalLauncher() -> wxString {
    return config().get_or("compiler.terminal", getDefaultTerminalLauncher());
}

auto ConfigManager::getLocale() -> wxString {
    const auto name = config().get_or("locale", "");
    if (name.empty()) {
        return wxEmptyString;
    }
    const wxFileName file { name };
    return file.GetName();
}

auto ConfigManager::getDefaultTerminalLauncher() -> wxString {
#ifdef __WXMSW__
    // `cmd /C` runs the program in a new console window allocated by
    // Windows. Console closes when the program exits — add `& pause` or
    // a SLEEP at the end of your program if you need to inspect output.
    // Keeping cmd in the foreground (no `start`) means kill / Stop
    // cascades through cmd's process group to the child program.
    return "cmd /C";
#elifdef __WXOSX__
    // TODO: Terminal.app does not accept the program as a CLI argument;
    // launching requires AppleScript via `osascript` or a temp `.command`
    // script. Cannot be expressed as a single-line template prefix.
    return "";
#else
    // Probe PATH for an installed emulator and pair it with the flag that runs a
    // command (gnome-terminal `--`, konsole `-e`, kitty/foot positional, …). A
    // distro-specific terminal not on the list can still be set via a custom
    // run-command template.
    const auto& term = detectLinuxTerminal();
    wxString launcher = term.binary;
    if (term.execFlag[0] != '\0') {
        launcher += ' ';
        launcher += term.execFlag;
    }
    return launcher;
#endif
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

ConfigManager::ConfigManager(
    const wxString& appPath,
    const wxString& idePath,
    const wxString& configPath,
    const wxString& userDataDirOverride
)
: m_appDir(toFsPath(appPath))
, m_userDataDir(toFsPath(userDataDirOverride.empty() ? wxStandardPaths::Get().GetUserDataDir() : userDataDirOverride)) {
    // Normalise trailing-separator forms so `.filename()` returns the
    // last segment (needed by the macOS bundle / AppImage detection
    // below). fs::path treats `/foo/bar/` and `/foo/bar` differently —
    // the former has an empty filename component.
    if (!m_appDir.empty() && !m_appDir.has_filename()) {
        m_appDir = m_appDir.parent_path();
    }

    std::error_code ec;
    if (!fs::is_directory(m_appDir, ec)) {
        wxLogError("app directory '%s' does not exist", appPath);
        return;
    }

    if (!idePath.empty()) {
        const auto path = absolutePath(toFsPath(idePath));
        if (fs::is_directory(path, ec)) {
            m_ideDir = path;
        } else {
            wxLogWarning("ide config directory '%s' does not exist", toWxString(path));
        }
    }
    if (m_ideDir.empty()) {
#ifdef __WXOSX__
        // Prefer bundle Resources when running inside a macOS .app
        if (m_appDir.filename() == "MacOS") {
            const auto bundleIde = m_appDir.parent_path() / "Resources" / "ide";
            if (fs::is_directory(bundleIde, ec)) {
                m_ideDir = bundleIde;
            } else {
                m_ideDir = m_appDir / "ide";
            }
        } else {
            // Not in a bundle (e.g., running from build dir)
            m_ideDir = m_appDir / "ide";
        }
#elifdef FBIDE_APPIMAGE_BUILD
        // FHS layout used by AppImage / future deb / rpm packages: the
        // binary lives at <prefix>/bin/fbide and resources at
        // <prefix>/share/fbide/ide. Walk one directory up from the
        // binary location and look for share/fbide/ide; fall back to
        // the portable side-by-side layout if that directory is absent
        // (e.g. running from a build tree before `cmake --install`).
        if (m_appDir.filename() == "bin") {
            const auto fhsIde = m_appDir.parent_path() / "share" / "fbide" / "ide";
            if (fs::is_directory(fhsIde, ec)) {
                m_ideDir = fhsIde;
            } else {
                m_ideDir = m_appDir / "ide";
            }
        } else {
            m_ideDir = m_appDir / "ide";
        }
#else
        m_ideDir = m_appDir / "ide";
#endif
    }
    if (!fs::is_directory(m_ideDir, ec)) {
        wxLogError("ide config directory '%s' does not exist", toWxString(m_ideDir));
        return;
    }

    // READONLY sentinel + explicit-config flag drive the strategy rules:
    // - sentinel present → writable artefacts (`.local.ini` overlays,
    //   theme copies) route to `m_userDataDir` rather than next to the
    //   bundle. The sentinel describes the dir, not the boot mode, so
    //   `--ide=PATH` is respected and the dir's own sentinel decides.
    // - `--config=PATH` → all four mutable categories run `Direct` (no
    //   overlay) so CI / repro runs aren't influenced by stray overlays.
    m_explicitConfig = !configPath.empty();
    m_readOnlyIde = hasReadOnlySentinel(m_ideDir);
    if (m_readOnlyIde) {
        wxLogVerbose("READONLY sentinel detected in '%s' — overlays route to '%s'", toWxString(m_ideDir), toWxString(m_userDataDir));
    }

    auto& entry = m_categories.at(static_cast<std::size_t>(Category::Config));
    entry.strategy = buildStrategy(
        Category::Config,
        absolutePath(toFsPath(configPath.empty() ? getPlatformConfigFileName() : configPath))
    );
    wxLogVerbose("ide directory: %s", toWxString(m_ideDir));
    load(Category::Config);

    // Load all configs
    for (const auto cat : { Category::Locale, Category::Shortcuts, Category::Keywords, Category::Layout }) {
        load(cat);
    }

    // Resolve + load theme immediately after config is available. If the
    // configured theme file is missing or absent, fall back to a built-in
    // minimal theme so the editor still launches with a usable scheme.
    if (const auto themeRel = config().get_or("theme", wxString {}); !themeRel.empty()) {
        const auto themeAbs = themePath(themeRel);
        if (wxFileExists(themeAbs)) {
            m_theme.load(themeAbs);
            wxLogVerbose("Loaded theme from %s", themeAbs);
        } else {
            wxLogError("Theme file '%s' not found — using built-in default", themeAbs);
            m_theme.loadDefaults();
        }
    } else {
        wxLogWarning("No 'theme' entry found in config '%s' — using built-in default", toWxString(entry.strategy.basePath()));
        m_theme.loadDefaults();
    }
}

void ConfigManager::setCategoryPath(const Category category, const wxString& path) {
    if (category == Category::Config) {
        wxLogError("Trying to set category path '%s' for root config.", path);
        return;
    }

    const auto key = getCategoryName(category);
    config()[wxString { key.data(), key.size() }] = relative(path);

    load(category);
}

void ConfigManager::reloadConfig(const wxString& configPath) {
    // Runtime equivalent of `--config=PATH` — flip to explicit mode so
    // subsequent strategy rebuilds (sub-categories, reloadIfKnown) stay
    // `Direct`. Sub-categories already loaded under prior mode are
    // unaffected here; full sub-category rebind is task #13 territory.
    m_explicitConfig = true;
    auto& entry = m_categories.at(static_cast<std::size_t>(Category::Config));
    entry.strategy = buildStrategy(Category::Config, absolutePath(toFsPath(configPath)));
    load(Category::Config);

    // Cascade — sub-categories were loaded under the prior mode (likely
    // Overlay). After flipping to explicit they need fresh strategies
    // so subsequent saves honour the Direct rule. load() rebuilds the
    // strategy via buildStrategy() using the new m_explicitConfig.
    for (const auto cat : { Category::Locale, Category::Shortcuts, Category::Keywords, Category::Layout }) {
        load(cat);
    }

    if (const auto themeRel = config().get_or("theme", wxString {}); !themeRel.empty()) {
        const auto themeAbs = themePath(themeRel);
        if (wxFileExists(themeAbs)) {
            m_theme.load(themeAbs);
        } else {
            wxLogError("Theme file '%s' not found — using built-in default", themeAbs);
            m_theme.loadDefaults();
        }
    } else {
        m_theme.loadDefaults();
    }
}

// ---------------------------------------------------------------------------
// Strategy
// ---------------------------------------------------------------------------

auto ConfigManager::buildStrategy(const Category category, const fs::path& basePath) const -> ConfigStrategy {
    // Locale is the only bundle-only mutable slot (custom locales come
    // from a user-set `locale=<path>` in the config overlay, not from an
    // overlay of the locale file itself). Everything else is overlay-
    // capable; the explicit-config flag still forces `Direct`.
    const bool overlayCapable = category != Category::Locale;
    return ConfigStrategy::select(basePath, m_userDataDir, m_readOnlyIde, overlayCapable, m_explicitConfig);
}

// ---------------------------------------------------------------------------
// Load / save
// ---------------------------------------------------------------------------

void ConfigManager::load(const Category category) {
    auto& entry = m_categories.at(static_cast<std::size_t>(category));
    const auto catView = getCategoryName(category);
    const wxString catName { catView.data(), catView.size() };
    fs::path file;

    if (category == Category::Config) {
        // Strategy already built by ctor / reloadConfig before they
        // called us — config is the bootstrap, its path is known up
        // front. Sub-categories below get strategy built right after
        // path resolution.
        file = entry.strategy.basePath();
    } else {
        const auto& ref = config().at(catName);
        const auto relPath = ref.as<wxString>();
        if (!relPath.has_value() || relPath->empty()) {
            wxLogError("Config category '%s' missing or invalid", catName);
            return;
        }
        file = absolutePath(toFsPath(*relPath));
        entry.strategy = buildStrategy(category, file);
    }

    std::error_code ec;
    if (!fs::exists(file, ec)) {
        // Per-category recovery for missing files. Config + Layout are
        // load-bearing for the rest of the IDE, so a miss is fatal —
        // every other path falls back to a workable empty / default
        // state and lets the app keep going.
        wxLogError(
            "Config file '%s' for '%s' category not found",
            toWxString(file), catName
        );
        switch (category) {
        case Category::Config:
            fatalAndExit(
                wxString::Format(
                    "FBIde could not start: the main configuration file was not found.\n\n"
                    "Expected at:\n%s\n\n"
                    "Reinstall FBIde or supply --config=<path>.",
                    toWxString(file)
                )
            );
        case Category::Layout:
            fatalAndExit(
                wxString::Format(
                    "FBIde could not start: the layout file was not found.\n\n"
                    "Expected at:\n%s\n\n"
                    "Reinstall FBIde or restore the layout.ini next to the IDE resources.",
                    toWxString(file)
                )
            );
        case Category::Locale:
            // Locale is also load-bearing — every dialog string flows
            // through it. Without a locale file we'd render a broken UI
            // with no translations available; treat as fatal and surface
            // a hard-coded English message.
            fatalAndExit(
                wxString::Format(
                    "FBIde could not start: the locale file was not found.\n\n"
                    "Expected at:\n%s\n\n"
                    "Reinstall FBIde or restore the locales directory next to the IDE resources.",
                    toWxString(file)
                )
            );
        case Category::Keywords:
        case Category::Shortcuts:
            entry.category = category;
            entry.baseline = Value {};
            entry.root = Value {};
            return;
        }
        return;
    }

    const auto fileWx = toWxString(file);
    wxFFileInputStream stream(fileWx);
    if (!stream.IsOk()) {
        wxLogError("Failed to open '%s' for reading", fileWx);
        return;
    }

    wxFileConfig cfg(stream, wxConvUTF8);

    // Parse twice — baseline is the pristine bundle tree (needed at save
    // time to diff against). Root starts as a second independent parse
    // because Value is move-only by design (`Value.hpp:48-49`); the
    // overlay merge below mutates root without touching baseline.
    Value baseline;
    cfg.SetPath("/");
    importGroup(cfg, baseline);

    Value root;
    cfg.SetPath("/");
    importGroup(cfg, root);

    // Overlay merge — only when the strategy provides an overlay path
    // and that file actually exists. Missing overlay is fine; it just
    // means the user has no divergences from bundle yet.
    if (entry.strategy.usesOverlay() && fs::exists(entry.strategy.overlayPath(), ec)) {
        const auto overlayWx = toWxString(entry.strategy.overlayPath());
        wxFFileInputStream overlayStream(overlayWx);
        if (overlayStream.IsOk()) {
            wxFileConfig overlayCfg(overlayStream, wxConvUTF8);
            Value overlay;
            overlayCfg.SetPath("/");
            importGroup(overlayCfg, overlay);
            root.mergeFrom(overlay);
            wxLogVerbose("Merged overlay %s into %s", overlayWx, catName);
        } else {
            wxLogWarning("Overlay '%s' exists but could not be read", overlayWx);
        }
    }

    entry.category = category;
    entry.baseline = std::move(baseline);
    entry.root = std::move(root);
    wxLogVerbose("Loaded %s from %s", catName, fileWx);
}

void ConfigManager::save(const Category category) const {
    // Locale is bundle-only. The IDE never writes its translation strings
    // back; any caller asking for `save(Locale)` is a bug — flag loudly
    // rather than silently truncating the bundle locale file under
    // portable mode (or no-op'ing under READONLY where the write would
    // fail anyway).
    if (category == Category::Locale) {
        wxLogError("save(Locale) refused — locale files are bundle-only");
        return;
    }

    const auto& entry = m_categories.at(static_cast<std::size_t>(category));
    if (entry.category != category) {
        const auto catView = getCategoryName(category);
        const wxString catName { catView.data(), catView.size() };
        wxLogWarning("Trying to save unloaded category '%s'", catName);
        return;
    }

    if (entry.strategy.usesOverlay()) {
        // Aggressive prune: persist only leaves that diverge from the
        // bundle baseline. Empty diff → no overlay file at all (delete
        // if one existed). Overlay file is machine-managed; we write
        // fresh from the diff rather than merging into existing keys so
        // stale entries from prior saves don't accumulate.
        const auto& overlayPath = entry.strategy.overlayPath();
        const auto overlayWx = toWxString(overlayPath);
        std::error_code ec;
        const auto diff = entry.root.diffAgainst(entry.baseline);
        if (!diff) {
            if (fs::exists(overlayPath, ec)) {
                fs::remove(overlayPath, ec);
                wxLogVerbose("Pruned empty overlay '%s'", overlayWx);
            }
            return;
        }
        // Ensure parent dir exists — under READONLY this is
        // `<UserDataDir>` which may not exist on first launch.
        fs::create_directories(overlayPath.parent_path(), ec);
        wxFileConfig overlayCfg(wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString, 0);
        wxFFileOutputStream outStream(overlayWx);
        if (!outStream.IsOk()) {
            wxLogError("Failed to open '%s' for writing", overlayWx);
            return;
        }
        exportGroup(diff, "", overlayCfg);
        overlayCfg.Save(outStream, wxConvUTF8);
        wxLogVerbose("Saved overlay '%s'", overlayWx);
        return;
    }

    // Direct mode (`--config=PATH` or locale). Open the read stream
    // before the write stream: `wxFileConfig` parses existingStream in
    // its constructor (preserves comments + ordering), then we truncate
    // the file for writing. Reversing the order would zero the file
    // before parsing completed.
    const auto savePathWx = toWxString(entry.strategy.savePath());
    wxFileInputStream existingStream(savePathWx);
    wxFileConfig cfg(existingStream, wxConvUTF8);
    wxFFileOutputStream outStream(savePathWx);
    if (!outStream.IsOk()) {
        wxLogError("Failed to open '%s' for writing", savePathWx);
        return;
    }

    exportGroup(entry.root, "", cfg);
    cfg.Save(outStream, wxConvUTF8);
}

auto ConfigManager::reloadIfKnown(const wxString& path) -> bool {
    const auto target = toFsPath(path);
    for (std::size_t index = 0; index < CAT_COUNT; index++) {
        const auto& entry = m_categories.at(index);
        // Trigger reload when the user touches either side of the
        // layered pair — the bundle base (rare; usually requires elevated
        // perms inside a bundle) or the writable overlay.
        const bool baseMatch = samePath(target, entry.strategy.basePath());
        const bool overlayMatch = entry.strategy.usesOverlay()
                               && samePath(target, entry.strategy.overlayPath());
        if (baseMatch || overlayMatch) {
            load(entry.category);
            // if this was config, then reload all other files as well.
            if (entry.category == Category::Config) {
                for (std::size_t sub = 1; sub < CAT_COUNT; sub++) {
                    load(m_categories.at(sub).category);
                }
                if (const auto themeRel = config().get_or("theme", ""); !themeRel.empty()) {
                    m_theme.load(themePath(themeRel));
                }
            }
            return true;
        }
    }

    // Theme reload requires a copied path — Theme::load(path) resets the
    // object and then assigns the incoming path back into m_themePath.
    if (const auto currentThemePath = m_theme.getPath(); samePath(target, toFsPath(currentThemePath))) {
        m_theme.load(currentThemePath);
        return true;
    }

    return false;
}

auto ConfigManager::isFirstRun() const -> bool {
    const auto& entry = m_categories.at(static_cast<std::size_t>(Category::Config));
    if (!entry.strategy.usesOverlay()) {
        return false;
    }
    std::error_code ec;
    return !fs::exists(entry.strategy.overlayPath(), ec);
}

auto ConfigManager::get(Category category) -> Value& {
    auto& entry = m_categories.at(static_cast<std::size_t>(category));
    if (entry.category != category) {
        load(category);
    }
    return entry.root;
}

auto ConfigManager::baseline(Category category) -> const Value& {
    auto& entry = m_categories.at(static_cast<std::size_t>(category));
    if (entry.category != category) {
        load(category);
    }
    return entry.baseline;
}

namespace {
/// Compose a wxFileDialog filter fragment from a description + glob:
///     `<desc> (<glob>)|<glob>`
/// Returns empty when `glob` is empty. Falls back to `key` for the
/// description when the locale entry is missing so translation gaps
/// are visible in the dialog.
[[nodiscard]] auto composeFilter(const wxString& key, const wxString& desc, const wxString& glob) -> wxString {
    if (glob.IsEmpty()) {
        return {};
    }
    const auto& label = desc.IsEmpty() ? key : desc;
    return label + " (" + glob + ")|" + glob;
}
} // namespace

auto ConfigManager::filePattern(const wxString& key) -> wxString {
    const auto glob = config().at("filePatterns").get_or(key, "");
    const auto desc = locale().at("filetypes").get_or(key, key);
    return composeFilter(key, desc, glob);
}

auto ConfigManager::filePatterns(const std::initializer_list<std::string_view> keys) -> wxString {
    const auto& patterns = config().at("filePatterns");
    const auto& filetypes = locale().at("filetypes");
    wxString joined;
    for (const auto sv : keys) {
        const wxString key(sv.data(), sv.size());
        const auto glob = patterns.get_or(key, "");
        if (glob.IsEmpty()) {
            continue;
        }
        const auto desc = filetypes.get_or(key, key);
        if (!joined.IsEmpty()) {
            joined += "|";
        }
        joined += composeFilter(key, desc, glob);
    }
    return joined;
}

auto ConfigManager::fileGlob(const wxString& key) -> wxString {
    return config().at("filePatterns").get_or(key, "");
}

auto ConfigManager::isEditorFile(const wxString& filename) -> bool {
    const wxString name = filename.Lower(); // globs are stored lowercase
    const auto matchesKey = [&](const std::string_view key) {
        for (wxString rest = fileGlob(wxString(key.data(), key.size())); !rest.IsEmpty();) {
            const wxString glob = rest.BeforeFirst(';');
            rest = rest.AfterFirst(';');
            if (!glob.IsEmpty() && wxMatchWild(glob.Lower(), name, false)) {
                return true;
            }
        }
        return false;
    };
    for (const auto key : kEditorFileTypeKeys) {
        if (matchesKey(key)) {
            return true;
        }
    }
    // `session` (.fbs) and the hidden `plaintext` key (extensionless README/
    // LICENSE/… ) open in fbide but are kept out of the Open/Save dialog filters.
    return matchesKey("session") || matchesKey("plaintext");
}

// ---------------------------------------------------------------------------
// Path handling
// ---------------------------------------------------------------------------

auto ConfigManager::getAppDir() const -> wxString {
    return toWxString(m_appDir);
}

auto ConfigManager::getIdeDir() const -> wxString {
    return toWxString(m_ideDir);
}

auto ConfigManager::absolute(const wxString& pathName) const -> wxString {
    return toWxString(absolutePath(toFsPath(pathName)));
}

auto ConfigManager::absolutePath(const fs::path& rel) const -> fs::path {
    if (rel.is_absolute()) {
        return rel.lexically_normal();
    }

    // Walk candidate bases: ide → app → cwd. Preserves the order the
    // legacy wxFileName-based resolver used; user-themes-dir precedence
    // is handled separately by `themePath()`. `lexically_normal`
    // resolves `.` and `..` segments without touching the filesystem
    // (so it doesn't canonicalise symlinks — matches prior behaviour
    // and keeps user-visible paths in the form the user supplied).
    std::error_code ec;
    const std::array bases { m_ideDir, m_appDir, fs::current_path(ec) };
    for (const auto& base : bases) {
        if (base.empty()) {
            continue;
        }
        const auto candidate = (base / rel).lexically_normal();
        if (fs::exists(candidate, ec)) {
            return candidate;
        }
    }

    wxLogError("Failed to resolve absolute path %s", toWxString(rel));
    return rel;
}

namespace {
/// Make `abs` relative to `base` iff `abs` is actually under `base`.
/// Returns empty optional when `abs` is on a different subtree (so the
/// caller can fall through to the next candidate base). Uses
/// `lexically_relative` which doesn't touch the filesystem.
[[nodiscard]] auto makeRelative(const fs::path& base, const fs::path& abs) -> std::optional<fs::path> {
    if (base.empty()) {
        return std::nullopt;
    }
    auto rel = abs.lexically_relative(base);
    if (rel.empty() || *rel.begin() == "..") {
        return std::nullopt;
    }
    return rel;
}
} // namespace

auto ConfigManager::relative(const wxString& path) const -> wxString {
    const auto abs = absolutePath(toFsPath(path));
    // Candidate base dirs, in priority order:
    // - m_ideDir       — bundle resources.
    // - m_userDataDir  — writable equivalent of ide/. A path like
    //                    `<userDataDir>/themes/dark.ini` must stringify
    //                    the same as the bundle equivalent so
    //                    themePath() round-trips correctly on the next
    //                    load.
    // - m_appDir       — binary directory; fallback for portable
    //                    side-by-side layouts.
    for (const auto* base : { &m_ideDir, &m_userDataDir, &m_appDir }) {
        if (const auto rel = makeRelative(*base, abs)) {
            return toWxString(rel->generic_string());
        }
    }
    return path;
}
