//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <array>
#include <filesystem>
#include <optional>
#include <vector>
#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <gtest/gtest.h>
#include "compiler/FbcAutoDetect.hpp"
#include "config/Value.hpp"
#include "utils/PathConversions.hpp"

using namespace fbide;

#ifdef __WXMSW__

namespace {
/// RAII scratch directory. `touch` drops empty files used as stand-in fbc
/// binaries — detection only checks existence; the version probe is stubbed.
/// Kept local so test files don't bind together via a shared helper header.
class TempDir final {
public:
    TempDir() {
        const auto base = wxFileName::CreateTempFileName("fbide_fbc_test");
        wxRemoveFile(base);
        wxFileName::Mkdir(base, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        m_path = base;
    }
    ~TempDir() {
        if (!m_path.IsEmpty() && wxDirExists(m_path)) {
            wxFileName::Rmdir(m_path, wxPATH_RMDIR_RECURSIVE);
        }
    }
    TempDir(const TempDir&) = delete;
    auto operator=(const TempDir&) -> TempDir& = delete;
    TempDir(TempDir&&) = delete;
    auto operator=(TempDir&&) -> TempDir& = delete;

    [[nodiscard]] auto fsPath() const -> std::filesystem::path { return toFsPath(m_path); }

    void touch(const wxString& name) const {
        wxFile file;
        file.Create(m_path + "/" + name, true);
    }

private:
    wxString m_path;
};
} // namespace

// ---------------------------------------------------------------------------
// parseArch
// ---------------------------------------------------------------------------
TEST(FbcAutoDetectTests, ParseArchReadsWin64) {
    EXPECT_EQ(
        FbcAutoDetect::parseArch("FreeBASIC Compiler - Version 1.10.1 (2024-01-25), built for win64 (64bit)"),
        std::optional { FbcArch::Win64 }
    );
}

TEST(FbcAutoDetectTests, ParseArchReadsWin32) {
    EXPECT_EQ(
        FbcAutoDetect::parseArch("FreeBASIC Compiler - Version 1.07.1, for win32"),
        std::optional { FbcArch::Win32 }
    );
}

TEST(FbcAutoDetectTests, ParseArchUnknownReturnsNullopt) {
    EXPECT_FALSE(FbcAutoDetect::parseArch("no architecture marker here").has_value());
}

// ---------------------------------------------------------------------------
// detectVariants
// ---------------------------------------------------------------------------
TEST(FbcAutoDetectTests, DetectVariantsNamedBinariesWinAndDedup) {
    const TempDir tmp;
    tmp.touch("fbc.exe");
    tmp.touch("fbc32.exe");
    tmp.touch("fbc64.exe");
    // Plain fbc.exe also reports win64, but fbc64.exe already covers Win64,
    // so the plain binary is dropped.
    const auto probe = [](const std::filesystem::path&) -> wxString { return "built for win64 (64bit)"; };

    const auto variants = FbcAutoDetect::detectVariants(tmp.fsPath(), probe);

    ASSERT_EQ(variants.size(), 2U);
    EXPECT_EQ(variants.at(0).arch, FbcArch::Win64);
    EXPECT_EQ(variants.at(0).exe.filename(), "fbc64.exe");
    EXPECT_EQ(variants.at(1).arch, FbcArch::Win32);
    EXPECT_EQ(variants.at(1).exe.filename(), "fbc32.exe");
}

TEST(FbcAutoDetectTests, DetectVariantsLonePlainUsesProbedArch) {
    const TempDir tmp;
    tmp.touch("fbc.exe");
    const auto probe = [](const std::filesystem::path&) -> wxString { return "for win32 (32bit)"; };

    const auto variants = FbcAutoDetect::detectVariants(tmp.fsPath(), probe);

    ASSERT_EQ(variants.size(), 1U);
    EXPECT_EQ(variants.at(0).arch, FbcArch::Win32);
    EXPECT_EQ(variants.at(0).exe.filename(), "fbc.exe");
}

TEST(FbcAutoDetectTests, DetectVariantsSkipsNonRunnableBinary) {
    const TempDir tmp;
    tmp.touch("fbc64.exe");
    // Empty version output means the binary could not be run.
    const auto probe = [](const std::filesystem::path&) -> wxString { return {}; };

    EXPECT_TRUE(FbcAutoDetect::detectVariants(tmp.fsPath(), probe).empty());
}

TEST(FbcAutoDetectTests, DetectVariantsEmptyFolder) {
    const TempDir tmp;
    const auto probe = [](const std::filesystem::path&) -> wxString { return "win64"; };
    EXPECT_TRUE(FbcAutoDetect::detectVariants(tmp.fsPath(), probe).empty());
}

// ---------------------------------------------------------------------------
// buildCompilerValue
// ---------------------------------------------------------------------------
TEST(FbcAutoDetectTests, BuildBothArchesOn64BitOs) {
    const std::filesystem::path exe32 { "C:/fb/fbc32.exe" };
    const std::filesystem::path exe64 { "C:/fb/fbc64.exe" };
    const std::vector<FbcVariant> variants {
        { .exe = exe32, .arch = FbcArch::Win32 },
        { .exe = exe64, .arch = FbcArch::Win64 },
    };

    const auto compiler = FbcAutoDetect::buildCompilerValue(variants, /*osIs64=*/true);

    // Canonical Default: OS-arch binary, generic command, hidden from menu.
    EXPECT_EQ(compiler.get_or("path", wxString {}), toWxString(exe64));
    EXPECT_EQ(compiler.get_or("compileCommand", wxString {}), R"("<$fbc>" "<$file>")");
    EXPECT_EQ(compiler.get_or("runCommand", wxString {}), R"("<$file>" <$param>)");
    EXPECT_EQ(compiler.get_or("terminal", wxString {}), "cmd /C");
    EXPECT_FALSE(compiler.get_or("showInMenu", true));
    EXPECT_EQ(compiler.get_or("active", wxString {}), "cfg-4");
    EXPECT_EQ(compiler.get_or("nextSlugIndex", 0), 5);

    // Four configs: Win32 pair, then Win64 pair.
    EXPECT_EQ(compiler.at("cfg-1").get_or("name", wxString {}), "Win32 GUI");
    EXPECT_EQ(compiler.at("cfg-1").get_or("compileCommand", wxString {}), R"("<$fbc>" -target win32 -s gui "<$file>")");
    EXPECT_EQ(compiler.at("cfg-1").get_or("path", wxString {}), toWxString(exe32));
    EXPECT_TRUE(compiler.at("cfg-1").get_or("showInMenu", false));
    // Run command + terminal are left to inherit from the Default.
    EXPECT_FALSE(compiler.at("cfg-1").contains("runCommand"));
    EXPECT_FALSE(compiler.at("cfg-1").contains("terminal"));

    EXPECT_EQ(compiler.at("cfg-2").get_or("name", wxString {}), "Win32 Console");
    EXPECT_EQ(compiler.at("cfg-2").get_or("compileCommand", wxString {}), R"("<$fbc>" -target win32 -s console "<$file>")");
    EXPECT_EQ(compiler.at("cfg-3").get_or("name", wxString {}), "Win64 GUI");
    EXPECT_EQ(compiler.at("cfg-4").get_or("name", wxString {}), "Win64 Console");
    EXPECT_EQ(compiler.at("cfg-4").get_or("path", wxString {}), toWxString(exe64));
}

TEST(FbcAutoDetectTests, BuildLoneWin32FallsBackForActiveAndPath) {
    const std::filesystem::path exe32 { "C:/fb/fbc32.exe" };
    const std::vector<FbcVariant> variants { { .exe = exe32, .arch = FbcArch::Win32 } };

    const auto compiler = FbcAutoDetect::buildCompilerValue(variants, /*osIs64=*/true);

    // 64-bit OS but only a 32-bit compiler: Default + active fall back to Win32.
    EXPECT_EQ(compiler.get_or("path", wxString {}), toWxString(exe32));
    EXPECT_EQ(compiler.get_or("active", wxString {}), "cfg-2");
    EXPECT_EQ(compiler.get_or("nextSlugIndex", 0), 3);
    EXPECT_EQ(compiler.at("cfg-2").get_or("name", wxString {}), "Win32 Console");
    EXPECT_FALSE(compiler.contains("cfg-3"));
}

#endif // __WXMSW__
