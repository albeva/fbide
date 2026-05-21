//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "config/Value.hpp"

using namespace fbide;

// gtest fixtures are referenced by TEST_F macro expansion and
// idiomatically stay at file scope.
// NOLINTNEXTLINE(misc-use-internal-linkage)
class ValueDiffTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Identity / empty
// ---------------------------------------------------------------------------

TEST_F(ValueDiffTests, IdenticalTreesProduceEmptyDiff) {
    Value merged;
    merged["foo"] = "x";
    merged["nested"]["a"] = "1";

    Value baseline;
    baseline["foo"] = "x";
    baseline["nested"]["a"] = "1";

    const auto diff = merged.diffAgainst(baseline);
    EXPECT_FALSE(static_cast<bool>(diff));
}

TEST_F(ValueDiffTests, EmptyMergedProducesEmptyDiff) {
    const Value merged;
    Value baseline;
    baseline["foo"] = "x";

    const auto diff = merged.diffAgainst(baseline);
    EXPECT_FALSE(static_cast<bool>(diff));
}

// ---------------------------------------------------------------------------
// Leaf-level divergences
// ---------------------------------------------------------------------------

TEST_F(ValueDiffTests, SingleDifferingLeafProducesSingleEntry) {
    Value merged;
    merged["foo"] = "user";
    merged["bar"] = "same";

    Value baseline;
    baseline["foo"] = "bundle";
    baseline["bar"] = "same";

    const auto diff = merged.diffAgainst(baseline);
    EXPECT_EQ(diff.get_or("foo", wxString { "<missing>" }), "user");
    // "bar" matches baseline → must not appear in diff
    EXPECT_EQ(diff.get_or("bar", wxString { "<missing>" }), "<missing>");
}

TEST_F(ValueDiffTests, NewKeyInMergedNotInBaselineIsIncluded) {
    Value merged;
    merged["foo"] = "bundle";
    merged["added"] = "new";

    Value baseline;
    baseline["foo"] = "bundle";

    const auto diff = merged.diffAgainst(baseline);
    EXPECT_EQ(diff.get_or("added", wxString { "<missing>" }), "new");
    EXPECT_EQ(diff.get_or("foo", wxString { "<missing>" }), "<missing>");
}

TEST_F(ValueDiffTests, KeyInBaselineNotInMergedIsExcluded) {
    // Deletion semantics are not expressed via the diff (the user would
    // edit the overlay file directly to express "remove this bundle key").
    // The diff persists only what's present in merged.
    Value merged;
    merged["keep"] = "x";

    Value baseline;
    baseline["keep"] = "x";
    baseline["removed"] = "y";

    const auto diff = merged.diffAgainst(baseline);
    EXPECT_FALSE(static_cast<bool>(diff));
}

TEST_F(ValueDiffTests, EmptyStringInMergedDifferingFromBaselineIsIncluded) {
    // Aggressive prune rule applies to *equal* values; an empty string
    // that differs from baseline is still a real divergence and must be
    // emitted so the overlay records "this key is intentionally blank".
    Value merged;
    merged["foo"] = "";

    Value baseline;
    baseline["foo"] = "bundle";

    const auto diff = merged.diffAgainst(baseline);
    EXPECT_EQ(diff.get_or("foo", wxString { "<missing>" }), "");
}

// ---------------------------------------------------------------------------
// Nested groups — parent groups synthesised only when they contain a divergence
// ---------------------------------------------------------------------------

TEST_F(ValueDiffTests, NestedGroupsOnlyDivergentSubPathsRetained) {
    Value merged;
    merged["editor"]["tabSize"] = "8";  // diverges from baseline
    merged["editor"]["theme"] = "dark"; // matches baseline
    merged["compiler"]["path"] = "fbc"; // matches baseline

    Value baseline;
    baseline["editor"]["tabSize"] = "4";
    baseline["editor"]["theme"] = "dark";
    baseline["compiler"]["path"] = "fbc";

    const auto diff = merged.diffAgainst(baseline);
    // editor.tabSize present, editor.theme absent, compiler.* absent
    EXPECT_EQ(diff.get_or("editor.tabSize", wxString { "<missing>" }), "8");
    EXPECT_EQ(diff.get_or("editor.theme", wxString { "<missing>" }), "<missing>");
    EXPECT_EQ(diff.get_or("compiler.path", wxString { "<missing>" }), "<missing>");
}

TEST_F(ValueDiffTests, DeeplyNestedDivergenceSynthesisesParentGroups) {
    Value merged;
    merged["a"]["b"]["c"] = "user";
    merged["a"]["b"]["d"] = "same";

    Value baseline;
    baseline["a"]["b"]["c"] = "bundle";
    baseline["a"]["b"]["d"] = "same";

    const auto diff = merged.diffAgainst(baseline);
    EXPECT_EQ(diff.get_or("a.b.c", wxString { "<missing>" }), "user");
    EXPECT_EQ(diff.get_or("a.b.d", wxString { "<missing>" }), "<missing>");
}

// ---------------------------------------------------------------------------
// Type mismatches — overlay always wins; matches mergeFrom symmetry
// ---------------------------------------------------------------------------

TEST_F(ValueDiffTests, TypeMismatchMergedLeafOverBaselineGroup) {
    // Pathological — INI overlays mirror base structure. But if the
    // merged tree has a leaf where baseline has a group, the leaf is a
    // divergence and must be persisted.
    Value merged;
    merged["foo"] = "leaf";

    Value baseline;
    baseline["foo"]["x"] = "1";

    const auto diff = merged.diffAgainst(baseline);
    EXPECT_EQ(diff.get_or("foo", wxString { "<missing>" }), "leaf");
}

TEST_F(ValueDiffTests, TypeMismatchMergedGroupOverBaselineLeaf) {
    Value merged;
    merged["foo"]["x"] = "1";

    Value baseline;
    baseline["foo"] = "leaf";

    const auto diff = merged.diffAgainst(baseline);
    EXPECT_EQ(diff.get_or("foo.x", wxString { "<missing>" }), "1");
}
