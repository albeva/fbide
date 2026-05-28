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
#include "compiler/CompilerConfigCatalog.hpp"
#include "config/ConfigManager.hpp"

using namespace fbide;

namespace {
/// RAII scratch directory + helper to seed an `ide/` bundle with a single
/// platform-config INI file. Mirrors the pattern used in
/// `ConfigManagerTests` — kept local so the two test files don't share a
/// helper header and accidentally bind together.
class TempDir final {
public:
    TempDir() {
        const auto base = wxFileName::CreateTempFileName("fbide_catalog_test");
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

    [[nodiscard]] auto path() const -> const wxString& { return m_path; }

    void write(const wxString& relPath, const wxString& contents) const {
        const wxString full = m_path + "/" + relPath;
        const wxFileName fn(full);
        wxFileName::Mkdir(fn.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        wxFFile out(full, "w");
        out.Write(contents);
    }

private:
    wxString m_path;
};

/// Build a ConfigManager whose `[compiler]` section is whatever the test
/// drops into the INI. The bundle layout exactly matches
/// `seedBundle(...)` in `ConfigManagerTests`.
auto makeConfig(const TempDir& tmp, const wxString& compilerIni) -> std::unique_ptr<ConfigManager> {
    const wxString configContents = wxString("version=0.5.0\n") + compilerIni;
    tmp.write("ide/" + ConfigManager::getPlatformConfigFileName(), configContents);
    const auto ideDir = tmp.path() + "/ide";
    return std::make_unique<ConfigManager>(tmp.path(), ideDir, "");
}
} // namespace

// gtest fixtures are referenced by TEST_F macro expansion and idiomatically
// stay at file scope.
// NOLINTNEXTLINE(misc-use-internal-linkage)
class CompilerConfigCatalogTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Canonical-only configuration
// ---------------------------------------------------------------------------
TEST_F(CompilerConfigCatalogTests, CanonicalOnlyExposesSingleEntry) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "path=/opt/fbc/bin/fbc\n"
        "runCommand=run-template\n"
        "compileCommand=compile-template\n"
        "terminal=term-launcher\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    EXPECT_EQ(catalog.all().size(), 1U);
    EXPECT_EQ(catalog.canonical().slug, "default");
    EXPECT_EQ(catalog.canonical().path, std::filesystem::path { "/opt/fbc/bin/fbc" });
    EXPECT_EQ(catalog.canonical().runCommand, "run-template");
    EXPECT_EQ(catalog.canonical().compileCommand, "compile-template");
    EXPECT_EQ(catalog.canonical().terminal, "term-launcher");
    EXPECT_EQ(catalog.find("default"), &catalog.canonical());
    EXPECT_EQ(catalog.find("cfg-1"), nullptr);
}

// ---------------------------------------------------------------------------
// User config with overrides — keys present on the child take precedence
// over canonical values for the corresponding field.
// ---------------------------------------------------------------------------
TEST_F(CompilerConfigCatalogTests, UserOverridesReplaceInheritedFields) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "path=/opt/fbc/bin/fbc\n"
        "runCommand=<$file>\n"
        "compileCommand=base\n"
        "terminal=base-term\n"
        "[compiler/cfg-1]\n"
        "name=FBC 32bit\n"
        "compileCommand=override\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    ASSERT_EQ(catalog.all().size(), 2U);
    const auto* cfg = catalog.find("cfg-1");
    ASSERT_NE(cfg, nullptr);
    EXPECT_EQ(cfg->displayName, "FBC 32bit");
    EXPECT_EQ(cfg->compileCommand, "override");
    // Un-overridden fields fall through to canonical.
    EXPECT_EQ(cfg->path, std::filesystem::path { "/opt/fbc/bin/fbc" });
    EXPECT_EQ(cfg->runCommand, "<$file>");
    EXPECT_EQ(cfg->terminal, "base-term");
}

// ---------------------------------------------------------------------------
// Key-present-but-empty is an override — distinguishable from key-absent.
// ---------------------------------------------------------------------------
TEST_F(CompilerConfigCatalogTests, EmptyValueCountsAsOverride) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "terminal=base-term\n"
        "[compiler/cfg-1]\n"
        "name=Quiet\n"
        "terminal=\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    const auto* cfg = catalog.find("cfg-1");
    ASSERT_NE(cfg, nullptr);
    EXPECT_TRUE(cfg->terminal.IsEmpty()) << "key present with empty value should override";
}

// ---------------------------------------------------------------------------
// A missing key on the canonical section falls through to the hard-coded
// platform template — same fallback the legacy `CompileCommand::build(ctx)`
// applied via `get_or(..., default)`. Empty-but-present is *not* replaced
// (covered by `EmptyValueCountsAsOverride`).
// ---------------------------------------------------------------------------
TEST_F(CompilerConfigCatalogTests, CanonicalAbsentKeyAppliesDefaultTemplate) {
    const TempDir tmp;
    auto cm = makeConfig(tmp, "[compiler]\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    EXPECT_FALSE(catalog.canonical().compileCommand.IsEmpty());
    EXPECT_FALSE(catalog.canonical().runCommand.IsEmpty());
    EXPECT_TRUE(catalog.canonical().compileCommand.Contains("<$fbc>"));
    EXPECT_TRUE(catalog.canonical().runCommand.Contains("<$file>"));
}

// ---------------------------------------------------------------------------
// Chain resolution: cfg-2 → cfg-1 → canonical. Each level contributes only
// the fields it overrides; everything else falls through.
// ---------------------------------------------------------------------------
TEST_F(CompilerConfigCatalogTests, ChainResolvesThroughMultipleLevels) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "path=canonical-path\n"
        "runCommand=canonical-run\n"
        "compileCommand=canonical-compile\n"
        "terminal=canonical-term\n"
        "[compiler/cfg-1]\n"
        "name=FBC 32bit\n"
        "path=cfg1-path\n"
        "[compiler/cfg-2]\n"
        "name=FBC 32bit GUI\n"
        "base=cfg-1\n"
        "compileCommand=cfg2-compile\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    const auto* cfg2 = catalog.find("cfg-2");
    ASSERT_NE(cfg2, nullptr);
    EXPECT_EQ(cfg2->compileCommand, "cfg2-compile"); // from cfg-2
    EXPECT_EQ(cfg2->path, std::filesystem::path { "cfg1-path" }); // from cfg-1
    EXPECT_EQ(cfg2->runCommand, "canonical-run"); // from canonical
    EXPECT_EQ(cfg2->terminal, "canonical-term"); // from canonical
}

// ---------------------------------------------------------------------------
// Cycle / orphan tolerance — resolution falls back to canonical for any
// field still missing after the walk terminates abnormally. The offending
// configs remain in the catalog so the settings UI can show and fix them.
// ---------------------------------------------------------------------------
TEST_F(CompilerConfigCatalogTests, CycleFallsBackToCanonicalFields) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "compileCommand=canonical-compile\n"
        "[compiler/cfg-1]\n"
        "name=A\n"
        "base=cfg-2\n"
        "[compiler/cfg-2]\n"
        "name=B\n"
        "base=cfg-1\n");

    // wxLogNull silences the cycle warning so the test output stays clean
    // (the warning itself is asserted by spec; the suppression is just
    // hygiene).
    const wxLogNull noLog;
    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    const auto* cfg1 = catalog.find("cfg-1");
    ASSERT_NE(cfg1, nullptr);
    EXPECT_EQ(cfg1->compileCommand, "canonical-compile");
}

TEST_F(CompilerConfigCatalogTests, OrphanBaseFallsBackToCanonicalFields) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "runCommand=canonical-run\n"
        "[compiler/cfg-1]\n"
        "name=Stray\n"
        "base=nonexistent\n");

    const wxLogNull noLog;
    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    const auto* cfg1 = catalog.find("cfg-1");
    ASSERT_NE(cfg1, nullptr);
    EXPECT_EQ(cfg1->runCommand, "canonical-run");
}

// ---------------------------------------------------------------------------
// activeSlug — unset, valid, and invalid all map to the documented behavior.
// ---------------------------------------------------------------------------
TEST_F(CompilerConfigCatalogTests, ActiveSlugDefaultsToCanonicalWhenUnset) {
    const TempDir tmp;
    auto cm = makeConfig(tmp, "[compiler]\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();
    EXPECT_EQ(catalog.activeSlug(), "default");
}

TEST_F(CompilerConfigCatalogTests, ActiveSlugReturnsKnownUserSlug) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "active=cfg-1\n"
        "[compiler/cfg-1]\n"
        "name=Active\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();
    EXPECT_EQ(catalog.activeSlug(), "cfg-1");
}

TEST_F(CompilerConfigCatalogTests, ActiveSlugFallsBackWhenSlugMissing) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "active=cfg-gone\n");

    const wxLogNull noLog;
    CompilerConfigCatalog catalog(*cm);
    catalog.reload();
    EXPECT_EQ(catalog.activeSlug(), "default");
}

// ---------------------------------------------------------------------------
// resolveByPinnedSlug — empty optional follows active; explicit slug pins.
// Missing pinned slug warns and falls back to active. Tested without
// needing a real `Document`: the catalog API takes the optional directly.
// ---------------------------------------------------------------------------
TEST_F(CompilerConfigCatalogTests, ResolveEmptyOptionalFollowsActive) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "active=cfg-1\n"
        "[compiler/cfg-1]\n"
        "name=Active\n"
        "compileCommand=cfg1-compile\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    const auto& resolved = catalog.resolveByPinnedSlug(std::nullopt);
    EXPECT_EQ(resolved.slug, "cfg-1");
    EXPECT_EQ(resolved.compileCommand, "cfg1-compile");
}

TEST_F(CompilerConfigCatalogTests, ResolvePinnedSlugReturnsThatConfig) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "active=cfg-1\n"
        "[compiler/cfg-1]\n"
        "name=A\n"
        "[compiler/cfg-2]\n"
        "name=B\n"
        "compileCommand=cfg2-compile\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    const auto& resolved = catalog.resolveByPinnedSlug(std::optional<wxString> { "cfg-2" });
    EXPECT_EQ(resolved.slug, "cfg-2");
    EXPECT_EQ(resolved.compileCommand, "cfg2-compile");
}

TEST_F(CompilerConfigCatalogTests, ResolveMissingPinnedSlugFallsBackToActive) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "active=cfg-1\n"
        "[compiler/cfg-1]\n"
        "name=Active\n"
        "compileCommand=cfg1-compile\n");

    const wxLogNull noLog;
    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    const auto& resolved = catalog.resolveByPinnedSlug(std::optional<wxString> { "cfg-gone" });
    EXPECT_EQ(resolved.slug, "cfg-1");
    EXPECT_EQ(resolved.compileCommand, "cfg1-compile");
}

// ---------------------------------------------------------------------------
// normalizeForStorage — collapses to nullopt when picked == active, so a
// document re-pinned to the currently-active config becomes "follow active"
// and tracks future active changes instead of locking to a slug.
// ---------------------------------------------------------------------------
TEST_F(CompilerConfigCatalogTests, NormalizeMatchingActiveYieldsNullopt) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "active=cfg-1\n"
        "[compiler/cfg-1]\n"
        "name=Active\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    EXPECT_FALSE(catalog.normalizeForStorage("cfg-1").has_value());
    ASSERT_TRUE(catalog.normalizeForStorage("default").has_value());
    EXPECT_EQ(*catalog.normalizeForStorage("default"), "default");
}

// ---------------------------------------------------------------------------
// all() ordering — canonical first, then user configs sorted by the
// numeric suffix of the cfg-N slug (so cfg-10 sorts after cfg-2).
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// CRUD — create / copy / remove / rename / setBase / setOverride / setActiveSlug
// ---------------------------------------------------------------------------

TEST_F(CompilerConfigCatalogTests, CreateAllocatesSequentialSlugs) {
    const TempDir tmp;
    auto cm = makeConfig(tmp, "[compiler]\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    EXPECT_EQ(catalog.createFromCanonical("First"), "cfg-1");
    EXPECT_EQ(catalog.createFromCanonical("Second"), "cfg-2");
    EXPECT_EQ(cm->config().get_or("compiler.nextSlugIndex", -1), 3);

    ASSERT_NE(catalog.find("cfg-1"), nullptr);
    EXPECT_EQ(catalog.find("cfg-1")->displayName, "First");
}

TEST_F(CompilerConfigCatalogTests, CreateNeverReusesSlugAfterRemove) {
    const TempDir tmp;
    auto cm = makeConfig(tmp, "[compiler]\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();
    catalog.createFromCanonical("First");                 // cfg-1
    catalog.createFromCanonical("Second");                // cfg-2
    catalog.remove("cfg-1");
    EXPECT_EQ(catalog.createFromCanonical("Third"), "cfg-3");
}

TEST_F(CompilerConfigCatalogTests, CopyDuplicatesOverridesAndBase) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "compileCommand=base-compile\n"
        "[compiler/cfg-1]\n"
        "name=Source\n"
        "compileCommand=cfg1-compile\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();
    const auto newSlug = catalog.copy("cfg-1", "Copy");

    const auto* copied = catalog.find(newSlug);
    ASSERT_NE(copied, nullptr);
    EXPECT_EQ(copied->displayName, "Copy");
    EXPECT_EQ(copied->compileCommand, "cfg1-compile");
}

TEST_F(CompilerConfigCatalogTests, RemoveReParentsDependentsToDefault) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "[compiler/cfg-1]\n"
        "name=Parent\n"
        "[compiler/cfg-2]\n"
        "name=Child\n"
        "base=cfg-1\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();
    catalog.remove("cfg-1");

    // cfg-2 keeps existing but its base= key was cleared (inherit from canonical).
    ASSERT_NE(catalog.find("cfg-2"), nullptr);
    EXPECT_FALSE(bool(cm->config().at("compiler.cfg-2.base")))
        << "base= key should be removed (= inherit from canonical default)";
}

TEST_F(CompilerConfigCatalogTests, RemoveClearsActiveWhenItMatched) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "active=cfg-1\n"
        "[compiler/cfg-1]\n"
        "name=Going\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();
    catalog.remove("cfg-1");

    EXPECT_EQ(catalog.activeSlug(), "default");
    EXPECT_FALSE(bool(cm->config().at("compiler.active")));
}

TEST_F(CompilerConfigCatalogTests, SetBaseRejectsSelfAndDescendant) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "[compiler/cfg-1]\n"
        "name=A\n"
        "[compiler/cfg-2]\n"
        "name=B\n"
        "base=cfg-1\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    const wxLogNull noLog;
    EXPECT_FALSE(catalog.setBase("cfg-1", "cfg-1")) << "self-base must be rejected";
    EXPECT_FALSE(catalog.setBase("cfg-1", "cfg-2")) << "descendant-base must be rejected";
    EXPECT_TRUE(catalog.setBase("cfg-2", "default")) << "moving back to canonical is fine";
}

TEST_F(CompilerConfigCatalogTests, SetOverrideNulloptRemovesKey) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "[compiler/cfg-1]\n"
        "name=A\n"
        "compileCommand=override-me\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();
    catalog.setOverride("cfg-1", CompilerField::CompileCommand, std::nullopt);

    EXPECT_FALSE(bool(cm->config().at("compiler.cfg-1.compileCommand")));
}

TEST_F(CompilerConfigCatalogTests, SetActiveSlugDefaultClearsKey) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "active=cfg-1\n"
        "[compiler/cfg-1]\n"
        "name=A\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();
    catalog.setActiveSlug("default");

    EXPECT_FALSE(bool(cm->config().at("compiler.active")));
}

TEST_F(CompilerConfigCatalogTests, ValidBasesExcludesSelfAndDescendants) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "[compiler/cfg-1]\n"
        "name=A\n"
        "[compiler/cfg-2]\n"
        "name=B\n"
        "base=cfg-1\n"
        "[compiler/cfg-3]\n"
        "name=C\n"
        "base=cfg-2\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();
    const auto bases = catalog.validBasesFor("cfg-1");

    // Self (cfg-1) and descendants (cfg-2, cfg-3) are excluded.
    // Default remains a valid choice.
    EXPECT_NE(std::ranges::find(bases, "default"), bases.end());
    EXPECT_EQ(std::ranges::find(bases, "cfg-1"), bases.end());
    EXPECT_EQ(std::ranges::find(bases, "cfg-2"), bases.end());
    EXPECT_EQ(std::ranges::find(bases, "cfg-3"), bases.end());
}

TEST_F(CompilerConfigCatalogTests, AllOrdersCanonicalFirstThenByNumericSlug) {
    const TempDir tmp;
    auto cm = makeConfig(tmp,
        "[compiler]\n"
        "[compiler/cfg-10]\n"
        "name=Ten\n"
        "[compiler/cfg-2]\n"
        "name=Two\n"
        "[compiler/cfg-1]\n"
        "name=One\n");

    CompilerConfigCatalog catalog(*cm);
    catalog.reload();

    std::vector<wxString> slugs;
    for (const auto& cfg : catalog.all()) {
        slugs.push_back(cfg.slug);
    }
    EXPECT_EQ(slugs, (std::vector<wxString> { "default", "cfg-1", "cfg-2", "cfg-10" }));
}
