//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/tools/ReadFileTool.hpp"
using namespace fbide;
using namespace fbide::ai;
namespace fs = std::filesystem;

namespace {

/// RAII directory with auto-cleanup for the file-resolution and
/// read-cap tests. Each instance gets a unique counter-suffixed path
/// so concurrent test runs don't collide.
class TempDir {
public:
    TempDir() {
        static std::atomic<std::size_t> counter { 0 };
        const auto stamp = std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
        m_path = fs::temp_directory_path() / ("fbide-readfile-" + stamp);
        fs::create_directories(m_path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(m_path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    auto operator=(const TempDir&) -> TempDir& = delete;
    auto operator=(TempDir&&) -> TempDir& = delete;

    [[nodiscard]] auto path() const -> const fs::path& { return m_path; }

    [[nodiscard]] auto writeFile(const std::string& name, const std::string& content) const -> fs::path {
        const auto target = m_path / name;
        fs::create_directories(target.parent_path());
        std::ofstream stream(target, std::ios::binary);
        stream << content;
        return target;
    }

private:
    fs::path m_path;
};

} // namespace

// ---------------------------------------------------------------------------
// resolveSafePath — relative + scope checks
// ---------------------------------------------------------------------------

TEST(ReadFileToolResolve, EmptyPathArgRejected) {
    const TempDir dir;
    EXPECT_FALSE(ReadFileTool::resolveSafePath("", dir.path(), {}));
}

TEST(ReadFileToolResolve, RelativePathResolvesAgainstActiveDir) {
    const TempDir dir;
    dir.writeFile("sibling.bi", "");
    const auto resolved = ReadFileTool::resolveSafePath("sibling.bi", dir.path(), {});
    ASSERT_TRUE(resolved);
    EXPECT_EQ(dir.path() / "sibling.bi", *resolved);
}

TEST(ReadFileToolResolve, RelativeWithDotDotIsRejected) {
    const TempDir dir;
    EXPECT_FALSE(ReadFileTool::resolveSafePath("../secret.bi", dir.path(), {}));
}

TEST(ReadFileToolResolve, SubdirRelativePathResolves) {
    const TempDir dir;
    fs::create_directories(dir.path() / "sub");
    const auto resolved = ReadFileTool::resolveSafePath("sub/child.bi", dir.path(), {});
    ASSERT_TRUE(resolved);
    EXPECT_EQ(dir.path() / "sub" / "child.bi", *resolved);
}

// ---------------------------------------------------------------------------
// resolveSafePath — absolute paths
// ---------------------------------------------------------------------------

TEST(ReadFileToolResolve, AbsolutePathInsideSubtreeAccepted) {
    const TempDir dir;
    const auto candidate = (dir.path() / "in-subtree.bas").lexically_normal();
    const auto resolved = ReadFileTool::resolveSafePath(candidate.string(), dir.path(), {});
    ASSERT_TRUE(resolved);
    EXPECT_EQ(candidate, *resolved);
}

TEST(ReadFileToolResolve, AbsolutePathOutsideSubtreeRejected) {
    const TempDir dir;
    // A path entirely outside the active doc's directory.
    EXPECT_FALSE(ReadFileTool::resolveSafePath("/etc/hosts", dir.path(), {}));
}

TEST(ReadFileToolResolve, AbsolutePathMatchesOpenTabByExactPath) {
    const TempDir activeDir; // model thinks workspace = this
    const TempDir other;     // open tab from elsewhere
    const auto openPath = other.writeFile("tab.bas", "");
    const auto resolved = ReadFileTool::resolveSafePath(openPath.string(), activeDir.path(), { openPath });
    ASSERT_TRUE(resolved);
    EXPECT_EQ(openPath.lexically_normal(), *resolved);
}

TEST(ReadFileToolResolve, AbsolutePathMatchesOpenTabByFilenameOnly) {
    const TempDir activeDir;
    const TempDir other;
    const auto openPath = other.writeFile("tab.bas", "");
    // Model asks for an absolute path under a totally unrelated root —
    // we still accept it because the filename matches an open tab.
    const auto resolved = ReadFileTool::resolveSafePath("/var/whatever/tab.bas", activeDir.path(), { openPath });
    ASSERT_TRUE(resolved);
    EXPECT_EQ(openPath.lexically_normal(), *resolved);
}

TEST(ReadFileToolResolve, NoActiveDirFallsBackToFilenameMatch) {
    const TempDir other;
    const auto openPath = other.writeFile("untitled-buddy.bas", "");
    // No active doc → only filename match against open tabs.
    const auto resolved = ReadFileTool::resolveSafePath("untitled-buddy.bas", {}, { openPath });
    ASSERT_TRUE(resolved);
    EXPECT_EQ(openPath.lexically_normal(), *resolved);
}

TEST(ReadFileToolResolve, NoActiveDirRejectsUnmatchedPath) {
    EXPECT_FALSE(ReadFileTool::resolveSafePath("nothing.bas", {}, {}));
}

// ---------------------------------------------------------------------------
// readCapped — size and error handling
// ---------------------------------------------------------------------------

TEST(ReadFileToolReadCapped, ReadsSmallFile) {
    const TempDir dir;
    const auto file = dir.writeFile("greet.txt", "hello world");
    EXPECT_EQ("hello world", ReadFileTool::readCapped(file));
}

TEST(ReadFileToolReadCapped, MissingFileReturnsError) {
    const TempDir dir;
    const auto out = ReadFileTool::readCapped(dir.path() / "absent.txt");
    EXPECT_TRUE(out.StartsWith("[error:"));
}

TEST(ReadFileToolReadCapped, OversizeFileReturnsError) {
    const TempDir dir;
    // Slightly over the cap — write kMaxBytes + 1 bytes so we hit the
    // "exceeds cap" branch without thrashing the filesystem.
    const std::string oversize(ReadFileTool::kMaxBytes + 1, 'x');
    const auto file = dir.writeFile("big.txt", oversize);
    const auto out = ReadFileTool::readCapped(file);
    EXPECT_TRUE(out.StartsWith("[error:"));
    EXPECT_NE(wxString::npos, out.find("exceeds"));
}
