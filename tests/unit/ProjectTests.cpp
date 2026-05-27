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

TEST_F(ProjectTest, SetFilePathSetsPathOnUntitledNode) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto* node = project.addFile(nullptr, {});
    project.setFilePath(node, fs::path { "/tmp/saved.bas" });
    EXPECT_EQ(node->path, fs::path { "/tmp/saved.bas" });
}

TEST_F(ProjectTest, SetFilePathReplacesExistingPath) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto* node = project.addFile(nullptr, fs::path { "/tmp/old.bas" });
    project.setFilePath(node, fs::path { "/tmp/new.bas" });
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

// --- addFolder / addRealFolder / addFiles ----------------------------------

TEST_F(ProjectTest, AddFolderVirtualUnderRoot) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* folder = project.addFolder(project.getRoot(), "Sources");
    ASSERT_NE(folder, nullptr);
    EXPECT_TRUE(folder->path.empty());
    EXPECT_EQ(std::get<Project::Node::Folder>(folder->entry).name, "Sources");
    EXPECT_EQ(folder->parent, project.getRoot());
}

TEST_F(ProjectTest, AddFolderNested) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* outer = project.addFolder(project.getRoot(), "outer");
    auto* inner = project.addFolder(outer, "inner");
    EXPECT_EQ(inner->parent, outer);
    const auto& outerChildren = std::get<Project::Node::Folder>(outer->entry).children;
    ASSERT_EQ(outerChildren.size(), 1U);
    EXPECT_EQ(outerChildren.front(), inner);
}

TEST_F(ProjectTest, AddRealFolderCreatesDirectory) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto dir = fs::path(m_tmp.path().ToStdString()) / "fresh";
    ASSERT_FALSE(fs::exists(dir));
    const auto result = project.addRealFolder(project.getRoot(), dir);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->path, dir);
    EXPECT_TRUE(fs::is_directory(dir));
}

TEST_F(ProjectTest, AddRealFolderAcceptsExistingDirectory) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto dir = fs::path(m_tmp.path().ToStdString()) / "existing";
    fs::create_directory(dir);
    const auto result = project.addRealFolder(project.getRoot(), dir);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->path, dir);
}

TEST_F(ProjectTest, AddRealFolderRejectsExistingFile) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto path = fs::path(m_tmp.path().ToStdString()) / "afile";
    { std::ofstream { path } << "hello"; }
    const auto result = project.addRealFolder(project.getRoot(), path);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::Clash);
}

TEST_F(ProjectTest, AddFilesBulk) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* folder = project.addFolder(project.getRoot(), "files");
    const std::array paths { fs::path { "/tmp/a.bas" }, fs::path { "/tmp/b.bas" }, fs::path { "/tmp/c.bas" } };
    const auto nodes = project.addFiles(folder, paths);
    ASSERT_EQ(nodes.size(), 3U);
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        EXPECT_EQ(nodes[i]->path, paths[i]);
        EXPECT_EQ(nodes[i]->parent, folder);
    }
}

// --- removeNode ------------------------------------------------------------

TEST_F(ProjectTest, RemoveFileDetachesFromParent) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* root = project.getRoot();
    auto* file = project.addFile(root, fs::path { "/tmp/a.bas" });
    const auto fileId = file->id;
    const auto result = project.removeNode(file);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(project.findNode(fileId), nullptr);
    EXPECT_TRUE(std::get<Project::Node::Folder>(root->entry).children.empty());
}

TEST_F(ProjectTest, RemoveFolderIsRecursive) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* root = project.getRoot();
    auto* folder = project.addFolder(root, "group");
    auto* a = project.addFile(folder, fs::path { "/tmp/a.bas" });
    auto* b = project.addFile(folder, fs::path { "/tmp/b.bas" });
    const auto aId = a->id;
    const auto bId = b->id;
    const auto folderId = folder->id;

    ASSERT_TRUE(project.removeNode(folder).has_value());
    EXPECT_EQ(project.findNode(folderId), nullptr);
    EXPECT_EQ(project.findNode(aId), nullptr);
    EXPECT_EQ(project.findNode(bId), nullptr);
}

TEST_F(ProjectTest, RemoveNodeDeletesFromDiskWhenRequested) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto path = fs::path(m_tmp.path().ToStdString()) / "doomed.bas";
    { std::ofstream { path } << "x"; }
    ASSERT_TRUE(fs::exists(path));

    auto* file = project.addFile(project.getRoot(), path);
    const auto result = project.removeNode(file, /*deleteOnDisk=*/true);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(fs::exists(path));
}

TEST_F(ProjectTest, RemoveNodeLeavesDiskAloneByDefault) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto path = fs::path(m_tmp.path().ToStdString()) / "kept.bas";
    { std::ofstream { path } << "x"; }

    auto* file = project.addFile(project.getRoot(), path);
    ASSERT_TRUE(project.removeNode(file).has_value());
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(ProjectTest, RemoveRealFolderDeletesRecursivelyFromDisk) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto dir = fs::path(m_tmp.path().ToStdString()) / "tree";
    fs::create_directory(dir);
    { std::ofstream { dir / "a.bas" } << "x"; }

    auto* folder = project.addRealFolder(project.getRoot(), dir).value();
    project.addFile(folder, dir / "a.bas");

    ASSERT_TRUE(project.removeNode(folder, /*deleteOnDisk=*/true).has_value());
    EXPECT_FALSE(fs::exists(dir));
}

// --- moveNode --------------------------------------------------------------

TEST_F(ProjectTest, MoveReordersWithinSameParent) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* root = project.getRoot();
    auto* a = project.addFile(root, fs::path { "/tmp/a.bas" });
    auto* b = project.addFile(root, fs::path { "/tmp/b.bas" });
    auto* c = project.addFile(root, fs::path { "/tmp/c.bas" });
    // [a, b, c] → move a to last
    ASSERT_TRUE(project.moveNode(a, root, 2).has_value());
    const auto& children = std::get<Project::Node::Folder>(root->entry).children;
    ASSERT_EQ(children.size(), 3U);
    EXPECT_EQ(children[0], b);
    EXPECT_EQ(children[1], c);
    EXPECT_EQ(children[2], a);
}

TEST_F(ProjectTest, MoveReparentsBetweenVirtualFolders) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* src = project.addFolder(project.getRoot(), "src");
    auto* lib = project.addFolder(project.getRoot(), "lib");
    auto* file = project.addFile(src, fs::path { "/tmp/foo.bas" });

    ASSERT_TRUE(project.moveNode(file, lib, 0).has_value());
    EXPECT_EQ(file->parent, lib);
    EXPECT_TRUE(std::get<Project::Node::Folder>(src->entry).children.empty());
    const auto& libChildren = std::get<Project::Node::Folder>(lib->entry).children;
    ASSERT_EQ(libChildren.size(), 1U);
    EXPECT_EQ(libChildren.front(), file);
    // Virtual parents: file path is untouched.
    EXPECT_EQ(file->path, fs::path { "/tmp/foo.bas" });
}

TEST_F(ProjectTest, MoveAutoMvsFileBetweenRealFolders) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto base = fs::path(m_tmp.path().ToStdString());
    fs::create_directory(base / "src");
    fs::create_directory(base / "lib");
    { std::ofstream { base / "src" / "foo.bas" } << "x"; }

    auto* src = project.addRealFolder(project.getRoot(), base / "src").value();
    auto* lib = project.addRealFolder(project.getRoot(), base / "lib").value();
    auto* file = project.addFile(src, base / "src" / "foo.bas");

    ASSERT_TRUE(project.moveNode(file, lib, 0).has_value());
    EXPECT_EQ(file->path, base / "lib" / "foo.bas");
    EXPECT_TRUE(fs::exists(base / "lib" / "foo.bas"));
    EXPECT_FALSE(fs::exists(base / "src" / "foo.bas"));
}

TEST_F(ProjectTest, MoveAutoMvClashFails) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto base = fs::path(m_tmp.path().ToStdString());
    fs::create_directory(base / "src");
    fs::create_directory(base / "lib");
    { std::ofstream { base / "src" / "foo.bas" } << "x"; }
    { std::ofstream { base / "lib" / "foo.bas" } << "y"; } // blocker

    auto* src = project.addRealFolder(project.getRoot(), base / "src").value();
    auto* lib = project.addRealFolder(project.getRoot(), base / "lib").value();
    auto* file = project.addFile(src, base / "src" / "foo.bas");

    const auto result = project.moveNode(file, lib, 0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::Clash);
    // Tree unchanged on failure.
    EXPECT_EQ(file->parent, src);
    EXPECT_TRUE(fs::exists(base / "src" / "foo.bas"));
}

// --- renameNode ------------------------------------------------------------

TEST_F(ProjectTest, RenameVirtualFolder) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* folder = project.addFolder(project.getRoot(), "old");
    ASSERT_TRUE(project.renameNode(folder, "new").has_value());
    EXPECT_EQ(std::get<Project::Node::Folder>(folder->entry).name, "new");
    EXPECT_TRUE(folder->path.empty());
}

TEST_F(ProjectTest, RenameFileOnDisk) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto base = fs::path(m_tmp.path().ToStdString());
    const auto oldPath = base / "old.bas";
    { std::ofstream { oldPath } << "x"; }

    auto* file = project.addFile(project.getRoot(), oldPath);
    ASSERT_TRUE(project.renameNode(file, "new.bas").has_value());
    EXPECT_EQ(file->path, base / "new.bas");
    EXPECT_TRUE(fs::exists(base / "new.bas"));
    EXPECT_FALSE(fs::exists(oldPath));
}

TEST_F(ProjectTest, RenameRealFolderUpdatesDescendantPaths) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto base = fs::path(m_tmp.path().ToStdString());
    fs::create_directory(base / "src");
    { std::ofstream { base / "src" / "foo.bas" } << "x"; }

    auto* folder = project.addRealFolder(project.getRoot(), base / "src").value();
    auto* file = project.addFile(folder, base / "src" / "foo.bas");

    ASSERT_TRUE(project.renameNode(folder, "source").has_value());
    EXPECT_EQ(folder->path, base / "source");
    EXPECT_EQ(std::get<Project::Node::Folder>(folder->entry).name, "source");
    EXPECT_EQ(file->path, base / "source" / "foo.bas");
    EXPECT_TRUE(fs::exists(base / "source" / "foo.bas"));
}

TEST_F(ProjectTest, RenameClashReportsClash) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto base = fs::path(m_tmp.path().ToStdString());
    { std::ofstream { base / "a.bas" } << "1"; }
    { std::ofstream { base / "b.bas" } << "2"; }

    auto* file = project.addFile(project.getRoot(), base / "a.bas");
    const auto result = project.renameNode(file, "b.bas");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::Clash);
    // On failure, file unchanged.
    EXPECT_EQ(file->path, base / "a.bas");
    EXPECT_TRUE(fs::exists(base / "a.bas"));
}

TEST_F(ProjectTest, RenameRejectsEmptyName) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* folder = project.addFolder(project.getRoot(), "old");
    const auto result = project.renameNode(folder, "");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::InvalidName);
}

TEST_F(ProjectTest, RenameRejectsNameWithSeparator) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* folder = project.addFolder(project.getRoot(), "old");
    const auto result = project.renameNode(folder, "sub/folder");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::InvalidName);
}
