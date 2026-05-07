//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "config/Version.hpp"

using namespace fbide;
using Tag = Version::Tag;

class VersionTests : public testing::Test {};

// ---------------------------------------------------------------------------
// asString — round-trip the canonical formats.
// ---------------------------------------------------------------------------

TEST_F(VersionTests, AsString_FinalRelease) {
    EXPECT_EQ(Version(0, 5, 0).asString(), "0.5.0");
    EXPECT_EQ(Version(1, 2, 3).asString(), "1.2.3");
}

TEST_F(VersionTests, AsString_Alpha) {
    EXPECT_EQ(Version(0, 5, 0, Tag::Alpha, 1).asString(), "0.5.0.alpha-1");
}

TEST_F(VersionTests, AsString_Beta) {
    EXPECT_EQ(Version(0, 5, 0, Tag::Beta, 7).asString(), "0.5.0.beta-7");
}

TEST_F(VersionTests, AsString_RC) {
    EXPECT_EQ(Version(0, 5, 0, Tag::ReleaseCandidate, 2).asString(), "0.5.0.rc-2");
}

// ---------------------------------------------------------------------------
// Parser — accept the same shapes asString produces, plus tolerate a
// tag without an iteration suffix (`0.5.0.alpha`).
// ---------------------------------------------------------------------------

TEST_F(VersionTests, Parse_Numeric) {
    Version v("0.5.0");
    EXPECT_EQ(v.getMajor(), 0);
    EXPECT_EQ(v.getMinor(), 5);
    EXPECT_EQ(v.getPatch(), 0);
    EXPECT_EQ(v.getTag(), Tag::None);
    EXPECT_EQ(v.getTweak(), 0);
}

TEST_F(VersionTests, Parse_TaggedWithTweak) {
    Version v("0.5.0.alpha-1");
    EXPECT_EQ(v.getTag(), Tag::Alpha);
    EXPECT_EQ(v.getTweak(), 1);
}

TEST_F(VersionTests, Parse_TaggedWithoutTweak) {
    Version v("0.5.0.beta");
    EXPECT_EQ(v.getTag(), Tag::Beta);
    EXPECT_EQ(v.getTweak(), 0);
}

TEST_F(VersionTests, Parse_RC) {
    Version v("1.0.0.rc-3");
    EXPECT_EQ(v.getMajor(), 1);
    EXPECT_EQ(v.getMinor(), 0);
    EXPECT_EQ(v.getPatch(), 0);
    EXPECT_EQ(v.getTag(), Tag::ReleaseCandidate);
    EXPECT_EQ(v.getTweak(), 3);
}

TEST_F(VersionTests, Parse_UnknownTagSilentlyDropped) {
    // Unknown tag word collapses to Tag::None; tweak is dropped so the
    // value round-trips as a clean numeric string.
    Version v("0.5.0.foo-9");
    EXPECT_EQ(v.getTag(), Tag::None);
    EXPECT_EQ(v.getTweak(), 0);
    EXPECT_EQ(v.asString(), "0.5.0");
}

TEST_F(VersionTests, Parse_RoundTrip) {
    for (const wxString& text : { "0.5.0", "0.5.0.alpha-1", "0.5.0.beta-7", "1.2.3.rc-2" }) {
        EXPECT_EQ(Version(text).asString(), text);
    }
}

// ---------------------------------------------------------------------------
// Ordering — pre-releases sort BEFORE the matching final release;
// alpha < beta < rc within the same numeric triple.
// ---------------------------------------------------------------------------

TEST_F(VersionTests, Ordering_NumericTriple) {
    EXPECT_LT(Version(0, 4, 9), Version(0, 5, 0));
    EXPECT_LT(Version(0, 5, 0), Version(0, 5, 1));
    EXPECT_LT(Version(0, 5, 1), Version(1, 0, 0));
}

TEST_F(VersionTests, Ordering_PreReleaseBeforeFinal) {
    EXPECT_LT(Version(0, 5, 0, Tag::Alpha, 1), Version(0, 5, 0));
    EXPECT_LT(Version(0, 5, 0, Tag::Beta, 1), Version(0, 5, 0));
    EXPECT_LT(Version(0, 5, 0, Tag::ReleaseCandidate, 1), Version(0, 5, 0));
}

TEST_F(VersionTests, Ordering_TagRanking) {
    EXPECT_LT(Version(0, 5, 0, Tag::Alpha, 1), Version(0, 5, 0, Tag::Beta, 1));
    EXPECT_LT(Version(0, 5, 0, Tag::Beta, 1), Version(0, 5, 0, Tag::ReleaseCandidate, 1));
}

TEST_F(VersionTests, Ordering_TweakBreaksTiesWithinTag) {
    EXPECT_LT(Version(0, 5, 0, Tag::Alpha, 1), Version(0, 5, 0, Tag::Alpha, 2));
    EXPECT_LT(Version(0, 5, 0, Tag::Beta, 3), Version(0, 5, 0, Tag::Beta, 7));
    EXPECT_LT(Version(0, 5, 0, Tag::ReleaseCandidate, 1), Version(0, 5, 0, Tag::ReleaseCandidate, 2));
}

TEST_F(VersionTests, Ordering_FullSemverChain) {
    // Canonical SemVer-style progression up to a final release.
    const Version v_alpha1 (0, 5, 0, Tag::Alpha, 1);
    const Version v_alpha2 (0, 5, 0, Tag::Alpha, 2);
    const Version v_beta1  (0, 5, 0, Tag::Beta, 1);
    const Version v_rc1    (0, 5, 0, Tag::ReleaseCandidate, 1);
    const Version v_rc2    (0, 5, 0, Tag::ReleaseCandidate, 2);
    const Version v_final  (0, 5, 0);

    EXPECT_LT(v_alpha1, v_alpha2);
    EXPECT_LT(v_alpha2, v_beta1);
    EXPECT_LT(v_beta1,  v_rc1);
    EXPECT_LT(v_rc1,    v_rc2);
    EXPECT_LT(v_rc2,    v_final);
}

TEST_F(VersionTests, Equality) {
    EXPECT_EQ(Version(0, 5, 0), Version(0, 5, 0));
    EXPECT_EQ(Version(0, 5, 0, Tag::Alpha, 1), Version(0, 5, 0, Tag::Alpha, 1));
    EXPECT_NE(Version(0, 5, 0), Version(0, 5, 0, Tag::Alpha, 1));
}

// ---------------------------------------------------------------------------
// Compile-time fbide() factory pulls from the configured cmake metadata.
// ---------------------------------------------------------------------------

TEST_F(VersionTests, FbideFactoryProducesNonEmptyString) {
    const auto v = Version::fbide();
    EXPECT_FALSE(v.asString().empty());
    // Pull from cmake-generated config; can't assert specific values
    // because they shift each release. At minimum patch+minor+major
    // shouldn't all be zero — the project ships with 0.5.0 today.
    EXPECT_NE(Version(), v);
}
