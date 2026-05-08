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
        "keywords=does_not_exist.ini\n"
    );

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
        "shortcuts=does_not_exist.ini\n"
    );

    ConfigManager cm(tmp.path(), tmp.path(), "config.ini");
    auto& sc = cm.shortcuts();
    EXPECT_EQ(sc.get_or("file.open", wxString { "<sentinel>" }), "<sentinel>");
}

TEST_F(ConfigManagerTests, MissingLocaleLoadsEmpty) {
    TempDir tmp;
    tmp.write("config.ini",
        "version=0.5.0\n"
        "locale=does_not_exist.ini\n"
    );

    ConfigManager cm(tmp.path(), tmp.path(), "config.ini");
    auto& loc = cm.locale();
    EXPECT_EQ(loc.get_or("dialogs.settings.title", wxString { "<sentinel>" }), "<sentinel>");
}

// ---------------------------------------------------------------------------
// Theme fallback — `m_theme.loadDefaults()` runs when the configured
// theme file is missing or absent from `[theme]`.
// ---------------------------------------------------------------------------

TEST_F(ConfigManagerTests, MissingThemeFileTriggersDefaults) {
    TempDir tmp;
    tmp.write("config.ini",
        "version=0.5.0\n"
        "theme=themes/does_not_exist.ini\n"
    );

    ConfigManager cm(tmp.path(), tmp.path(), "config.ini");
    const auto& theme = cm.getTheme();
    EXPECT_EQ(theme.get(ThemeCategory::Default).colors.foreground, *wxBLACK);
    EXPECT_EQ(theme.get(ThemeCategory::Default).colors.background, wxColour(30, 30, 30));
}

TEST_F(ConfigManagerTests, MissingThemeEntryTriggersDefaults) {
    TempDir tmp;
    // No `theme=` key at all — fallback kicks in just the same.
    tmp.write("config.ini", "version=0.5.0\n");

    ConfigManager cm(tmp.path(), tmp.path(), "config.ini");
    const auto& theme = cm.getTheme();
    EXPECT_EQ(theme.get(ThemeCategory::Default).colors.foreground, *wxBLACK);
    EXPECT_EQ(theme.get(ThemeCategory::Default).colors.background, wxColour(30, 30, 30));
}
