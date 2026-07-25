//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filename.h>
#include <gtest/gtest.h>
#include "config/ConfigManager.hpp"
#include "format/FormatSettings.hpp"

using namespace fbide;

namespace {
/// Scratch dir seeded with a minimal `config.ini`; auto-removed. A fresh
/// `ConfigManager` is built from `path()` so the `[format]` section can be
/// saved and re-read through the real config store.
class ConfigScratch final {
public:
    ConfigScratch() {
        const auto base = wxFileName::CreateTempFileName("fbide_fmt_test");
        wxRemoveFile(base);
        wxFileName::Mkdir(base, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        m_path = base;
        wxFFile out(m_path + "/config.ini", "w");
        out.Write("version=0.5.0\n");
    }
    ~ConfigScratch() {
        if (!m_path.IsEmpty() && wxDirExists(m_path)) {
            wxFileName::Rmdir(m_path, wxPATH_RMDIR_RECURSIVE);
        }
    }
    ConfigScratch(const ConfigScratch&) = delete;
    auto operator=(const ConfigScratch&) -> ConfigScratch& = delete;
    ConfigScratch(ConfigScratch&&) = delete;
    auto operator=(ConfigScratch&&) -> ConfigScratch& = delete;

    [[nodiscard]] auto path() const -> const wxString& { return m_path; }

private:
    wxString m_path;
};
} // namespace

// NOLINTNEXTLINE(misc-use-internal-linkage)
class FormatSettingsTests : public testing::Test {};

TEST_F(FormatSettingsTests, DefaultsWhenUnset) {
    const ConfigScratch scratch;
    ConfigManager cm(scratch.path(), scratch.path(), "config.ini");

    const auto settings = FormatSettings::load(cm);
    EXPECT_TRUE(settings.reIndent);
    EXPECT_TRUE(settings.reFormat);
    EXPECT_FALSE(settings.alignPP);
    EXPECT_FALSE(settings.applyCase);
}

TEST_F(FormatSettingsTests, SaveThenReloadRoundTrips) {
    const ConfigScratch scratch;

    // Save non-default options, flushing the overlay to disk.
    {
        ConfigManager cm(scratch.path(), scratch.path(), "config.ini");
        const FormatSettings saved { .reIndent = false, .reFormat = true, .alignPP = true, .applyCase = true };
        saved.save(cm);
    }

    // A fresh manager reads the persisted overlay back.
    ConfigManager cm(scratch.path(), scratch.path(), "config.ini");
    const auto loaded = FormatSettings::load(cm);
    EXPECT_FALSE(loaded.reIndent);
    EXPECT_TRUE(loaded.reFormat);
    EXPECT_TRUE(loaded.alignPP);
    EXPECT_TRUE(loaded.applyCase);
}
