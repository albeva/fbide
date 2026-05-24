//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/AiContext.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {

/// Count how many `EditTargetItem`s currently live in `context.items()`.
/// At most one is expected, but the test checks that the invariant holds
/// after every operation.
auto editTargetCount(const AiContext& context) -> std::size_t {
    std::size_t count = 0;
    for (const auto& item : context.items()) {
        if (dynamic_cast<const EditTargetItem*>(item.get()) != nullptr) {
            count++;
        }
    }
    return count;
}

} // namespace

// ---------------------------------------------------------------------------
// AiContext::setEditTarget
// ---------------------------------------------------------------------------

TEST(AiContextSetEditTarget, SettingFromEmptyContextAddsTheTarget) {
    AiContext context;
    EXPECT_EQ(nullptr, context.editTarget());

    context.setEditTarget("/path/to/file.bas");
    ASSERT_NE(nullptr, context.editTarget());
    EXPECT_EQ(std::filesystem::path("/path/to/file.bas"), context.editTarget()->path());
    EXPECT_EQ(1U, editTargetCount(context));
}

TEST(AiContextSetEditTarget, SettingWithEmptyPathClearsTheTarget) {
    AiContext context;
    context.setEditTarget("/path/to/file.bas");
    ASSERT_NE(nullptr, context.editTarget());

    context.setEditTarget(std::filesystem::path {});
    EXPECT_EQ(nullptr, context.editTarget());
    EXPECT_EQ(0U, editTargetCount(context));
}

TEST(AiContextSetEditTarget, SettingReplacesAnExistingTarget) {
    AiContext context;
    context.setEditTarget("/path/first.bas");
    context.setEditTarget("/path/second.bas");

    ASSERT_NE(nullptr, context.editTarget());
    EXPECT_EQ(std::filesystem::path("/path/second.bas"), context.editTarget()->path());
    // Invariant: at most one edit target exists.
    EXPECT_EQ(1U, editTargetCount(context));
}

TEST(AiContextSetEditTarget, KeepsSiblingFileContextItems) {
    AiContext context;
    context.add(std::make_unique<FileContextItem>("/path/sibling.bi"));
    context.setEditTarget("/path/main.bas");

    // The FileContextItem must survive both the target-add and a target-clear.
    ASSERT_EQ(2U, context.items().size());
    context.setEditTarget(std::filesystem::path {});
    ASSERT_EQ(1U, context.items().size());
    EXPECT_NE(nullptr, dynamic_cast<const FileContextItem*>(context.items().at(0).get()));
}

TEST(AiContextSetEditTarget, KeepsBufferContextItems) {
    AiContext context;
    context.add(std::make_unique<BufferContextItem>("tab.bas", "DIM x AS INTEGER"));
    context.setEditTarget("/path/main.bas");
    context.setEditTarget("/path/other.bas");

    // The buffer item must survive both the replace and the existence of an
    // edit target alongside it.
    ASSERT_EQ(2U, context.items().size());
    EXPECT_EQ(1U, editTargetCount(context));
    EXPECT_NE(nullptr, dynamic_cast<const BufferContextItem*>(context.items().at(0).get()));
}

TEST(AiContextSetEditTarget, ClearingWhenNoTargetIsANoOp) {
    AiContext context;
    context.add(std::make_unique<FileContextItem>("/path/sibling.bi"));

    // No target to clear — siblings shouldn't move.
    context.setEditTarget(std::filesystem::path {});
    ASSERT_EQ(1U, context.items().size());
    EXPECT_EQ(nullptr, context.editTarget());
}

// ---------------------------------------------------------------------------
// AiContext::removeAt
// ---------------------------------------------------------------------------

TEST(AiContextRemoveAt, RemovesInRangeIndex) {
    AiContext context;
    context.add(std::make_unique<FileContextItem>("/path/a.bas"));
    context.add(std::make_unique<FileContextItem>("/path/b.bas"));
    context.add(std::make_unique<FileContextItem>("/path/c.bas"));

    context.removeAt(1);
    ASSERT_EQ(2U, context.items().size());
    // Order is preserved — `b.bas` is gone, `a.bas` and `c.bas` stay in order.
    EXPECT_EQ("a.bas", context.items().at(0)->label());
    EXPECT_EQ("c.bas", context.items().at(1)->label());
}

TEST(AiContextRemoveAt, OutOfRangeIndexIsANoOp) {
    constexpr std::size_t kOutOfRangeIndex = 7;
    AiContext context;
    context.add(std::make_unique<FileContextItem>("/path/only.bas"));

    context.removeAt(kOutOfRangeIndex);
    EXPECT_EQ(1U, context.items().size());
}

TEST(AiContextRemoveAt, EmptyContainerIsANoOp) {
    AiContext context;
    context.removeAt(0);
    EXPECT_TRUE(context.empty());
}

TEST(AiContextRemoveAt, RemovingTheLastItemLeavesItEmpty) {
    AiContext context;
    context.add(std::make_unique<FileContextItem>("/path/only.bas"));
    context.removeAt(0);
    EXPECT_TRUE(context.empty());
}
