//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ConfigManager.hpp"
using namespace fbide;

namespace {
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
auto hasReadOnlySentinel(const wxString& dir) -> bool {
    wxFileName marker(dir, kReadOnlySentinel);
    return marker.FileExists();
}
} // namespace

namespace {
/// True when `a` and `b` refer to the same filesystem entry. Follows
/// symlinks on both sides via std::filesystem::equivalent, so editing a
/// config file through a symlink still matches the loaded canonical path.
auto samePath(const wxString& a, const wxString& b) -> bool {
    if (a.empty() || b.empty()) {
        return false;
    }
    std::error_code ec;
#ifdef __WXMSW__
    const std::filesystem::path fa(a.ToStdWstring());
    const std::filesystem::path fb(b.ToStdWstring());
#else
    const std::filesystem::path fa(a.ToStdString(wxConvUTF8));
    const std::filesystem::path fb(b.ToStdString(wxConvUTF8));
#endif
    return std::filesystem::equivalent(fa, fb, ec);
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

static auto enumerate(const wxString& base, const std::initializer_list<wxString> specs = { "*.ini" }) -> std::vector<wxString> {
    std::vector<wxString> files;
    for (const auto& spec : specs) {
        if (const wxDir dir(base); dir.IsOpened()) {
            wxString name;
            if (dir.GetFirst(&name, spec, wxDIR_FILES)) {
                do {
                    wxFileName path { name };
                    path.MakeAbsolute(base);
                    files.emplace_back(path.GetFullPath());
                } while (dir.GetNext(&name));
            }
        }
    }
    std::ranges::sort(files);
    return files;
}

auto ConfigManager::getAllLanguages() const -> std::vector<wxString> {
    return enumerate(m_ideDir / "locales");
}

auto ConfigManager::getAllThemes() const -> std::vector<wxString> {
    return enumerate(m_ideDir / "themes", { "*.ini", "*.fbt" });
}

auto ConfigManager::getPlatformConfigFileName() -> wxString {
#ifdef __WXMSW__
    return "config_win.ini";
#elif defined(__WXOSX__)
    return "config_macos.ini";
#else
    return "config_linux.ini";
#endif
}

auto ConfigManager::getTerminal() -> wxString {
#ifdef __WXMSW__
    return "cmd.exe";
#elif defined(__WXOSX__)
    return "open -a Terminal";
#else
    return "x-terminal-emulator";
#endif
}

auto ConfigManager::getTerminalLauncher() -> wxString {
    return config().get_or("compiler.terminal", getDefaultTerminalLauncher());
}

auto ConfigManager::getDefaultTerminalLauncher() -> wxString {
#ifdef __WXMSW__
    // `cmd /C` runs the program in a new console window allocated by
    // Windows. Console closes when the program exits — add `& pause` or
    // a SLEEP at the end of your program if you need to inspect output.
    // Keeping cmd in the foreground (no `start`) means kill / Stop
    // cascades through cmd's process group to the child program.
    return "cmd /C";
#elif defined(__WXOSX__)
    // TODO: Terminal.app does not accept the program as a CLI argument;
    // launching requires AppleScript via `osascript` or a temp `.command`
    // script. Cannot be expressed as a single-line template prefix.
    return "";
#else
    // `-e` is the de facto flag accepted by the Debian/Ubuntu alternatives
    // symlink. Distro-specific terminals (gnome-terminal `--`, konsole `-e`)
    // may need a custom run-command template.
    return "x-terminal-emulator -e";
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
: m_appDir(appPath)
, m_userDataDir(userDataDirOverride.empty() ? wxStandardPaths::Get().GetUserDataDir() : userDataDirOverride) {
    if (not wxDirExists(appPath)) {
        wxLogError("app directory '%s' does not exist", appPath);
        return;
    }

    if (not idePath.empty()) {
        const auto path = absolute(idePath);
        if (wxDirExists(path)) {
            m_ideDir = path;
        } else {
            wxLogWarning("ide config directory '%s' does not exist", path);
        }
    }
    if (m_ideDir.empty()) {
#ifdef __WXOSX__
        // Prefer bundle Resources when running inside a macOS .app
        wxFileName appPathFn(m_appDir, "");
        const auto& dirs = appPathFn.GetDirs();
        if (!dirs.empty() && dirs.back() == "MacOS") {
            appPathFn.RemoveLastDir(); // MacOS -> Contents
            const wxString contentsDir = appPathFn.GetPath();
            const wxString bundleIde = contentsDir + "/Resources/ide";
            if (wxDirExists(bundleIde)) {
                m_ideDir = bundleIde;
            } else {
                // Fallback if bundle layout not as expected
                m_ideDir = m_appDir / "ide";
            }
        } else {
            // Not in a bundle (e.g., running from build dir)
            m_ideDir = m_appDir / "ide";
        }
#elif defined(FBIDE_APPIMAGE_BUILD)
        // FHS layout used by AppImage / future deb / rpm packages: the
        // binary lives at <prefix>/bin/fbide and resources at
        // <prefix>/share/fbide/ide. Walk one directory up from the
        // binary location and look for share/fbide/ide; fall back to
        // the portable side-by-side layout if that directory is absent
        // (e.g. running from a build tree before `cmake --install`).
        wxFileName appPathFn(m_appDir, "");
        const auto& dirs = appPathFn.GetDirs();
        if (!dirs.empty() && dirs.back() == "bin") {
            appPathFn.RemoveLastDir();
            const wxString prefix = appPathFn.GetPath();
            const wxString fhsIde = prefix + "/share/fbide/ide";
            if (wxDirExists(fhsIde)) {
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
    if (not wxDirExists(m_ideDir)) {
        wxLogError("ide config directory '%s' does not exist", m_ideDir);
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
        wxLogMessage("READONLY sentinel detected in '%s' — overlays route to '%s'", m_ideDir, m_userDataDir);
    }

    auto& entry = m_categories[static_cast<std::size_t>(Category::Config)];
    entry.strategy = buildStrategy(
        Category::Config,
        absolute(configPath.empty() ? getPlatformConfigFileName() : configPath)
    );
    wxLogMessage("ide directory: %s", m_ideDir);
    load(Category::Config);

    // Load all configs
    for (const auto cat : { Category::Locale, Category::Shortcuts, Category::Keywords, Category::Layout }) {
        load(cat);
    }

    // Resolve + load theme immediately after config is available. If the
    // configured theme file is missing or absent, fall back to a built-in
    // minimal theme so the editor still launches with a usable scheme.
    if (const auto themeRel = config().get_or("theme", wxString {}); not themeRel.empty()) {
        const auto themeAbs = absolute(themeRel);
        if (wxFileExists(themeAbs)) {
            m_theme.load(themeAbs);
            wxLogMessage("Loaded theme from %s", themeAbs);
        } else {
            wxLogError("Theme file '%s' not found — using built-in default", themeAbs);
            m_theme.loadDefaults();
        }
    } else {
        wxLogWarning("No 'theme' entry found in config '%s' — using built-in default", entry.strategy.basePath());
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
    auto& entry = m_categories[static_cast<std::size_t>(Category::Config)];
    entry.strategy = buildStrategy(Category::Config, absolute(configPath));
    load(Category::Config);

    if (const auto themeRel = config().get_or("theme", wxString {}); not themeRel.empty()) {
        const auto themeAbs = absolute(themeRel);
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

auto ConfigManager::buildStrategy(const Category category, wxString basePath) const -> ConfigStrategy {
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
    auto& entry = m_categories[static_cast<std::size_t>(category)];
    wxString file;

    if (category == Category::Config) {
        // Strategy already built by ctor / reloadConfig before they
        // called us — config is the bootstrap, its path is known up
        // front. Sub-categories below get strategy built right after
        // path resolution.
        file = entry.strategy.basePath();
    } else {
        const auto key = getCategoryName(category);
        const auto& ref = config().at({ key.data(), key.size() });
        const auto relPath = ref.as<wxString>();
        if (!relPath.has_value() || relPath->empty()) {
            wxLogError("Config category '%s' missing or invalid", key.data());
            return;
        }
        file = absolute(*relPath);
        entry.strategy = buildStrategy(category, file);
    }

    if (!wxFileExists(file)) {
        // Per-category recovery for missing files. Config + Layout are
        // load-bearing for the rest of the IDE, so a miss is fatal —
        // every other path falls back to a workable empty / default
        // state and lets the app keep going.
        const auto catName = getCategoryName(category);
        wxLogError(
            "Config file '%s' for '%s' category not found",
            file, catName.data()
        );
        switch (category) {
        case Category::Config:
            fatalAndExit(
                wxString::Format(
                    "FBIde could not start: the main configuration file was not found.\n\n"
                    "Expected at:\n%s\n\n"
                    "Reinstall FBIde or supply --config=<path>.",
                    file
                )
            );
        case Category::Layout:
            fatalAndExit(
                wxString::Format(
                    "FBIde could not start: the layout file was not found.\n\n"
                    "Expected at:\n%s\n\n"
                    "Reinstall FBIde or restore the layout.ini next to the IDE resources.",
                    file
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
                    file
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

    wxFFileInputStream stream(file);
    if (!stream.IsOk()) {
        wxLogError("Failed to open '%s' for reading", file);
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
    if (entry.strategy.usesOverlay() && wxFileExists(entry.strategy.overlayPath())) {
        wxFFileInputStream overlayStream(entry.strategy.overlayPath());
        if (overlayStream.IsOk()) {
            wxFileConfig overlayCfg(overlayStream, wxConvUTF8);
            Value overlay;
            overlayCfg.SetPath("/");
            importGroup(overlayCfg, overlay);
            root.mergeFrom(overlay);
            wxLogMessage(
                "Merged overlay %s into %s",
                entry.strategy.overlayPath(), getCategoryName(category).data()
            );
        } else {
            wxLogWarning("Overlay '%s' exists but could not be read", entry.strategy.overlayPath());
        }
    }

    entry.category = category;
    entry.baseline = std::move(baseline);
    entry.root = std::move(root);
    wxLogMessage("Loaded %s from %s", getCategoryName(category).data(), file);
}

void ConfigManager::save(const Category category) {
    const auto& entry = m_categories[static_cast<std::size_t>(category)];
    if (entry.category != category) {
        wxLogWarning("Trying to save unloaded category '%s'", getCategoryName(category).data());
        return;
    }

    if (entry.strategy.usesOverlay()) {
        // Aggressive prune: persist only leaves that diverge from the
        // bundle baseline. Empty diff → no overlay file at all (delete
        // if one existed). Overlay file is machine-managed; we write
        // fresh from the diff rather than merging into existing keys so
        // stale entries from prior saves don't accumulate.
        const auto& overlayPath = entry.strategy.overlayPath();
        const auto diff = entry.root.diffAgainst(entry.baseline);
        if (!diff) {
            if (wxFileExists(overlayPath)) {
                wxRemoveFile(overlayPath);
                wxLogMessage("Pruned empty overlay '%s'", overlayPath);
            }
            return;
        }
        wxFileConfig overlayCfg;
        wxFFileOutputStream outStream(overlayPath);
        if (!outStream.IsOk()) {
            wxLogError("Failed to open '%s' for writing", overlayPath);
            return;
        }
        exportGroup(diff, "", overlayCfg);
        overlayCfg.Save(outStream, wxConvUTF8);
        wxLogMessage("Saved overlay '%s'", overlayPath);
        return;
    }

    // Direct mode (`--config=PATH` or locale). Open the read stream
    // before the write stream: `wxFileConfig` parses existingStream in
    // its constructor (preserves comments + ordering), then we truncate
    // the file for writing. Reversing the order would zero the file
    // before parsing completed.
    const auto& savePath = entry.strategy.savePath();
    wxFileInputStream existingStream(savePath);
    wxFileConfig cfg(existingStream, wxConvUTF8);
    wxFFileOutputStream outStream(savePath);
    if (!outStream.IsOk()) {
        wxLogError("Failed to open '%s' for writing", savePath);
        return;
    }

    exportGroup(entry.root, "", cfg);
    cfg.Save(outStream, wxConvUTF8);
}

auto ConfigManager::reloadIfKnown(const wxString& path) -> bool {
    for (std::size_t index = 0; index < CAT_COUNT; index++) {
        const auto& entry = m_categories[index];
        // Trigger reload when the user touches either side of the
        // layered pair — the bundle base (rare; usually requires elevated
        // perms inside a bundle) or the writable overlay.
        const bool baseMatch = samePath(path, entry.strategy.basePath());
        const bool overlayMatch = entry.strategy.usesOverlay()
                               && samePath(path, entry.strategy.overlayPath());
        if (baseMatch || overlayMatch) {
            load(entry.category);
            // if this was config, then reload all other files as well.
            if (entry.category == Category::Config) {
                for (std::size_t sub = 1; sub < CAT_COUNT; sub++) {
                    load(m_categories[sub].category);
                }
                if (const auto themeRel = config().get_or("theme", ""); not themeRel.empty()) {
                    m_theme.load(absolute(themeRel));
                }
            }
            return true;
        }
    }

    // Theme reload requires a copied path — Theme::load(path) resets the
    // object and then assigns the incoming path back into m_themePath.
    if (const auto themePath = m_theme.getPath(); samePath(path, themePath)) {
        m_theme.load(themePath);
        return true;
    }

    return false;
}

auto ConfigManager::get(Category category) -> Value& {
    auto& entry = m_categories.at(static_cast<std::size_t>(category));
    if (entry.category != category) {
        load(category);
    }
    return entry.root;
}

namespace {
/// Compose a wxFileDialog filter fragment from a description + glob:
///     `<desc> (<glob>)|<glob>`
/// Returns empty when `glob` is empty. Falls back to `key` for the
/// description when the locale entry is missing so translation gaps
/// are visible in the dialog.
auto composeFilter(const wxString& key, const wxString& desc, const wxString& glob) -> wxString {
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

// ---------------------------------------------------------------------------
// Path handling
// ---------------------------------------------------------------------------

auto ConfigManager::absolute(const wxString& pathName) const -> wxString {
    wxFileName path(pathName);
    path.Normalize(wxPATH_NORM_ENV_VARS | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE | wxPATH_NORM_SHORTCUT);

    if (path.IsAbsolute()) {
        return path.GetAbsolutePath();
    }

    wxFileName fn(path);

    fn.MakeAbsolute(m_ideDir);
    if (fn.Exists()) {
        return fn.GetAbsolutePath();
    }

    fn = path;
    fn.MakeAbsolute(m_appDir);
    if (fn.Exists()) {
        return fn.GetAbsolutePath();
    }

    fn = path;
    fn.MakeAbsolute(wxGetCwd());
    if (fn.Exists()) {
        return fn.GetAbsolutePath();
    }

    wxLogError("Failed to resolve absolute path %s", pathName);
    return pathName;
}

static auto makeRelative(const wxString& path, const wxString& to) -> std::optional<wxString> {
    if (to.StartsWith(path)) {
        wxFileName result(to);
        result.MakeRelativeTo(path);
        return result.GetFullPath(wxPATH_UNIX);
    }
    return {};
}

auto ConfigManager::relative(const wxString& path) const -> wxString {
    const auto abs = absolute(path);
    if (const auto ide = makeRelative(m_ideDir, abs)) {
        return *ide;
    }
    if (const auto app = makeRelative(m_appDir, abs)) {
        return *app;
    }
    return path;
}
