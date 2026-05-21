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

/// Seed a minimal bundle layout under `<tmp>/ide/`. Writes the
/// platform-specific config file, optionally drops the READONLY
/// sentinel, and returns the ide dir path so the caller can pass it to
/// the `ConfigManager` ctor.
auto seedBundle(const TempDir& tmp, const wxString& configContents = "\n", const bool readOnly = false) -> wxString {
    tmp.write("ide/" + ConfigManager::getPlatformConfigFileName(), configContents);
    if (readOnly) {
        tmp.write("ide/READONLY", "");
    }
    return tmp.path() + "/ide";
}

/// Compute the `<base>.local.ini` filename for the running platform's
/// config — same logic as `ConfigStrategy::deriveOverlayPath` but just
/// the basename, since these tests pick the path themselves.
auto overlayBasename() -> wxString {
    wxFileName fn(ConfigManager::getPlatformConfigFileName());
    fn.SetName(fn.GetName() + ".local");
    return fn.GetFullName();
}
} // namespace

class ConfigManagerTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Per-category fallback when the backing file is missing
//
// Note: missing Config / Layout / Locale exits the process via
// `fatalAndExit` (plain-English message-box → std::exit). Not covered
// by direct tests because they would terminate the gtest runner. The
// fallback paths below (Keywords / Shortcuts / Theme) are the ones
// callers expect to keep going.
// ---------------------------------------------------------------------------

TEST_F(ConfigManagerTests, MissingNonFatalCategoriesLoadEmpty) {
    struct Case {
        ConfigManager::Category cat;
        const char* configLine;
        const char* lookupKey;
    };
    const Case cases[] = {
        { ConfigManager::Category::Keywords, "keywords=does_not_exist.ini\n", "groups.Keywords" },
        { ConfigManager::Category::Shortcuts, "shortcuts=does_not_exist.ini\n", "file.open" },
    };
    for (const auto& tc : cases) {
        TempDir tmp;
        tmp.write("config.ini", wxString::Format("version=0.5.0\n%s", tc.configLine));

        ConfigManager cm(tmp.path(), tmp.path(), "config.ini");
        EXPECT_EQ(
            cm.get(tc.cat).get_or(tc.lookupKey, wxString { "<sentinel>" }),
            "<sentinel>"
        ) << "category="
          << ConfigManager::getCategoryName(tc.cat);
    }
}

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
// strategy). They lay out a minimal bundle under <tmp>/ide/ via
// `seedBundle` and drive READONLY routing via the `userDataDirOverride`
// ctor seam so nothing touches the real platform user-data directory.
// ---------------------------------------------------------------------------

TEST_F(ConfigManagerTests, DefaultBootPortableNoOverlayLoadsBundleAsIs) {
    TempDir tmp;
    const auto ideDir = seedBundle(tmp,
        "[editor]\n"
        "tabSize=4\n"
        "theme=dark\n");

    // No overlay file present — root must reflect bundle exactly.
    ConfigManager cm(tmp.path(), ideDir, "");
    EXPECT_EQ(cm.config().get_or("editor.tabSize", wxString { "<missing>" }), "4");
    EXPECT_EQ(cm.config().get_or("editor.theme", wxString { "<missing>" }), "dark");
}

TEST_F(ConfigManagerTests, DefaultBootOverlayMergesIntoConfigRoot) {
    TempDir tmp;
    const auto ideDir = seedBundle(tmp,
        "[editor]\n"
        "tabSize=4\n"
        "theme=dark\n");
    tmp.write("ide/" + overlayBasename(),
        "[editor]\n"
        "tabSize=8\n");

    ConfigManager cm(tmp.path(), ideDir, "");
    // Overlay wins where it diverges, bundle survives where it doesn't.
    EXPECT_EQ(cm.config().get_or("editor.tabSize", wxString { "<missing>" }), "8");
    EXPECT_EQ(cm.config().get_or("editor.theme", wxString { "<missing>" }), "dark");
}

TEST_F(ConfigManagerTests, SaveMatchingBaselineProducesNoOverlayFile) {
    TempDir tmp;
    const auto ideDir = seedBundle(tmp,
        "[editor]\n"
        "tabSize=4\n");

    ConfigManager cm(tmp.path(), ideDir, "");
    // Mutate then revert — diff against baseline is empty.
    cm.config()["editor"]["tabSize"] = "8";
    cm.config()["editor"]["tabSize"] = "4";
    cm.save(ConfigManager::Category::Config);

    EXPECT_FALSE(wxFileExists(ideDir + "/" + overlayBasename()));
}

TEST_F(ConfigManagerTests, SaveDivergentValueWritesPrunedOverlay) {
    TempDir tmp;
    const auto ideDir = seedBundle(tmp,
        "[editor]\n"
        "tabSize=4\n"
        "theme=dark\n");

    ConfigManager cm(tmp.path(), ideDir, "");
    cm.config()["editor"]["tabSize"] = "8";
    cm.save(ConfigManager::Category::Config);

    const wxString overlayPath = ideDir + "/" + overlayBasename();
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
    const auto ideDir = seedBundle(tmp,
        "[editor]\n"
        "tabSize=4\n");
    // Pre-existing overlay from a previous run that we now revert.
    tmp.write("ide/" + overlayBasename(),
        "[editor]\n"
        "tabSize=8\n");

    ConfigManager cm(tmp.path(), ideDir, "");
    EXPECT_EQ(cm.config().get_or("editor.tabSize", wxString {}), "8");

    cm.config()["editor"]["tabSize"] = "4";
    cm.save(ConfigManager::Category::Config);

    EXPECT_FALSE(wxFileExists(ideDir + "/" + overlayBasename()));
}

TEST_F(ConfigManagerTests, ReadOnlyRoutesOverlayToUserDataDir) {
    TempDir tmp;
    const auto ideDir = seedBundle(tmp,
        "[editor]\n"
        "tabSize=4\n",
        /*readOnly=*/true);

    ConfigManager cm(tmp.path(), ideDir, "", tmp.path() + "/userdata");
    cm.config()["editor"]["tabSize"] = "8";
    cm.save(ConfigManager::Category::Config);

    EXPECT_FALSE(wxFileExists(ideDir + "/" + overlayBasename()));
    EXPECT_TRUE(wxFileExists(tmp.path() + "/userdata/" + overlayBasename()));
}

TEST_F(ConfigManagerTests, ReadOnlyLoadsOverlayFromUserDataDir) {
    // Round-trip: write an overlay into the user dir, construct
    // ConfigManager pointing at the same dir, confirm the overlay is
    // picked up during load (not the absent bundle-adjacent one).
    TempDir tmp;
    const auto ideDir = seedBundle(tmp,
        "[editor]\n"
        "tabSize=4\n",
        /*readOnly=*/true);
    tmp.write("userdata/" + overlayBasename(),
        "[editor]\n"
        "tabSize=12\n");

    ConfigManager cm(tmp.path(), ideDir, "", tmp.path() + "/userdata");
    EXPECT_EQ(cm.config().get_or("editor.tabSize", wxString { "<missing>" }), "12");
}

// ---------------------------------------------------------------------------
// Theme two-dir enumeration + path resolution (READONLY only)
// ---------------------------------------------------------------------------

TEST_F(ConfigManagerTests, GetAllThemesPortableEnumeratesBundleOnly) {
    TempDir tmp;
    const auto ideDir = seedBundle(tmp);
    tmp.write("ide/themes/dark.ini", "[Default]\n");
    tmp.write("ide/themes/light.ini", "[Default]\n");
    // userdata exists but no READONLY — its themes/ must not contribute.
    tmp.write("userdata/themes/ignored.ini", "[Default]\n");

    ConfigManager cm(tmp.path(), ideDir, "", tmp.path() + "/userdata");

    const auto themes = cm.getAllThemes();
    ASSERT_EQ(themes.size(), 2);
    EXPECT_EQ(wxFileName(themes[0]).GetFullName(), "dark.ini");
    EXPECT_EQ(wxFileName(themes[1]).GetFullName(), "light.ini");
}

TEST_F(ConfigManagerTests, GetAllThemesReadOnlyMergesBundleAndUserDirs) {
    TempDir tmp;
    const auto ideDir = seedBundle(tmp, "\n", /*readOnly=*/true);
    tmp.write("ide/themes/dark.ini", "[Default]\n");
    tmp.write("ide/themes/light.ini", "[Default]\n");
    tmp.write("userdata/themes/solarized.ini", "[Default]\n");

    ConfigManager cm(tmp.path(), ideDir, "", tmp.path() + "/userdata");

    const auto themes = cm.getAllThemes();
    ASSERT_EQ(themes.size(), 3);
    // Sorted by basename across both dirs.
    EXPECT_EQ(wxFileName(themes[0]).GetFullName(), "dark.ini");
    EXPECT_EQ(wxFileName(themes[1]).GetFullName(), "light.ini");
    EXPECT_EQ(wxFileName(themes[2]).GetFullName(), "solarized.ini");
}

TEST_F(ConfigManagerTests, GetAllThemesUserDirWinsOnBasenameCollision) {
    TempDir tmp;
    const auto ideDir = seedBundle(tmp, "\n", /*readOnly=*/true);
    tmp.write("ide/themes/dark.ini", "[Default]\n");
    tmp.write("userdata/themes/dark.ini", "[Default]\n");

    ConfigManager cm(tmp.path(), ideDir, "", tmp.path() + "/userdata");

    const auto themes = cm.getAllThemes();
    ASSERT_EQ(themes.size(), 1);
    EXPECT_EQ(wxFileName(themes[0]).GetPath(), tmp.path() + "/userdata/themes");
}

TEST_F(ConfigManagerTests, ThemePathReadOnlyPrefersUserOverride) {
    TempDir tmp;
    const auto ideDir = seedBundle(tmp, "\n", /*readOnly=*/true);
    tmp.write("ide/themes/dark.ini", "[Default]\n");
    tmp.write("userdata/themes/dark.ini", "[Default]\n");

    ConfigManager cm(tmp.path(), ideDir, "", tmp.path() + "/userdata");

    EXPECT_EQ(
        wxFileName(cm.themePath("themes/dark.ini")).GetPath(),
        tmp.path() + "/userdata/themes"
    );
}

TEST_F(ConfigManagerTests, ThemePathReadOnlyFallsBackToBundleWhenUserMissing) {
    TempDir tmp;
    const auto ideDir = seedBundle(tmp, "\n", /*readOnly=*/true);
    tmp.write("ide/themes/dark.ini", "[Default]\n");

    ConfigManager cm(tmp.path(), ideDir, "", tmp.path() + "/userdata");

    EXPECT_EQ(
        wxFileName(cm.themePath("themes/dark.ini")).GetPath(),
        ideDir + "/themes"
    );
}

TEST_F(ConfigManagerTests, ThemePathPortableIgnoresUserDir) {
    TempDir tmp;
    const auto ideDir = seedBundle(tmp);
    tmp.write("ide/themes/dark.ini", "[Default]\n");
    tmp.write("userdata/themes/dark.ini", "[Default]\n");

    // No READONLY sentinel → user dir is ignored even though it exists.
    ConfigManager cm(tmp.path(), ideDir, "", tmp.path() + "/userdata");

    EXPECT_EQ(
        wxFileName(cm.themePath("themes/dark.ini")).GetPath(),
        ideDir + "/themes"
    );
}

TEST_F(ConfigManagerTests, SaveLocaleIsRefusedAndLeavesBundleFileUntouched) {
    // Locale is bundle-only — no code path should ever ask the IDE to
    // write it. `save(Locale)` must refuse rather than truncate the
    // shipped locale file (which would happen under portable mode where
    // the bundle dir is writable). Asserts both branches: in-memory
    // mutation does not reach disk, and the on-disk file is byte-for-
    // byte unchanged.
    TempDir tmp;
    const auto ideDir = seedBundle(tmp, "locale=locales/en.ini\n");
    tmp.write("ide/locales/en.ini",
        "[strings]\n"
        "greeting=Hello\n");

    ConfigManager cm(tmp.path(), ideDir, "");
    ASSERT_EQ(cm.locale().get_or("strings.greeting", wxString {}), "Hello");

    // Mutate in-memory then attempt to persist. For any other category
    // this would round-trip to disk; for Locale we expect a refusal.
    cm.locale()["strings"]["greeting"] = "Bonjour";
    cm.save(ConfigManager::Category::Locale);

    wxFFileInputStream roundTripStream(ideDir + "/locales/en.ini");
    wxFileConfig roundTrip(roundTripStream, wxConvUTF8);
    wxString value;
    ASSERT_TRUE(roundTrip.Read("strings/greeting", &value));
    EXPECT_EQ(value, "Hello");
}

TEST_F(ConfigManagerTests, RelativeOfUserDataPathStripsUserDataPrefix) {
    // Regression: under READONLY, `themesWriteDir()` returns
    // `<UserDataDir>/themes/`, so a theme saved there yields an absolute
    // path. `relative()` must strip the `m_userDataDir` prefix the same
    // way it strips `m_ideDir` — otherwise `config["theme"]` ends up
    // storing the full absolute path and leaks the user's home dir
    // into the overlay file.
    TempDir tmp;
    const auto ideDir = seedBundle(tmp, "\n", /*readOnly=*/true);
    tmp.write("userdata/themes/modern-dark.ini", "[Default]\n");

    ConfigManager cm(tmp.path(), ideDir, "", tmp.path() + "/userdata");

    EXPECT_EQ(
        cm.relative(tmp.path() + "/userdata/themes/modern-dark.ini"),
        "themes/modern-dark.ini"
    );
}

TEST_F(ConfigManagerTests, ThemesWriteDirRoutesByReadOnly) {
    // Portable → bundle themes dir; READONLY → user themes dir. This is
    // the rule ThemePage::onSaveTheme uses to redirect save targets so
    // edits to a bundle-shipped theme land in the user-writable copy.
    TempDir tmp;
    const auto ideDir = seedBundle(tmp);

    // Portable
    {
        ConfigManager cm(tmp.path(), ideDir, "");
        EXPECT_EQ(cm.themesWriteDir(), ideDir + "/themes");
    }

    // READONLY routes to user dir
    tmp.write("ide/READONLY", "");
    {
        ConfigManager cm(tmp.path(), ideDir, "", tmp.path() + "/userdata");
        EXPECT_EQ(cm.themesWriteDir(), tmp.path() + "/userdata/themes");
    }
}

TEST_F(ConfigManagerTests, ReloadConfigCascadesSubCategoriesToDirectMode) {
    // Default-boot start with overlay-mode keywords. After
    // `reloadConfig(PATH)` flips to explicit mode, mutating and saving
    // keywords must hit the base file directly — no `.local.ini` is
    // produced, because the sub-category strategy was rebuilt under
    // the new mode.
    TempDir tmp;
    const auto ideDir = seedBundle(tmp, "keywords=keywords.ini\n");
    tmp.write("ide/keywords.ini",
        "[functions]\n"
        "Print=1\n");
    tmp.write("explicit.ini", "keywords=alt_keywords.ini\n");
    tmp.write("alt_keywords.ini",
        "[functions]\n"
        "Foo=1\n");

    ConfigManager cm(tmp.path(), ideDir, "");
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
    const auto ideDir = seedBundle(tmp, "keywords=keywords.ini\n");
    tmp.write("ide/keywords.ini",
        "[functions]\n"
        "Print=1\n");
    tmp.write("ide/alt_keywords.ini",
        "[functions]\n"
        "Alt=1\n");

    ConfigManager cm(tmp.path(), ideDir, "");
    cm.setCategoryPath(ConfigManager::Category::Keywords, ideDir + "/alt_keywords.ini");

    EXPECT_EQ(cm.keywords().get_or("functions.Alt", wxString { "<missing>" }), "1");
    EXPECT_EQ(cm.keywords().get_or("functions.Print", wxString { "<missing>" }), "<missing>");

    cm.keywords()["functions"]["Bar"] = "1";
    cm.save(ConfigManager::Category::Keywords);

    // Overlay mode inherits → `.local.ini` lands next to the new base.
    EXPECT_TRUE(wxFileExists(ideDir + "/alt_keywords.local.ini"));
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
