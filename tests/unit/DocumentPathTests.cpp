//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <wx/file.h>
#include <wx/filename.h>
#include <gtest/gtest.h>
#include "document/DocumentPath.hpp"

using namespace fbide;
namespace fs = std::filesystem;

namespace {

class TempDir {
public:
    TempDir() {
        const auto base = wxFileName::CreateTempFileName("fbide_pathtest");
        wxRemoveFile(base);
        wxFileName::Mkdir(base, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        m_path = toFsPath(base);
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

    auto touch(const std::string& name) const -> fs::path {
        const auto full = m_path / name;
        wxFile f(toWxString(full), wxFile::write);
        f.Write("x", 1);
        return full;
    }

private:
    fs::path m_path;
};

} // namespace

class DocumentPathTests : public testing::Test {};

TEST_F(DocumentPathTests, EmptyInputReturnsEmpty) {
    EXPECT_TRUE(canonicalizePath(fs::path {}).empty());
}

TEST_F(DocumentPathTests, RelativePathBecomesAbsolute) {
    const TempDir dir;
    const auto file = dir.touch("hello.bas");

    const auto saved = fs::current_path();
    fs::current_path(file.parent_path());
    const auto canonical = canonicalizePath(fs::path { "./" } / file.filename());
    fs::current_path(saved);

    EXPECT_TRUE(canonical.is_absolute()) << canonical;
    EXPECT_EQ(canonical.filename(), file.filename());
}

TEST_F(DocumentPathTests, DotDotIsResolved) {
    const TempDir dir;
    const auto file = dir.touch("real.bas");

    const auto with_dotdot = file.parent_path() / "sub" / ".." / file.filename();
    EXPECT_EQ(canonicalizePath(with_dotdot), canonicalizePath(file));
}

TEST_F(DocumentPathTests, MixedCaseCollapsesOnCaseInsensitiveFS) {
    // The bug from issue #87 — macOS/Windows resolve `fbgfx.bi` and
    // `FBGFX.bi` to the same file. weakly_canonical reflects the on-disk
    // casing, so both queries must canonicalize to the same string. On
    // case-sensitive filesystems the second path is a different file and
    // the test is skipped.
    const TempDir dir;
    const auto file = dir.touch("Mixed.bi");

    const auto upper = file.parent_path() / "MIXED.BI";

    std::error_code ec;
    if (!fs::exists(upper, ec)) {
        GTEST_SKIP() << "Case-sensitive filesystem — skip case-fold check.";
    }

    EXPECT_EQ(canonicalizePath(upper), canonicalizePath(file));
}

TEST_F(DocumentPathTests, SymlinkResolvesToTarget) {
#ifdef _WIN32
    GTEST_SKIP() << "Symlink creation requires elevation on Windows.";
#else
    const TempDir dir;
    const auto target = dir.touch("target.bas");

    const auto link = dir.path() / "alias.bas";
    std::error_code ec;
    fs::create_symlink(target, link, ec);
    ASSERT_FALSE(ec) << "create_symlink failed: " << ec.message();

    EXPECT_EQ(canonicalizePath(link), canonicalizePath(target));
#endif
}

TEST_F(DocumentPathTests, NonExistingPathDoesNotThrow) {
    const TempDir dir;
    const auto ghost = dir.path() / "ghost.bas";
    // Must not throw; result is just the absolute form (no real file to
    // resolve).
    const auto canonical = canonicalizePath(ghost);
    EXPECT_FALSE(canonical.empty());
    EXPECT_EQ(canonical.filename(), "ghost.bas");
}

TEST_F(DocumentPathTests, IdempotentOnCanonicalInput) {
    const TempDir dir;
    const auto file = dir.touch("ok.bas");
    const auto once = canonicalizePath(file);
    const auto twice = canonicalizePath(once);
    EXPECT_EQ(once, twice);
}

TEST_F(DocumentPathTests, ToFsPathAndBackRoundTrips) {
    const TempDir dir;
    const auto file = dir.touch("round.trip.bas");
    const auto wx = toWxString(file);
    EXPECT_EQ(toFsPath(wx), file);
}
