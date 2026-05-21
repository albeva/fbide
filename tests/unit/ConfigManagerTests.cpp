//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <gtest/gtest.h>
#include "config/ConfigManager.hpp"
#include "config/Theme.hpp"
#include "config/ThemeCategory.hpp"

using namespace fbide;

namespace {
/// RAII scratch directory used to host config + ide layout for a single
/// test. Auto-cleans on destruction so tests don't leak files between
/// runs even when assertions fail mid-flight.
class TempDir final {
public:
    TempDir() {
        const auto base = wxFileName::CreateTempFileName("fbide_cfg_test");
        wxRemoveFile(base);
        wxFileName::Mkdir(base, 0755, wxPATH_MKDIR_FULL);
        m_path = base;
    }
    ~TempDir() {
        if (!m_path.IsEmpty() && wxDirExists(m_path)) {
            wxFileName::Rmdir(m_path, wxPATH_RMDIR_RECURSIVE);
        }
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;

    [[nodiscard]] auto path() const -> const wxString& { return m_path; }

    void write(const wxString& relPath, const wxString& contents) const {
        const wxString full = m_path + "/" + relPath;
        wxFileName fn(full);
        wxFileName::Mkdir(fn.GetPath(), 0755, wxPATH_MKDIR_FULL);
        wxFFile out(full, "w");
        out.Write(contents);
    }

private:
    wxString m_path;
};
} // namespace

class ConfigManagerTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Per-category fallback when the backing file is missing
// ---------------------------------------------------------------------------

TEST_F(ConfigManagerTests, MissingKeywordsLoadsEmpty) {
    TempDir tmp;
    tmp.write("config.ini",
        "version=0.5.0\n"
        "keywords=does_not_exist.ini\n");

    ConfigManager cm(tmp.path(), tmp.path(), "config.ini");
    auto& kw = cm.keywords();
    // Empty fallback — every lookup returns the supplied default. The
    // editor still launches; the user just has no keyword groups.
    EXPECT_EQ(kw.get_or("groups.Keywords", wxString { "<sentinel>" }), "<sentinel>");
}

TEST_F(ConfigManagerTests, MissingShortcutsLoadsEmpty) {
    TempDir tmp;
    tmp.write("config.ini",
        "version=0.5.0\n"
        "shortcuts=does_not_exist.ini\n");

    ConfigManager cm(tmp.path(), tmp.path(), "config.ini");
    auto& sc = cm.shortcuts();
    EXPECT_EQ(sc.get_or("file.open", wxString { "<sentinel>" }), "<sentinel>");
}

// Note: missing Config / Layout / Locale exits the process via
// `fatalAndExit` (plain-English message-box → std::exit). Not covered
// by direct tests because they would terminate the gtest runner. The
// fallback paths above (Keywords / Shortcuts / Theme) are the ones
// callers expect to keep going.

// ---------------------------------------------------------------------------
// Theme fallback — `m_theme.loadDefaults()` runs when the configured
// theme file is missing or absent from `[theme]`.
// ---------------------------------------------------------------------------

TEST_F(ConfigManagerTests, MissingThemeFileTriggersDefaults) {
    TempDir tmp;
    tmp.write("config.ini",
        "version=0.5.0\n"
        "theme=themes/does_not_exist.ini\n");

    ConfigManager cm(tmp.path(), tmp.path(), "config.ini");
    const auto& theme = cm.getTheme();
    EXPECT_EQ(theme.get(ThemeCategory::Default).colors.foreground, *wxBLACK);
    EXPECT_EQ(theme.get(ThemeCategory::Default).colors.background, *wxWHITE);
}

TEST_F(ConfigManagerTests, MissingThemeEntryTriggersDefaults) {
    TempDir tmp;
    // No `theme=` key at all — fallback kicks in just the same.
    tmp.write("config.ini", "version=0.5.0\n");

    ConfigManager cm(tmp.path(), tmp.path(), "config.ini");
    const auto& theme = cm.getTheme();
    EXPECT_EQ(theme.get(ThemeCategory::Default).colors.foreground, *wxBLACK);
    EXPECT_EQ(theme.get(ThemeCategory::Default).colors.background, *wxWHITE);
}

// ---------------------------------------------------------------------------
// Layered config — bundle base + .local.ini overlay
//
// These tests exercise the default-boot path (empty `configPath` → Overlay
// strategy). They lay out a minimal bundle under <tmp>/ide/ and drive
// READONLY routing via the `userDataDirOverride` ctor seam so nothing
// touches the real platform user-data directory.
// ---------------------------------------------------------------------------

namespace {
/// Compute the `<base>.local.ini` filename for the running platform's
/// config — same logic as `ConfigStrategy::deriveOverlayPath` but just
/// the basename, since these tests pick the path themselves.
auto overlayBasename(const wxString& configBasename) -> wxString {
    wxFileName fn(configBasename);
    fn.SetName(fn.GetName() + ".local");
    return fn.GetFullName();
}
} // namespace

TEST_F(ConfigManagerTests, DefaultBootPortableNoOverlayLoadsBundleAsIs) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName,
        "[editor]\n"
        "tabSize=4\n"
        "theme=dark\n");

    // No overlay file present — root must reflect bundle exactly.
    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "");
    EXPECT_EQ(cm.config().get_or("editor.tabSize", wxString { "<missing>" }), "4");
    EXPECT_EQ(cm.config().get_or("editor.theme", wxString { "<missing>" }), "dark");
}

TEST_F(ConfigManagerTests, DefaultBootOverlayMergesIntoConfigRoot) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName,
        "[editor]\n"
        "tabSize=4\n"
        "theme=dark\n");
    tmp.write("ide/" + overlayBasename(cfgName),
        "[editor]\n"
        "tabSize=8\n");

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "");
    // Overlay wins where it diverges, bundle survives where it doesn't.
    EXPECT_EQ(cm.config().get_or("editor.tabSize", wxString { "<missing>" }), "8");
    EXPECT_EQ(cm.config().get_or("editor.theme", wxString { "<missing>" }), "dark");
}

TEST_F(ConfigManagerTests, SaveMatchingBaselineProducesNoOverlayFile) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName,
        "[editor]\n"
        "tabSize=4\n");

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "");
    // Mutate then revert — diff against baseline is empty.
    cm.config()["editor"]["tabSize"] = "8";
    cm.config()["editor"]["tabSize"] = "4";
    cm.save(ConfigManager::Category::Config);

    const wxString overlayPath = tmp.path() + "/ide/" + overlayBasename(cfgName);
    EXPECT_FALSE(wxFileExists(overlayPath));
}

TEST_F(ConfigManagerTests, SaveDivergentValueWritesPrunedOverlay) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName,
        "[editor]\n"
        "tabSize=4\n"
        "theme=dark\n");

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "");
    cm.config()["editor"]["tabSize"] = "8";
    cm.save(ConfigManager::Category::Config);

    const wxString overlayPath = tmp.path() + "/ide/" + overlayBasename(cfgName);
    ASSERT_TRUE(wxFileExists(overlayPath));

    // Parse the produced overlay and confirm it carries only the divergence.
    wxFFileInputStream in(overlayPath);
    wxFileConfig produced(in, wxConvUTF8);
    wxString value;
    EXPECT_TRUE(produced.Read("editor/tabSize", &value));
    EXPECT_EQ(value, "8");
    EXPECT_FALSE(produced.Read("editor/theme", &value));
}

TEST_F(ConfigManagerTests, SaveResetToBaselineDeletesExistingOverlay) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName,
        "[editor]\n"
        "tabSize=4\n");
    // Pre-existing overlay from a previous run that we now revert.
    tmp.write("ide/" + overlayBasename(cfgName),
        "[editor]\n"
        "tabSize=8\n");

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "");
    EXPECT_EQ(cm.config().get_or("editor.tabSize", wxString {}), "8");

    cm.config()["editor"]["tabSize"] = "4";
    cm.save(ConfigManager::Category::Config);

    const wxString overlayPath = tmp.path() + "/ide/" + overlayBasename(cfgName);
    EXPECT_FALSE(wxFileExists(overlayPath));
}

TEST_F(ConfigManagerTests, ReadOnlyRoutesOverlayToUserDataDir) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName,
        "[editor]\n"
        "tabSize=4\n");
    tmp.write("ide/READONLY", "");
    // userDataDir must exist before we try to write into it.
    wxFileName::Mkdir(tmp.path() + "/userdata", 0755, wxPATH_MKDIR_FULL);

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "", tmp.path() + "/userdata");
    cm.config()["editor"]["tabSize"] = "8";
    cm.save(ConfigManager::Category::Config);

    const wxString bundleOverlay = tmp.path() + "/ide/" + overlayBasename(cfgName);
    const wxString userOverlay = tmp.path() + "/userdata/" + overlayBasename(cfgName);
    EXPECT_FALSE(wxFileExists(bundleOverlay));
    EXPECT_TRUE(wxFileExists(userOverlay));
}

TEST_F(ConfigManagerTests, ReadOnlyLoadsOverlayFromUserDataDir) {
    // Round-trip: write an overlay into the user dir, construct
    // ConfigManager pointing at the same dir, confirm the overlay is
    // picked up during load (not the absent bundle-adjacent one).
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName,
        "[editor]\n"
        "tabSize=4\n");
    tmp.write("ide/READONLY", "");
    tmp.write("userdata/" + overlayBasename(cfgName),
        "[editor]\n"
        "tabSize=12\n");

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "", tmp.path() + "/userdata");
    EXPECT_EQ(cm.config().get_or("editor.tabSize", wxString { "<missing>" }), "12");
}

// ---------------------------------------------------------------------------
// Theme two-dir enumeration + path resolution (READONLY only)
// ---------------------------------------------------------------------------

TEST_F(ConfigManagerTests, GetAllThemesPortableEnumeratesBundleOnly) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName, "\n");
    tmp.write("ide/themes/dark.ini", "[Default]\n");
    tmp.write("ide/themes/light.ini", "[Default]\n");
    // userdata exists but no READONLY — its themes/ must not contribute.
    wxFileName::Mkdir(tmp.path() + "/userdata/themes", 0755, wxPATH_MKDIR_FULL);
    tmp.write("userdata/themes/ignored.ini", "[Default]\n");

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "", tmp.path() + "/userdata");

    const auto themes = cm.getAllThemes();
    ASSERT_EQ(themes.size(), 2);
    EXPECT_EQ(wxFileName(themes[0]).GetFullName(), "dark.ini");
    EXPECT_EQ(wxFileName(themes[1]).GetFullName(), "light.ini");
}

TEST_F(ConfigManagerTests, GetAllThemesReadOnlyMergesBundleAndUserDirs) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName, "\n");
    tmp.write("ide/READONLY", "");
    tmp.write("ide/themes/dark.ini", "[Default]\n");
    tmp.write("ide/themes/light.ini", "[Default]\n");

    wxFileName::Mkdir(tmp.path() + "/userdata/themes", 0755, wxPATH_MKDIR_FULL);
    tmp.write("userdata/themes/solarized.ini", "[Default]\n");

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "", tmp.path() + "/userdata");

    const auto themes = cm.getAllThemes();
    ASSERT_EQ(themes.size(), 3);
    // Sorted by basename across both dirs.
    EXPECT_EQ(wxFileName(themes[0]).GetFullName(), "dark.ini");
    EXPECT_EQ(wxFileName(themes[1]).GetFullName(), "light.ini");
    EXPECT_EQ(wxFileName(themes[2]).GetFullName(), "solarized.ini");
}

TEST_F(ConfigManagerTests, GetAllThemesUserDirWinsOnBasenameCollision) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName, "\n");
    tmp.write("ide/READONLY", "");
    tmp.write("ide/themes/dark.ini", "[Default]\n");
    wxFileName::Mkdir(tmp.path() + "/userdata/themes", 0755, wxPATH_MKDIR_FULL);
    tmp.write("userdata/themes/dark.ini", "[Default]\n");

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "", tmp.path() + "/userdata");

    const auto themes = cm.getAllThemes();
    ASSERT_EQ(themes.size(), 1);
    EXPECT_EQ(wxFileName(themes[0]).GetPath(), tmp.path() + "/userdata/themes");
}

TEST_F(ConfigManagerTests, ThemePathReadOnlyPrefersUserOverride) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName, "\n");
    tmp.write("ide/READONLY", "");
    tmp.write("ide/themes/dark.ini", "[Default]\n");
    wxFileName::Mkdir(tmp.path() + "/userdata/themes", 0755, wxPATH_MKDIR_FULL);
    tmp.write("userdata/themes/dark.ini", "[Default]\n");

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "", tmp.path() + "/userdata");

    const auto resolved = cm.themePath("themes/dark.ini");
    EXPECT_EQ(wxFileName(resolved).GetPath(), tmp.path() + "/userdata/themes");
}

TEST_F(ConfigManagerTests, ThemePathReadOnlyFallsBackToBundleWhenUserMissing) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName, "\n");
    tmp.write("ide/READONLY", "");
    tmp.write("ide/themes/dark.ini", "[Default]\n");
    wxFileName::Mkdir(tmp.path() + "/userdata/themes", 0755, wxPATH_MKDIR_FULL);

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "", tmp.path() + "/userdata");

    const auto resolved = cm.themePath("themes/dark.ini");
    EXPECT_EQ(wxFileName(resolved).GetPath(), tmp.path() + "/ide/themes");
}

TEST_F(ConfigManagerTests, ThemePathPortableIgnoresUserDir) {
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName, "\n");
    tmp.write("ide/themes/dark.ini", "[Default]\n");
    wxFileName::Mkdir(tmp.path() + "/userdata/themes", 0755, wxPATH_MKDIR_FULL);
    tmp.write("userdata/themes/dark.ini", "[Default]\n");

    // No READONLY sentinel → user dir is ignored even though it exists.
    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "", tmp.path() + "/userdata");

    const auto resolved = cm.themePath("themes/dark.ini");
    EXPECT_EQ(wxFileName(resolved).GetPath(), tmp.path() + "/ide/themes");
}

TEST_F(ConfigManagerTests, ReloadConfigCascadesSubCategoriesToDirectMode) {
    // Default-boot start with overlay-mode keywords. After
    // `reloadConfig(PATH)` flips to explicit mode, mutating and saving
    // keywords must hit the base file directly — no `.local.ini` is
    // produced, because the sub-category strategy was rebuilt under
    // the new mode.
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName, "keywords=keywords.ini\n");
    tmp.write("ide/keywords.ini",
        "[functions]\n"
        "Print=1\n");
    tmp.write("explicit.ini", "keywords=alt_keywords.ini\n");
    tmp.write("alt_keywords.ini",
        "[functions]\n"
        "Foo=1\n");

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "");
    EXPECT_EQ(cm.keywords().get_or("functions.Print", wxString { "<missing>" }), "1");

    cm.reloadConfig(tmp.path() + "/explicit.ini");

    // New keywords visible; old keywords gone.
    EXPECT_EQ(cm.keywords().get_or("functions.Foo", wxString { "<missing>" }), "1");
    EXPECT_EQ(cm.keywords().get_or("functions.Print", wxString { "<missing>" }), "<missing>");

    // Mutate + save. Direct mode → base file overwritten, no overlay.
    cm.keywords()["functions"]["Bar"] = "1";
    cm.save(ConfigManager::Category::Keywords);

    EXPECT_FALSE(wxFileExists(tmp.path() + "/alt_keywords.local.ini"));

    wxFFileInputStream in(tmp.path() + "/alt_keywords.ini");
    wxFileConfig produced(in, wxConvUTF8);
    wxString value;
    EXPECT_TRUE(produced.Read("functions/Bar", &value));
    EXPECT_EQ(value, "1");
}

TEST_F(ConfigManagerTests, SetCategoryPathInheritsBootModeStrategy) {
    // Default-boot rotation. `setCategoryPath` flows through `load()`
    // which calls `buildStrategy()` under the current mode (Overlay),
    // so the new sub-category gets an overlay path alongside its base.
    TempDir tmp;
    const auto cfgName = ConfigManager::getPlatformConfigFileName();
    tmp.write("ide/" + cfgName, "keywords=keywords.ini\n");
    tmp.write("ide/keywords.ini",
        "[functions]\n"
        "Print=1\n");
    tmp.write("ide/alt_keywords.ini",
        "[functions]\n"
        "Alt=1\n");

    ConfigManager cm(tmp.path(), tmp.path() + "/ide", "");
    cm.setCategoryPath(ConfigManager::Category::Keywords, tmp.path() + "/ide/alt_keywords.ini");

    EXPECT_EQ(cm.keywords().get_or("functions.Alt", wxString { "<missing>" }), "1");
    EXPECT_EQ(cm.keywords().get_or("functions.Print", wxString { "<missing>" }), "<missing>");

    cm.keywords()["functions"]["Bar"] = "1";
    cm.save(ConfigManager::Category::Keywords);

    // Overlay mode inherits → `.local.ini` lands next to the new base.
    EXPECT_TRUE(wxFileExists(tmp.path() + "/ide/alt_keywords.local.ini"));
}

TEST_F(ConfigManagerTests, ExplicitConfigPathBypassesOverlay) {
    // `--config=PATH` semantics: even with READONLY sentinel and an
    // overlay file alongside, the explicit path is treated as a single
    // self-contained config. Saves go back to PATH directly.
    TempDir tmp;
    tmp.write("explicit.ini",
        "[editor]\n"
        "tabSize=4\n");
    tmp.write("explicit.local.ini",
        "[editor]\n"
        "tabSize=999\n");

    ConfigManager cm(tmp.path(), tmp.path(), "explicit.ini");
    // Overlay must be ignored — root reflects the explicit file only.
    EXPECT_EQ(cm.config().get_or("editor.tabSize", wxString {}), "4");

    cm.config()["editor"]["tabSize"] = "16";
    cm.save(ConfigManager::Category::Config);

    // Save must overwrite explicit.ini, not produce any new overlay.
    wxFFileInputStream in(tmp.path() + "/explicit.ini");
    wxFileConfig roundTrip(in, wxConvUTF8);
    wxString value;
    EXPECT_TRUE(roundTrip.Read("editor/tabSize", &value));
    EXPECT_EQ(value, "16");
}
