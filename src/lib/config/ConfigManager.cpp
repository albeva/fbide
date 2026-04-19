//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ConfigManager.hpp"
using namespace fbide;

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

static auto enumerate(const wxString& base) -> std::vector<wxString> {
    std::vector<wxString> files;
    if (const wxDir dir(base); dir.IsOpened()) {
        wxString name;
        if (dir.GetFirst(&name, "*.ini", wxDIR_FILES)) {
            do {
                wxFileName path { name };
                path.MakeAbsolute(base);
                files.emplace_back(path.GetFullPath());
            } while (dir.GetNext(&name));
        }
    }
    return files;
}

auto ConfigManager::getAllLanguages() const -> std::vector<wxString> {
    return enumerate(m_ideDir / "locales");
}

auto ConfigManager::getAllThemes() const -> std::vector<wxString> {
    return enumerate(m_ideDir / "themes");
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

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

ConfigManager::ConfigManager(const wxString& appPath, const wxString& idePath, const wxString& configPath)
: m_appDir(appPath) {
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
        m_ideDir = m_appDir / "IDE" / "v2";
    }
    if (not wxDirExists(m_ideDir)) {
        wxLogError("ide config directory '%s' does not exist", m_ideDir);
        return;
    }

    auto& entry = m_categories[static_cast<std::size_t>(Category::Config)];
    entry.path = absolute(configPath.empty() ? getPlatformConfigFileName() : configPath);
    load(Category::Config);

    // Resolve + load theme immediately after config is available.
    if (const auto themeRel = config().get_or("theme", wxString {}); not themeRel.empty()) {
        m_theme.load(absolute(themeRel));
    } else {
        wxLogWarning("No 'theme' entry found in config '%s'", entry.path);
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
    auto& entry = m_categories[static_cast<std::size_t>(Category::Config)];
    entry.path = absolute(configPath);
    load(Category::Config);

    if (const auto themeRel = config().get_or("theme", wxString {}); not themeRel.empty()) {
        m_theme.load(absolute(themeRel));
    }
}

// ---------------------------------------------------------------------------
// Load / save
// ---------------------------------------------------------------------------

void ConfigManager::load(const Category category) {
    auto& entry = m_categories[static_cast<std::size_t>(category)];
    wxString file;

    if (category == Category::Config) {
        file = entry.path;
    } else {
        const auto key = getCategoryName(category);
        const auto& ref = config().at({ key.data(), key.size() });
        const auto relPath = ref.as<wxString>();
        if (!relPath.has_value() || relPath->empty()) {
            wxLogError("Config category '%s' missing or invalid", key.data());
            return;
        }
        file = absolute(*relPath);
    }

    if (!wxFileExists(file)) {
        wxLogError("Config file '%s' for '%s' category not found", file, getCategoryName(category).data());
        return;
    }

    wxFFileInputStream stream(file);
    if (!stream.IsOk()) {
        wxLogError("Failed to open '%s' for reading", file);
        return;
    }

    wxFileConfig cfg(stream, wxConvUTF8);
    cfg.SetPath("/");

    Value root;
    importGroup(cfg, root);

    entry.category = category;
    entry.path = file;
    entry.root = std::move(root);
}

void ConfigManager::save(const Category category) {
    const auto& entry = m_categories[static_cast<std::size_t>(category)];
    if (entry.category != category) {
        wxLogWarning("Trying to save unloaded category '%s'", getCategoryName(category).data());
        return;
    }

    // Open the read stream before the write stream: wxFileConfig parses
    // existingStream in its constructor (preserves comments + ordering),
    // then we truncate the file for writing. Reversing the order would
    // zero the file before parsing completed.
    wxFileInputStream existingStream(entry.path);
    wxFileConfig cfg(existingStream, wxConvUTF8);
    wxFFileOutputStream outStream(entry.path);
    if (!outStream.IsOk()) {
        wxLogError("Failed to open '%s' for writing", entry.path);
        return;
    }

    exportGroup(entry.root, "", cfg);
    cfg.Save(outStream, wxConvUTF8);
}

auto ConfigManager::get(Category category) -> Value& {
    auto& entry = m_categories.at(static_cast<std::size_t>(category));
    if (entry.category != category) {
        load(category);
    }
    return entry.root;
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
        return result.GetFullPath();
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
