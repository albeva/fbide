//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "markdown/MarkdownImageCache.hpp"

using namespace fbide;
using namespace fbide::markdown;

namespace {

/// 1×1 placeholder used by the eviction tests — the contents don't matter,
/// only the existence of a Ready entry holding it. wxImage default-fills
/// to black; we don't care about colour here.
auto makeBitmap(const int width, const int height) -> wxBitmap {
    return { wxImage(width, height) };
}

} // namespace

class MarkdownImageCacheTests : public testing::Test {};

// LRU eviction kicks in once the configured cap of Ready entries is full;
// the oldest (least-recently inserted / accessed) entry is dropped.
TEST_F(MarkdownImageCacheTests, EvictsOldestOnceCapReached) {
    MarkdownImageCache cache(3);
    cache.insertReady("a", makeBitmap(1, 1), 1, 1);
    cache.insertReady("b", makeBitmap(1, 1), 1, 1);
    cache.insertReady("c", makeBitmap(1, 1), 1, 1);
    cache.insertReady("d", makeBitmap(1, 1), 1, 1);
    EXPECT_FALSE(cache.contains("a"));
    EXPECT_TRUE(cache.contains("b"));
    EXPECT_TRUE(cache.contains("c"));
    EXPECT_TRUE(cache.contains("d"));
}

// A `get` on an existing Ready entry counts as a use and moves it to the
// most-recently-used end of the LRU — so a subsequent insert evicts the
// next-oldest instead.
TEST_F(MarkdownImageCacheTests, GetTouchesLruPosition) {
    MarkdownImageCache cache(3);
    cache.insertReady("a", makeBitmap(1, 1), 1, 1);
    cache.insertReady("b", makeBitmap(1, 1), 1, 1);
    cache.insertReady("c", makeBitmap(1, 1), 1, 1);
    (void)cache.get("a");                           // touches "a" — LRU order is now b, c, a
    cache.insertReady("d", makeBitmap(1, 1), 1, 1); // should evict "b"
    EXPECT_TRUE(cache.contains("a"));
    EXPECT_FALSE(cache.contains("b"));
    EXPECT_TRUE(cache.contains("c"));
    EXPECT_TRUE(cache.contains("d"));
}

// Failed entries hold no bitmap, so they don't pressure the bitmap cap.
// A cap of 1 should permit one Ready entry alongside any number of Failed
// entries from disallowed-scheme lookups.
TEST_F(MarkdownImageCacheTests, FailedEntriesAreNotCountedAgainstCap) {
    MarkdownImageCache cache(1);
    // file:// is not in the allowed-scheme list — synthesises a Failed
    // entry synchronously without touching the network.
    const auto& failed = cache.get("file:///tmp/nope.png");
    EXPECT_EQ(failed.state, MarkdownImageCache::State::Failed);
    cache.insertReady("a", makeBitmap(1, 1), 1, 1);
    cache.insertReady("b", makeBitmap(1, 1), 1, 1);
    EXPECT_TRUE(cache.contains("file:///tmp/nope.png"));
    EXPECT_FALSE(cache.contains("a")); // evicted by "b"
    EXPECT_TRUE(cache.contains("b"));
}

// `clearAll` drops every entry and resets the LRU bookkeeping — the cache
// is fully usable again at full capacity.
TEST_F(MarkdownImageCacheTests, ClearAllResetsBookkeeping) {
    MarkdownImageCache cache(2);
    cache.insertReady("a", makeBitmap(1, 1), 1, 1);
    cache.insertReady("b", makeBitmap(1, 1), 1, 1);
    cache.clearAll();
    cache.insertReady("c", makeBitmap(1, 1), 1, 1);
    cache.insertReady("d", makeBitmap(1, 1), 1, 1);
    EXPECT_FALSE(cache.contains("a"));
    EXPECT_FALSE(cache.contains("b"));
    EXPECT_TRUE(cache.contains("c"));
    EXPECT_TRUE(cache.contains("d"));
}
