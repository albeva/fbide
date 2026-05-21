//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "config/ConfigStrategy.hpp"

using namespace fbide;

class ConfigStrategyTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Overlay strategy — bundle baseline + writable user overlay
// ---------------------------------------------------------------------------

TEST_F(ConfigStrategyTests, OverlayCarriesBothPaths) {
    const auto strat = ConfigStrategy::overlay("/ide/config.ini", "/user/config.local.ini");
    EXPECT_EQ(strat.basePath(), "/ide/config.ini");
    EXPECT_EQ(strat.overlayPath(), "/user/config.local.ini");
}

TEST_F(ConfigStrategyTests, OverlaySavesToOverlayPath) {
    // Saves under the Overlay strategy must land in the writable .local.ini,
    // never in the immutable bundle file. savePath() encodes that rule so
    // ConfigManager::save() can stay agnostic of strategy mode.
    const auto strat = ConfigStrategy::overlay("/ide/config.ini", "/user/config.local.ini");
    EXPECT_EQ(strat.savePath(), strat.overlayPath());
}

TEST_F(ConfigStrategyTests, OverlayUsesOverlay) {
    const auto strat = ConfigStrategy::overlay("/ide/config.ini", "/user/config.local.ini");
    EXPECT_TRUE(strat.usesOverlay());
}

// ---------------------------------------------------------------------------
// Direct strategy — single file, no layering (--config=PATH at startup, or
// reloadConfig() at runtime)
// ---------------------------------------------------------------------------

TEST_F(ConfigStrategyTests, DirectCarriesOnlyBasePath) {
    const auto strat = ConfigStrategy::direct("/tmp/ci-config.ini");
    EXPECT_EQ(strat.basePath(), "/tmp/ci-config.ini");
    EXPECT_TRUE(strat.overlayPath().IsEmpty());
}

TEST_F(ConfigStrategyTests, DirectSavesToBasePath) {
    // Explicit-path mode round-trips: load and save target the same file. No
    // overlay file is ever produced.
    const auto strat = ConfigStrategy::direct("/tmp/ci-config.ini");
    EXPECT_EQ(strat.savePath(), strat.basePath());
}

TEST_F(ConfigStrategyTests, DirectDoesNotUseOverlay) {
    const auto strat = ConfigStrategy::direct("/tmp/ci-config.ini");
    EXPECT_FALSE(strat.usesOverlay());
}

// ---------------------------------------------------------------------------
// Value-type behaviour — must be copyable / movable so ConfigManager can
// keep a strategy per Entry without ceremony.
// ---------------------------------------------------------------------------

TEST_F(ConfigStrategyTests, IsCopyable) {
    const auto original = ConfigStrategy::overlay("/a", "/b");
    const auto copy = original;
    EXPECT_EQ(copy.basePath(), "/a");
    EXPECT_EQ(copy.overlayPath(), "/b");
    EXPECT_TRUE(copy.usesOverlay());
}

// ---------------------------------------------------------------------------
// deriveOverlayPath — pure path transform. Inserts ".local" before the
// extension; routes to <userDataDir> when READONLY, else stays next to base.
// ---------------------------------------------------------------------------

TEST_F(ConfigStrategyTests, DeriveOverlayPathPortableLivesNextToBase) {
    const wxString base = "/ide/config_macos.ini";
    const auto path = ConfigStrategy::deriveOverlayPath(base, "/anything", /*readOnly=*/false);
    EXPECT_EQ(wxFileName(path).GetFullName(), "config_macos.local.ini");
    EXPECT_EQ(wxFileName(path).GetPath(), wxFileName(base).GetPath());
}

TEST_F(ConfigStrategyTests, DeriveOverlayPathReadOnlyRoutesToUserDir) {
    const wxString base = "/ide/config_macos.ini";
    const wxString userDataDir = "/user/Library/Application Support/fbide";
    const auto path = ConfigStrategy::deriveOverlayPath(base, userDataDir, /*readOnly=*/true);
    EXPECT_EQ(wxFileName(path).GetFullName(), "config_macos.local.ini");
    EXPECT_EQ(wxFileName(path).GetPath(), userDataDir);
}

TEST_F(ConfigStrategyTests, DeriveOverlayPathKeywordsFile) {
    const auto path = ConfigStrategy::deriveOverlayPath("/ide/keywords.ini", "", false);
    EXPECT_EQ(wxFileName(path).GetFullName(), "keywords.local.ini");
}

TEST_F(ConfigStrategyTests, DeriveOverlayPathPreservesExtension) {
    // Theme ".ini" today; ".fbt" legacy. Either should round-trip with
    // ".local" inserted before the extension, not appended after it.
    const auto fbt = ConfigStrategy::deriveOverlayPath("/themes/dark.fbt", "", false);
    EXPECT_EQ(wxFileName(fbt).GetFullName(), "dark.local.fbt");
}

// ---------------------------------------------------------------------------
// select — top-level rule. Two axes (explicitMode, overlayCapable) collapse
// to Direct; only the (overlayCapable && !explicitMode) cell is Overlay.
// ---------------------------------------------------------------------------

TEST_F(ConfigStrategyTests, SelectBundleOnlyCategoryAlwaysDirect) {
    // overlayCapable=false → locale (and any other bundle-only slot).
    // Direct regardless of explicitMode / readOnly.
    const auto strat = ConfigStrategy::select(
        "/ide/locales/en.ini", "/user", /*readOnly=*/true,
        /*overlayCapable=*/false, /*explicitMode=*/false
    );
    EXPECT_FALSE(strat.usesOverlay());
    EXPECT_EQ(strat.basePath(), "/ide/locales/en.ini");
}

TEST_F(ConfigStrategyTests, SelectExplicitModeForcesDirect) {
    // --config=PATH (or runtime reloadConfig) → Direct for every category.
    // Reproducibility/CI rule: no overlays sneak in on top of an explicit
    // single-file config.
    const auto strat = ConfigStrategy::select(
        "/tmp/ci.ini", "/user", /*readOnly=*/true,
        /*overlayCapable=*/true, /*explicitMode=*/true
    );
    EXPECT_FALSE(strat.usesOverlay());
    EXPECT_EQ(strat.basePath(), "/tmp/ci.ini");
}

TEST_F(ConfigStrategyTests, SelectDefaultBootPortableProducesOverlayNextToBase) {
    const wxString base = "/ide/config_macos.ini";
    const auto strat = ConfigStrategy::select(
        base, "/user", /*readOnly=*/false,
        /*overlayCapable=*/true, /*explicitMode=*/false
    );
    EXPECT_TRUE(strat.usesOverlay());
    EXPECT_EQ(strat.basePath(), base);
    EXPECT_EQ(wxFileName(strat.overlayPath()).GetFullName(), "config_macos.local.ini");
    EXPECT_EQ(wxFileName(strat.overlayPath()).GetPath(), wxFileName(base).GetPath());
}

TEST_F(ConfigStrategyTests, SelectDefaultBootReadOnlyRoutesOverlayToUserDir) {
    const wxString base = "/ide/config_macos.ini";
    const wxString userDataDir = "/user/Library/Application Support/fbide";
    const auto strat = ConfigStrategy::select(
        base, userDataDir, /*readOnly=*/true,
        /*overlayCapable=*/true, /*explicitMode=*/false
    );
    EXPECT_TRUE(strat.usesOverlay());
    EXPECT_EQ(strat.basePath(), base);
    EXPECT_EQ(wxFileName(strat.overlayPath()).GetFullName(), "config_macos.local.ini");
    EXPECT_EQ(wxFileName(strat.overlayPath()).GetPath(), userDataDir);
}
