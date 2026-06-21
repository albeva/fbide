//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <filesystem>
#include <optional>
#include <vector>
#include <gtest/gtest.h>
#include "compiler/FbcAutoDetect.hpp"
#include "config/Value.hpp"
#include "utils/PathConversions.hpp"

using namespace fbide;

#ifdef __WXMSW__

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
// parseVersion
// ---------------------------------------------------------------------------
TEST(FbcAutoDetectTests, ParseVersionReadsThreePart) {
    EXPECT_EQ(
        FbcAutoDetect::parseVersion("FreeBASIC Compiler - Version 1.10.1 (2024-01-25), built for win64 (64bit)"),
        std::optional<wxString> { "1.10.1" }
    );
}

TEST(FbcAutoDetectTests, ParseVersionStopsAtComma) {
    EXPECT_EQ(
        FbcAutoDetect::parseVersion("FreeBASIC Compiler - Version 1.07.1, for win32"),
        std::optional<wxString> { "1.07.1" }
    );
}

TEST(FbcAutoDetectTests, ParseVersionUnknownReturnsNullopt) {
    EXPECT_FALSE(FbcAutoDetect::parseVersion("no version marker here").has_value());
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
