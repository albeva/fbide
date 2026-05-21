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
class ValueMergeTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Empty / no-op cases
// ---------------------------------------------------------------------------

TEST_F(ValueMergeTests, EmptyOverlayLeavesBaselineUnchanged) {
    Value baseline;
    baseline["foo"] = "bundle";
    baseline["nested"]["x"] = "1";

    const Value overlay;
    baseline.mergeFrom(overlay);

    EXPECT_EQ(baseline.get_or("foo", wxString { "<sentinel>" }), "bundle");
    EXPECT_EQ(baseline.get_or("nested.x", wxString { "<sentinel>" }), "1");
}

// ---------------------------------------------------------------------------
// Leaf overrides
// ---------------------------------------------------------------------------

TEST_F(ValueMergeTests, OverlayLeafReplacesBaselineLeaf) {
    Value baseline;
    baseline["foo"] = "bundle";

    Value overlay;
    overlay["foo"] = "user";

    baseline.mergeFrom(overlay);
    EXPECT_EQ(baseline.get_or("foo", wxString {}), "user");
}

TEST_F(ValueMergeTests, OverlayLeafAtNewPathIsAdded) {
    Value baseline;
    baseline["foo"] = "bundle";

    Value overlay;
    overlay["bar"] = "added";

    baseline.mergeFrom(overlay);
    EXPECT_EQ(baseline.get_or("foo", wxString {}), "bundle");
    EXPECT_EQ(baseline.get_or("bar", wxString { "<sentinel>" }), "added");
}

TEST_F(ValueMergeTests, BaselineKeyNotInOverlayIsPreserved) {
    Value baseline;
    baseline["keep"] = "yes";
    baseline["replace"] = "old";

    Value overlay;
    overlay["replace"] = "new";

    baseline.mergeFrom(overlay);
    EXPECT_EQ(baseline.get_or("keep", wxString {}), "yes");
    EXPECT_EQ(baseline.get_or("replace", wxString {}), "new");
}

// ---------------------------------------------------------------------------
// Empty-string overrides — key-presence wins regardless of value. Aggressive
// pruning later will only persist keys whose value differs from baseline;
// here we verify that an explicit empty value in the overlay still wins.
// ---------------------------------------------------------------------------

TEST_F(ValueMergeTests, EmptyStringOverlayValueStillOverrides) {
    Value baseline;
    baseline["foo"] = "bundle";

    Value overlay;
    overlay["foo"] = "";

    baseline.mergeFrom(overlay);
    EXPECT_EQ(baseline.get_or("foo", wxString { "<sentinel>" }), "");
}

// ---------------------------------------------------------------------------
// Nested groups
// ---------------------------------------------------------------------------

TEST_F(ValueMergeTests, NestedGroupsMergeRecursively) {
    Value baseline;
    baseline["editor"]["tabSize"] = "4";
    baseline["editor"]["theme"] = "dark";
    baseline["editor"]["font"] = "Menlo";

    Value overlay;
    overlay["editor"]["theme"] = "solarized"; // override existing
    overlay["editor"]["wrap"] = "true";       // add new in same group

    baseline.mergeFrom(overlay);

    EXPECT_EQ(baseline.get_or("editor.tabSize", wxString {}), "4");               // untouched
    EXPECT_EQ(baseline.get_or("editor.font", wxString {}), "Menlo");              // untouched
    EXPECT_EQ(baseline.get_or("editor.theme", wxString {}), "solarized");         // overridden
    EXPECT_EQ(baseline.get_or("editor.wrap", wxString { "<sentinel>" }), "true"); // added
}

TEST_F(ValueMergeTests, DeeplyNestedGroupsMergeRecursively) {
    Value baseline;
    baseline["a"]["b"]["c"] = "1";
    baseline["a"]["b"]["d"] = "2";

    Value overlay;
    overlay["a"]["b"]["c"] = "overridden";
    overlay["a"]["b"]["e"] = "added";

    baseline.mergeFrom(overlay);

    EXPECT_EQ(baseline.get_or("a.b.c", wxString {}), "overridden");
    EXPECT_EQ(baseline.get_or("a.b.d", wxString {}), "2");
    EXPECT_EQ(baseline.get_or("a.b.e", wxString { "<sentinel>" }), "added");
}

// ---------------------------------------------------------------------------
// Type-mismatch — overlay always wins. INI overlays mirror base structure
// so these aren't typical, but the rule must be consistent in case a
// hand-edited overlay drifts.
// ---------------------------------------------------------------------------

TEST_F(ValueMergeTests, OverlayLeafReplacesBaselineGroup) {
    Value baseline;
    baseline["foo"]["x"] = "1";
    baseline["foo"]["y"] = "2";

    Value overlay;
    overlay["foo"] = "leaf";

    baseline.mergeFrom(overlay);
    EXPECT_EQ(baseline.get_or("foo", wxString {}), "leaf");
}

TEST_F(ValueMergeTests, OverlayGroupReplacesBaselineLeaf) {
    Value baseline;
    baseline["foo"] = "leaf";

    Value overlay;
    overlay["foo"]["x"] = "1";

    baseline.mergeFrom(overlay);
    EXPECT_EQ(baseline.get_or("foo.x", wxString { "<sentinel>" }), "1");
}
