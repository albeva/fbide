//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/AiTypes.hpp"
using namespace fbide;
using namespace fbide::ai;

TEST(JoinSystemTests, EmptyVectorYieldsEmptyString) {
    EXPECT_TRUE(joinSystem({}).empty());
}

TEST(JoinSystemTests, SingleBlockReturnsItsText) {
    EXPECT_EQ(joinSystem({ { .text = "hello", .cacheable = false } }), "hello");
}

TEST(JoinSystemTests, MultipleBlocksJoinedByDoubleNewline) {
    const std::vector<AiContent> blocks {
        { .text = "first", .cacheable = false },
        { .text = "second", .cacheable = true },
    };
    EXPECT_EQ(joinSystem(blocks), "first\n\nsecond");
}

TEST(JoinSystemTests, EmptyBlocksAreSkipped) {
    const std::vector<AiContent> blocks {
        { .text = "", .cacheable = false },
        { .text = "a", .cacheable = false },
        { .text = "", .cacheable = true },
        { .text = "b", .cacheable = false },
        { .text = "", .cacheable = false },
    };
    EXPECT_EQ(joinSystem(blocks), "a\n\nb");
}

TEST(JoinSystemTests, AllEmptyYieldsEmpty) {
    const std::vector<AiContent> blocks {
        { .text = "", .cacheable = false },
        { .text = "", .cacheable = true },
    };
    EXPECT_TRUE(joinSystem(blocks).empty());
}

TEST(AiContentTests, DefaultIsNotCacheable) {
    const AiContent block {};
    EXPECT_FALSE(block.cacheable);
    EXPECT_TRUE(block.text.empty());
}
