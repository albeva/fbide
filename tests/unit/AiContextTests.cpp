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

/// RAII temp file for the FileContextItem cache tests. Cleans up on
/// destruction; supports overwrite + mtime pin so the cache-hit case
/// can be exercised without filesystem race-condition flakiness.
class TempFile {
public:
    explicit TempFile(const std::string& initial) {
        // Atomic counter disambiguates concurrent test temp files
        // without leaning on pointer-to-int reinterpret_cast.
        static std::atomic<std::size_t> counter { 0 };
        const auto stamp = std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
        m_path = std::filesystem::temp_directory_path() / ("fbide-aictx-" + stamp + ".txt");
        write(initial);
    }
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }
    TempFile(const TempFile&) = delete;
    TempFile(TempFile&&) = delete;
    auto operator=(const TempFile&) -> TempFile& = delete;
    auto operator=(TempFile&&) -> TempFile& = delete;

    [[nodiscard]] auto path() const -> const std::filesystem::path& { return m_path; }

    void write(const std::string& content) const {
        std::ofstream stream(m_path);
        stream << content;
    }

    [[nodiscard]] auto mtime() const -> std::filesystem::file_time_type {
        return std::filesystem::last_write_time(m_path);
    }

    void setMtime(std::filesystem::file_time_type time) const {
        std::filesystem::last_write_time(m_path, time);
    }

private:
    std::filesystem::path m_path;
};

/// Extract the body text emitted by a context item — strips the
/// `--- File: ... ---` header and the surrounding newlines so tests
/// can assert against the content alone.
auto itemBody(const AiContextItem& item) -> wxString {
    const auto& out = item.toBlock().text;
    // The header line is `\n--- ... ---\n`, body follows, then a `\n`.
    const auto firstNl = out.find('\n', 1); // skip the leading \n
    if (firstNl == wxString::npos) {
        return {};
    }
    auto rest = out.SubString(firstNl + 1, out.size() - 1);
    if (rest.EndsWith("\n")) {
        rest.RemoveLast();
    }
    return rest;
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

// ---------------------------------------------------------------------------
// FileContextItem mtime-keyed content cache
// ---------------------------------------------------------------------------

TEST(FileContextItemCache, FirstCallReadsTheFile) {
    const TempFile file("hello world");
    const FileContextItem item(file.path());
    EXPECT_EQ("hello world", itemBody(item));
}

TEST(FileContextItemCache, CacheHitReturnsStaleContentWhenMtimeUnchanged) {
    // Pin the mtime: write A, snapshot mtime, write B, restore mtime.
    // The cached read of A should win because the mtime says "nothing
    // changed since the last read".
    const TempFile file("first");
    const FileContextItem item(file.path());
    ASSERT_EQ("first", itemBody(item));

    const auto pinned = file.mtime();
    file.write("second");
    file.setMtime(pinned);

    // Cache hit — same mtime, returns the cached "first".
    EXPECT_EQ("first", itemBody(item));
}

TEST(FileContextItemCache, MtimeChangeForcesReread) {
    using namespace std::chrono_literals;
    // Bump the mtime by a day so it's distinctly different — some
    // filesystems have coarse mtime resolution and a same-second
    // write may not register as new.
    constexpr auto kMtimeBumpForward = 24h;

    const TempFile file("first");
    const FileContextItem item(file.path());
    ASSERT_EQ("first", itemBody(item));

    file.write("second");
    file.setMtime(file.mtime() + kMtimeBumpForward);

    EXPECT_EQ("second", itemBody(item));
}

TEST(FileContextItemCache, MtimeMovingBackwardsForcesReread) {
    // Backup-restore tools can move mtime backwards; the cache must
    // treat that as a change too (compare via `!=`, not `<`).
    using namespace std::chrono_literals;
    constexpr auto kMtimeBumpBackward = 48h;

    const TempFile file("first");
    const FileContextItem item(file.path());
    ASSERT_EQ("first", itemBody(item));

    file.write("second");
    file.setMtime(file.mtime() - kMtimeBumpBackward);

    EXPECT_EQ("second", itemBody(item));
}

TEST(FileContextItemCache, MissingFileFallsBackToPlaceholder) {
    const FileContextItem item("/path/does/not/exist.txt");
    EXPECT_EQ("<could not read file>", itemBody(item));
}

TEST(FileContextItemCache, DeletedFileAfterFirstReadFallsBackOnSecondCall) {
    // The cache holds the first read's content, but a stat failure on
    // the second call evicts the cache and returns the placeholder.
    auto file = std::make_unique<TempFile>("initial");
    const FileContextItem item(file->path());
    ASSERT_EQ("initial", itemBody(item));

    file.reset(); // remove the file

    EXPECT_EQ("<could not read file>", itemBody(item));
}

// ---------------------------------------------------------------------------
// toBlock() cacheable flag — drives prompt caching on supported providers
// ---------------------------------------------------------------------------

TEST(ContextBlockCacheable, FileItemIsCacheable) {
    const TempFile file("content");
    const FileContextItem item(file.path());
    EXPECT_TRUE(item.toBlock().cacheable);
}

TEST(ContextBlockCacheable, EditTargetItemIsCacheable) {
    const TempFile file("content");
    const EditTargetItem item(file.path());
    EXPECT_TRUE(item.toBlock().cacheable);
}

TEST(ContextBlockCacheable, BufferItemIsNotCacheable) {
    const BufferContextItem item("tab.bas", "DIM x AS INTEGER");
    EXPECT_FALSE(item.toBlock().cacheable);
}

// ---------------------------------------------------------------------------
// AiContext::buildBlocks — one block per item, in insertion order
// ---------------------------------------------------------------------------

TEST(AiContextBuildBlocks, EmptyContextYieldsNoBlocks) {
    const AiContext context;
    EXPECT_TRUE(context.buildBlocks().empty());
}

TEST(AiContextBuildBlocks, EmitsOneBlockPerItemInInsertionOrder) {
    AiContext context;
    context.add(std::make_unique<BufferContextItem>("one.bas", "alpha"));
    context.add(std::make_unique<BufferContextItem>("two.bas", "beta"));

    const auto blocks = context.buildBlocks();
    ASSERT_EQ(2U, blocks.size());
    EXPECT_NE(wxString::npos, blocks.at(0).text.find("one.bas"));
    EXPECT_NE(wxString::npos, blocks.at(0).text.find("alpha"));
    EXPECT_NE(wxString::npos, blocks.at(1).text.find("two.bas"));
    EXPECT_NE(wxString::npos, blocks.at(1).text.find("beta"));
}

TEST(AiContextBuildBlocks, PreservesPerItemCacheableFlag) {
    AiContext context;
    const TempFile file("disk-content");
    context.add(std::make_unique<FileContextItem>(file.path()));
    context.add(std::make_unique<BufferContextItem>("tab.bas", "live-edit"));

    const auto blocks = context.buildBlocks();
    ASSERT_EQ(2U, blocks.size());
    EXPECT_TRUE(blocks.at(0).cacheable);  // file
    EXPECT_FALSE(blocks.at(1).cacheable); // buffer
}
