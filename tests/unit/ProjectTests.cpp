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
#include "config/ConfigManager.hpp"
#include "workspace/Project.hpp"

using namespace fbide;
namespace fs = std::filesystem;

namespace {

/// Project stores `Document*` as an opaque handle — it never dereferences.
/// Tests fabricate distinct, byte-aligned addresses so the bookkeeping can
/// be exercised without dragging the full `Document` type (and its
/// `Context` dependency) into the unit suite.
auto fakeDoc(const std::uintptr_t value) -> Document* {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
    return reinterpret_cast<Document*>(value);
}

constexpr std::uintptr_t kFakeDocPrimary = 0xABCD;
constexpr std::uintptr_t kFakeDocAlt = 0xBEEF;
constexpr std::uintptr_t kFakeDocMiddle = 0x1234;

/// RAII scratch directory used to back a `ConfigManager` for tests that
/// need a `Project` instance. Auto-cleans on destruction. Mirrors the
/// helper in `ConfigManagerTests.cpp` — duplicated locally so this file
/// stays self-contained; promote to `TestHelpers.hpp` once a third user
/// shows up.
class TempDir final {
public:
    TempDir() {
        const auto base = wxFileName::CreateTempFileName("fbide_proj_test");
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
        wxFFile out(full, "w");
        out.Write(contents);
    }

private:
    wxString m_path;
};

} // namespace

// --- ID types --------------------------------------------------------------

TEST(ProjectIdTest, DefaultIsInvalid) {
    constexpr Project::Id id;
    EXPECT_FALSE(static_cast<bool>(id));
}

TEST(ProjectIdTest, GeneratedIsValid) {
    const auto id = Project::Id::generate();
    EXPECT_TRUE(static_cast<bool>(id));
}

TEST(ProjectIdTest, GeneratedAreUnique) {
    const auto lhs = Project::Id::generate();
    const auto rhs = Project::Id::generate();
    EXPECT_NE(lhs, rhs);
}

TEST(ProjectIdTest, CopyIsEqual) {
    const auto original = Project::Id::generate();
    const auto copy = original;
    EXPECT_EQ(original, copy);
}

TEST(ProjectIdTest, Hashable) {
    std::unordered_map<Project::Id, int> map;
    const auto first = Project::Id::generate();
    const auto second = Project::Id::generate();
    map[first] = 1;
    map[second] = 2;
    EXPECT_EQ(map[first], 1);
    EXPECT_EQ(map[second], 2);
}

TEST(ProjectNodeIdTest, DefaultIsInvalid) {
    constexpr Project::Node::Id id;
    EXPECT_FALSE(static_cast<bool>(id));
}

TEST(ProjectNodeIdTest, GeneratedIsValid) {
    const auto id = Project::Node::Id::generate();
    EXPECT_TRUE(static_cast<bool>(id));
}

TEST(ProjectNodeIdTest, GeneratedAreUnique) {
    const auto lhs = Project::Node::Id::generate();
    const auto rhs = Project::Node::Id::generate();
    EXPECT_NE(lhs, rhs);
}

TEST(ProjectNodeIdTest, Hashable) {
    std::unordered_map<Project::Node::Id, int> map;
    const auto first = Project::Node::Id::generate();
    const auto second = Project::Node::Id::generate();
    map[first] = 1;
    map[second] = 2;
    EXPECT_EQ(map[first], 1);
    EXPECT_EQ(map[second], 2);
}

// --- Project construction --------------------------------------------------

/// `Project` now takes a `ConfigManager&` so its build-input getters can
/// forward to the IDE config. The fixture spins up a minimal disk-backed
/// `ConfigManager` per test so construction is real but isolated.
class ProjectTest : public testing::Test {
protected:
    void SetUp() override {
        m_tmp.write("config.ini", "version=0.5.0\n");
        m_config = std::make_unique<ConfigManager>(m_tmp.path(), m_tmp.path(), "config.ini");
    }

    [[nodiscard]] auto makeProject(Project::Mode mode) const -> Project {
        return Project { *m_config, mode };
    }

    TempDir m_tmp;
    std::unique_ptr<ConfigManager> m_config;
};

TEST_F(ProjectTest, EphemeralMode) {
    const auto project = makeProject(Project::Mode::Ephemeral);
    EXPECT_TRUE(project.isEphemeral());
    EXPECT_EQ(project.getMode(), Project::Mode::Ephemeral);
}

TEST_F(ProjectTest, PersistentMode) {
    const auto project = makeProject(Project::Mode::Persistent);
    EXPECT_FALSE(project.isEphemeral());
    EXPECT_EQ(project.getMode(), Project::Mode::Persistent);
}

TEST_F(ProjectTest, IdIsValid) {
    const auto project = makeProject(Project::Mode::Ephemeral);
    EXPECT_TRUE(static_cast<bool>(project.getId()));
}

TEST_F(ProjectTest, IdsAreUniquePerInstance) {
    const auto lhs = makeProject(Project::Mode::Ephemeral);
    const auto rhs = makeProject(Project::Mode::Ephemeral);
    EXPECT_NE(lhs.getId(), rhs.getId());
}

// --- addFile / setNodePath -------------------------------------------------

TEST_F(ProjectTest, AddFileWithPathYieldsValidNode) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto* node = project.addFile(nullptr, fs::path { "/tmp/main.bas" });
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->path, fs::path { "/tmp/main.bas" });
}

TEST_F(ProjectTest, AddFileUntitledHasEmptyPath) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto* node = project.addFile(nullptr, {});
    ASSERT_NE(node, nullptr);
    EXPECT_TRUE(node->path.empty());
}

TEST_F(ProjectTest, AddFileNodesAreUnique) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* first = project.addFile(project.getRoot(), fs::path { "/tmp/a.bas" });
    auto* second = project.addFile(project.getRoot(), fs::path { "/tmp/b.bas" });
    EXPECT_NE(first, second);
}

TEST_F(ProjectTest, EphemeralRootIsTheAddedFile) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto* node = project.addFile(nullptr, fs::path { "/tmp/main.bas" });
    EXPECT_EQ(project.getRoot(), node);
}

TEST_F(ProjectTest, PersistentChildIsLinkedUnderRoot) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* root = project.getRoot();
    ASSERT_NE(root, nullptr);
    auto* child = project.addFile(root, fs::path { "/tmp/a.bas" });
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->parent, root);
    const auto& children = std::get<Project::Node::Folder>(root->entry).children;
    ASSERT_EQ(children.size(), 1U);
    EXPECT_EQ(children.front(), child);
}

TEST_F(ProjectTest, SetNodePathSetsPathOnUntitledNode) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto* node = project.addFile(nullptr, {});
    project.setNodePath(node, fs::path { "/tmp/saved.bas" });
    EXPECT_EQ(node->path, fs::path { "/tmp/saved.bas" });
}

TEST_F(ProjectTest, SetNodePathReplacesExistingPath) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto* node = project.addFile(nullptr, fs::path { "/tmp/old.bas" });
    project.setNodePath(node, fs::path { "/tmp/new.bas" });
    EXPECT_EQ(node->path, fs::path { "/tmp/new.bas" });
}

// --- findNode --------------------------------------------------------------

TEST_F(ProjectTest, FindNodeReturnsAddedNode) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto* node = project.addFile(nullptr, fs::path { "/tmp/main.bas" });
    EXPECT_EQ(project.findNode(node->id), node);
}

TEST_F(ProjectTest, FindNodeReturnsNullForUnknownId) {
    auto project = makeProject(Project::Mode::Ephemeral);
    const auto stranger = Project::Node::Id::generate();
    EXPECT_EQ(project.findNode(stranger), nullptr);
}

// --- getPrimarySource ------------------------------------------------------

TEST_F(ProjectTest, EphemeralPrimarySourceReturnsBoundDoc) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto* doc = fakeDoc(kFakeDocPrimary);
    project.addFile(nullptr, fs::path { "/tmp/main.bas" }, doc);
    EXPECT_EQ(project.getPrimarySource(), doc);
}

TEST_F(ProjectTest, EphemeralPrimarySourceNullWhenUnbound) {
    auto project = makeProject(Project::Mode::Ephemeral);
    project.addFile(nullptr, fs::path { "/tmp/main.bas" });
    EXPECT_EQ(project.getPrimarySource(), nullptr);
}

TEST_F(ProjectTest, EphemeralPrimarySourceNullWhenEmpty) {
    const auto project = makeProject(Project::Mode::Ephemeral);
    EXPECT_EQ(project.getPrimarySource(), nullptr);
}

// --- getDocuments ----------------------------------------------------------

TEST_F(ProjectTest, GetDocumentsIncludesBound) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto* doc = fakeDoc(kFakeDocAlt);
    project.addFile(nullptr, fs::path { "/tmp/main.bas" }, doc);
    const auto docs = project.getDocuments();
    ASSERT_EQ(docs.size(), 1U);
    EXPECT_EQ(docs.front(), doc);
}

TEST_F(ProjectTest, GetDocumentsSkipsUnbound) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* root = project.getRoot();
    project.addFile(root, fs::path { "/tmp/a.bas" });
    project.addFile(root, fs::path { "/tmp/b.bas" }, fakeDoc(kFakeDocMiddle));
    project.addFile(root, fs::path { "/tmp/c.bas" });
    const auto docs = project.getDocuments();
    ASSERT_EQ(docs.size(), 1U);
    EXPECT_EQ(docs.front(), fakeDoc(kFakeDocMiddle));
}

TEST_F(ProjectTest, GetDocumentsEmptyOnFreshProject) {
    const auto project = makeProject(Project::Mode::Persistent);
    EXPECT_TRUE(project.getDocuments().empty());
}

// --- clearNodeDocument -----------------------------------------------------

TEST_F(ProjectTest, ClearNodeDocumentDropsBackLink) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto* doc = fakeDoc(kFakeDocAlt);
    auto* node = project.addFile(nullptr, fs::path { "/tmp/main.bas" }, doc);
    project.clearNodeDocument(node);
    EXPECT_TRUE(project.getDocuments().empty());
}

// --- capabilities ----------------------------------------------------------

TEST_F(ProjectTest, EphemeralAdvertisesAllCapabilities) {
    const auto project = makeProject(Project::Mode::Ephemeral);
    const auto caps = project.getCapabilities();
    EXPECT_TRUE(caps & +Project::Capability::Compile);
    EXPECT_TRUE(caps & +Project::Capability::CompileAndRun);
    EXPECT_TRUE(caps & +Project::Capability::Run);
    EXPECT_TRUE(caps & +Project::Capability::QuickRun);
}
