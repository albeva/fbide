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
#include "document/Document.hpp"
#include "workspace/Project.hpp"

using namespace fbide;
namespace fs = std::filesystem;

namespace {

/// RAII scratch directory used to back a `ConfigManager` for tests that
/// need a `Project` instance. Auto-cleans on destruction.
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

/// `Project` takes a `ConfigManager&` so its build-input getters can forward
/// to the IDE config. The fixture spins up a minimal disk-backed
/// `ConfigManager` per test, and a doc factory that creates real
/// `Document` instances (also `ConfigManager`-driven) so the
/// `Project`/`Document` binding can be exercised without the wider
/// `Context` chain.
class ProjectTest : public testing::Test {
protected:
    void SetUp() override {
        m_tmp.write("config.ini", "version=0.5.0\n");
        m_config = std::make_unique<ConfigManager>(m_tmp.path(), m_tmp.path(), "config.ini");
    }

    [[nodiscard]] auto makeProject(Project::Mode mode) -> Project {
        return Project { *m_config, mode };
    }

    /// Spawn a Document owned by the fixture so its lifetime outlives the
    /// project under test. Returned by reference because Documents are
    /// non-movable.
    auto makeDoc() -> Document& {
        m_docs.emplace_back(std::make_unique<Document>(*m_config, DocumentType::FreeBASIC, nullptr));
        return *m_docs.back();
    }

    /// Convenience: doc seeded with a file path before any project bind.
    auto makeDoc(const fs::path& path) -> Document& {
        auto& doc = makeDoc();
        doc.setFilePath(path);
        return doc;
    }

    TempDir m_tmp;
    std::unique_ptr<ConfigManager> m_config;
    std::vector<std::unique_ptr<Document>> m_docs;
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

// --- Ephemeral root: File-as-root -----------------------------------------

TEST_F(ProjectTest, EphemeralRootIsNullBeforeAddFile) {
    const auto project = makeProject(Project::Mode::Ephemeral);
    EXPECT_EQ(project.getRoot(), nullptr);
}

TEST_F(ProjectTest, EphemeralRootIsTheFileAfterAddFile) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc(fs::path { "/tmp/main.bas" });
    auto* node = project.addFile(&doc);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(project.getRoot(), node);
    EXPECT_TRUE(node->isFile());
    EXPECT_EQ(node->parent, nullptr);
    EXPECT_EQ(node->path, fs::path { "/tmp/main.bas" });
}

TEST_F(ProjectTest, EphemeralRootPathMatchesSavedFile) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc(fs::path { "/foo/bar/baz.bas" });
    project.addFile(&doc);
    EXPECT_EQ(project.getRoot()->path, fs::path { "/foo/bar/baz.bas" });
}

TEST_F(ProjectTest, EphemeralRootPathEmptyForUntitled) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc();
    project.addFile(&doc);
    EXPECT_TRUE(project.getRoot()->path.empty());
}

TEST_F(ProjectTest, EphemeralSetFilePathUpdatesRoot) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc(fs::path { "/old/a.bas" });
    auto* file = project.addFile(&doc);
    ASSERT_TRUE(project.setFilePath(file, fs::path { "/new/b.bas" }).has_value());
    EXPECT_EQ(project.getRoot()->path, fs::path { "/new/b.bas" });
}

// --- Persistent root + child linking --------------------------------------

TEST_F(ProjectTest, PersistentRootIsEmptyFolder) {
    const auto project = makeProject(Project::Mode::Persistent);
    auto* root = project.getRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->isFolder());
    EXPECT_TRUE(root->path.empty());
}

TEST_F(ProjectTest, PersistentChildIsLinkedUnderRoot) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* root = project.getRoot();
    ASSERT_NE(root, nullptr);
    auto& doc = makeDoc(fs::path { "/tmp/a.bas" });
    auto* child = project.addFile(&doc, root);
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->parent, root);
    const auto& children = root->getFolder()->children;
    ASSERT_EQ(children.size(), 1U);
    EXPECT_EQ(children.front(), child);
}

TEST_F(ProjectTest, AddFileNodesAreUnique) {
    auto project = makeProject(Project::Mode::Persistent);
    auto& a = makeDoc(fs::path { "/tmp/a.bas" });
    auto& b = makeDoc(fs::path { "/tmp/b.bas" });
    auto* first = project.addFile(&a, project.getRoot());
    auto* second = project.addFile(&b, project.getRoot());
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
}

// --- addFile binding behaviour --------------------------------------------

TEST_F(ProjectTest, AddFileBindsDocumentToProject) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc(fs::path { "/tmp/main.bas" });
    project.addFile(&doc);
    EXPECT_EQ(doc.getProject(), &project);
    EXPECT_EQ(doc.getFilePath(), fs::path { "/tmp/main.bas" });
}

TEST_F(ProjectTest, AddFileUntitledHasEmptyPath) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc();
    auto* node = project.addFile(&doc);
    ASSERT_NE(node, nullptr);
    EXPECT_TRUE(node->path.empty());
}

// --- setFilePath ----------------------------------------------------------

TEST_F(ProjectTest, SetFilePathSetsPathOnUntitledNode) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc();
    auto* node = project.addFile(&doc);
    ASSERT_TRUE(project.setFilePath(node, fs::path { "/tmp/saved.bas" }).has_value());
    EXPECT_EQ(node->path, fs::path { "/tmp/saved.bas" });
}

TEST_F(ProjectTest, SetFilePathReplacesExistingPath) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc(fs::path { "/tmp/old.bas" });
    auto* node = project.addFile(&doc);
    ASSERT_TRUE(project.setFilePath(node, fs::path { "/tmp/new.bas" }).has_value());
    EXPECT_EQ(node->path, fs::path { "/tmp/new.bas" });
}

TEST_F(ProjectTest, EphemeralSetFilePathNeverRejectsOutOfTree) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc(fs::path { "/here/foo.bas" });
    auto* file = project.addFile(&doc);
    // Even moving to a totally different directory works — Ephemeral
    // root follows the file.
    ASSERT_TRUE(project.setFilePath(file, fs::path { "/over/there/bar.bas" }).has_value());
    EXPECT_EQ(file->path, fs::path { "/over/there/bar.bas" });
}

// --- findNode -------------------------------------------------------------

TEST_F(ProjectTest, FindNodeReturnsAddedNode) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc(fs::path { "/tmp/main.bas" });
    auto* node = project.addFile(&doc);
    EXPECT_EQ(project.findNode(node->id), node);
}

TEST_F(ProjectTest, FindNodeReturnsNullForUnknownId) {
    const auto project = makeProject(Project::Mode::Ephemeral);
    const auto stranger = Project::Node::Id::generate();
    EXPECT_EQ(project.findNode(stranger), nullptr);
}

// --- getDocuments / getSources --------------------------------------------

TEST_F(ProjectTest, GetDocumentsIncludesBound) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc(fs::path { "/tmp/main.bas" });
    project.addFile(&doc);
    const auto docs = project.getDocuments();
    ASSERT_EQ(docs.size(), 1U);
    EXPECT_EQ(docs.front(), &doc);
}

TEST_F(ProjectTest, GetDocumentsEmptyOnFreshProject) {
    const auto project = makeProject(Project::Mode::Persistent);
    EXPECT_TRUE(project.getDocuments().empty());
}

TEST_F(ProjectTest, GetDocumentsSkipsClearedBacklink) {
    auto project = makeProject(Project::Mode::Persistent);
    auto& a = makeDoc(fs::path { "/tmp/a.bas" });
    auto& b = makeDoc(fs::path { "/tmp/b.bas" });
    auto& c = makeDoc(fs::path { "/tmp/c.bas" });
    auto* nodeA = project.addFile(&a, project.getRoot());
    project.addFile(&b, project.getRoot());
    auto* nodeC = project.addFile(&c, project.getRoot());
    // Drop the back-link on the outer two; only `b` should remain in the list.
    project.clearNodeDocument(nodeA);
    project.clearNodeDocument(nodeC);
    const auto docs = project.getDocuments();
    ASSERT_EQ(docs.size(), 1U);
    EXPECT_EQ(docs.front(), &b);
}

TEST_F(ProjectTest, GetSourcesEphemeralReturnsBoundDoc) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc(fs::path { "/tmp/main.bas" });
    project.addFile(&doc);
    const auto sources = project.getSources();
    ASSERT_EQ(sources.size(), 1U);
    EXPECT_EQ(sources.front(), &doc);
}

TEST_F(ProjectTest, GetSourcesEphemeralEmptyBeforeAddFile) {
    const auto project = makeProject(Project::Mode::Ephemeral);
    EXPECT_TRUE(project.getSources().empty());
}

TEST_F(ProjectTest, GetSourcesPersistentFiltersByExtension) {
    auto project = makeProject(Project::Mode::Persistent);
    auto& bas = makeDoc(fs::path { "/tmp/a.bas" });
    auto& txt = makeDoc(fs::path { "/tmp/notes.txt" });
    project.addFile(&bas, project.getRoot());
    project.addFile(&txt, project.getRoot());
    const auto sources = project.getSources();
    ASSERT_EQ(sources.size(), 1U);
    EXPECT_EQ(sources.front(), &bas);
}

// --- clearNodeDocument ----------------------------------------------------

TEST_F(ProjectTest, ClearNodeDocumentDropsBackLink) {
    auto project = makeProject(Project::Mode::Ephemeral);
    auto& doc = makeDoc(fs::path { "/tmp/main.bas" });
    auto* node = project.addFile(&doc);
    project.clearNodeDocument(node);
    EXPECT_TRUE(project.getDocuments().empty());
}

// --- capabilities ---------------------------------------------------------

TEST_F(ProjectTest, EphemeralAdvertisesAllCapabilities) {
    const auto project = makeProject(Project::Mode::Ephemeral);
    const auto caps = project.getCapabilities();
    EXPECT_TRUE(caps & +Project::Capability::Compile);
    EXPECT_TRUE(caps & +Project::Capability::CompileAndRun);
    EXPECT_TRUE(caps & +Project::Capability::Run);
    EXPECT_TRUE(caps & +Project::Capability::QuickRun);
}

// --- addFolder / addRealFolder -------------------------------------------

TEST_F(ProjectTest, AddFolderVirtualUnderRoot) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* folder = project.addFolder(project.getRoot(), "Sources");
    ASSERT_NE(folder, nullptr);
    EXPECT_TRUE(folder->path.empty());
    EXPECT_EQ(folder->getFolder()->name, "Sources");
    EXPECT_EQ(folder->parent, project.getRoot());
}

TEST_F(ProjectTest, AddFolderNested) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* outer = project.addFolder(project.getRoot(), "outer");
    auto* inner = project.addFolder(outer, "inner");
    EXPECT_EQ(inner->parent, outer);
    const auto& outerChildren = outer->getFolder()->children;
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

// --- removeNode ----------------------------------------------------------

TEST_F(ProjectTest, RemoveFileDetachesFromParent) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* root = project.getRoot();
    auto& doc = makeDoc(fs::path { "/tmp/a.bas" });
    auto* file = project.addFile(&doc, root);
    const auto fileId = file->id;
    // removeNode asserts the file has no bound document — clear the
    // back-link first so the project- and document-side state stay symmetric
    // (real callers go through Document::unbindFromProject first).
    project.clearNodeDocument(file);
    const auto result = project.removeNode(file);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(project.findNode(fileId), nullptr);
    EXPECT_TRUE(root->getFolder()->children.empty());
}

TEST_F(ProjectTest, RemoveFolderIsRecursive) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* root = project.getRoot();
    auto* folder = project.addFolder(root, "group");
    auto& docA = makeDoc(fs::path { "/tmp/a.bas" });
    auto& docB = makeDoc(fs::path { "/tmp/b.bas" });
    auto* a = project.addFile(&docA, folder);
    auto* b = project.addFile(&docB, folder);
    const auto aId = a->id;
    const auto bId = b->id;
    const auto folderId = folder->id;

    project.clearNodeDocument(a);
    project.clearNodeDocument(b);
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

    auto& doc = makeDoc(path);
    auto* file = project.addFile(&doc, project.getRoot());
    project.clearNodeDocument(file);
    const auto result = project.removeNode(file, /*deleteOnDisk=*/true);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(fs::exists(path));
}

TEST_F(ProjectTest, RemoveNodeLeavesDiskAloneByDefault) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto path = fs::path(m_tmp.path().ToStdString()) / "kept.bas";
    { std::ofstream { path } << "x"; }

    auto& doc = makeDoc(path);
    auto* file = project.addFile(&doc, project.getRoot());
    project.clearNodeDocument(file);
    ASSERT_TRUE(project.removeNode(file).has_value());
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(ProjectTest, RemoveRealFolderDeletesRecursivelyFromDisk) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto dir = fs::path(m_tmp.path().ToStdString()) / "tree";
    fs::create_directory(dir);
    { std::ofstream { dir / "a.bas" } << "x"; }

    auto* folder = project.addRealFolder(project.getRoot(), dir).value();
    auto& doc = makeDoc(dir / "a.bas");
    auto* file = project.addFile(&doc, folder);
    project.clearNodeDocument(file);

    ASSERT_TRUE(project.removeNode(folder, /*deleteOnDisk=*/true).has_value());
    EXPECT_FALSE(fs::exists(dir));
}

// --- moveNode ------------------------------------------------------------

TEST_F(ProjectTest, MoveReordersWithinSameParent) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* root = project.getRoot();
    auto& dA = makeDoc(fs::path { "/tmp/a.bas" });
    auto& dB = makeDoc(fs::path { "/tmp/b.bas" });
    auto& dC = makeDoc(fs::path { "/tmp/c.bas" });
    auto* a = project.addFile(&dA, root);
    auto* b = project.addFile(&dB, root);
    auto* c = project.addFile(&dC, root);
    // [a, b, c] → move a to last
    ASSERT_TRUE(project.moveNode(a, root, 2).has_value());
    const auto& children = root->getFolder()->children;
    ASSERT_EQ(children.size(), 3U);
    EXPECT_EQ(children[0], b);
    EXPECT_EQ(children[1], c);
    EXPECT_EQ(children[2], a);
}

TEST_F(ProjectTest, MoveReparentsBetweenVirtualFolders) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* src = project.addFolder(project.getRoot(), "src");
    auto* lib = project.addFolder(project.getRoot(), "lib");
    auto& doc = makeDoc(fs::path { "/tmp/foo.bas" });
    auto* file = project.addFile(&doc, src);

    ASSERT_TRUE(project.moveNode(file, lib, 0).has_value());
    EXPECT_EQ(file->parent, lib);
    EXPECT_TRUE(src->getFolder()->children.empty());
    const auto& libChildren = lib->getFolder()->children;
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
    auto& doc = makeDoc(base / "src" / "foo.bas");
    auto* file = project.addFile(&doc, src);

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
    auto& doc = makeDoc(base / "src" / "foo.bas");
    auto* file = project.addFile(&doc, src);

    const auto result = project.moveNode(file, lib, 0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::Clash);
    // Tree unchanged on failure.
    EXPECT_EQ(file->parent, src);
    EXPECT_TRUE(fs::exists(base / "src" / "foo.bas"));
}

// --- renameNode ----------------------------------------------------------

TEST_F(ProjectTest, RenameVirtualFolder) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* folder = project.addFolder(project.getRoot(), "old");
    ASSERT_TRUE(project.renameNode(folder, "new").has_value());
    EXPECT_EQ(folder->getFolder()->name, "new");
    EXPECT_TRUE(folder->path.empty());
}

TEST_F(ProjectTest, RenameFileOnDisk) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto base = fs::path(m_tmp.path().ToStdString());
    const auto oldPath = base / "old.bas";
    { std::ofstream { oldPath } << "x"; }

    auto& doc = makeDoc(oldPath);
    auto* file = project.addFile(&doc, project.getRoot());
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
    auto& doc = makeDoc(base / "src" / "foo.bas");
    auto* file = project.addFile(&doc, folder);

    ASSERT_TRUE(project.renameNode(folder, "source").has_value());
    EXPECT_EQ(folder->path, base / "source");
    EXPECT_EQ(folder->getFolder()->name, "source");
    EXPECT_EQ(file->path, base / "source" / "foo.bas");
    EXPECT_TRUE(fs::exists(base / "source" / "foo.bas"));
}

TEST_F(ProjectTest, RenameClashReportsClash) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto base = fs::path(m_tmp.path().ToStdString());
    { std::ofstream { base / "a.bas" } << "1"; }
    { std::ofstream { base / "b.bas" } << "2"; }

    auto& doc = makeDoc(base / "a.bas");
    auto* file = project.addFile(&doc, project.getRoot());
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

// --- nullptr parent defaults to root -------------------------------------

TEST_F(ProjectTest, AddFolderNullParentDefaultsToRoot) {
    auto project = makeProject(Project::Mode::Persistent);
    auto* folder = project.addFolder(nullptr, "X");
    EXPECT_EQ(folder->parent, project.getRoot());
}

TEST_F(ProjectTest, AddRealFolderNullParentDefaultsToRoot) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto dir = fs::path(m_tmp.path().ToStdString()) / "auto";
    auto* folder = project.addRealFolder(nullptr, dir).value();
    EXPECT_EQ(folder->parent, project.getRoot());
}

TEST_F(ProjectTest, PersistentAddFileNullParentDefaultsToRoot) {
    auto project = makeProject(Project::Mode::Persistent);
    auto& doc = makeDoc(fs::path { "/tmp/a.bas" });
    auto* file = project.addFile(&doc);
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->parent, project.getRoot());
}

// --- moveNode auto-mv via closest real ancestor --------------------------

TEST_F(ProjectTest, MoveIntoVirtualWalksUpToRealAncestor) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto base = fs::path(m_tmp.path().ToStdString());
    fs::create_directory(base / "lib");
    { std::ofstream { base / "foo.bas" } << "x"; }

    auto* realLib = project.addRealFolder(nullptr, base / "lib").value();
    auto* virtualGroup = project.addFolder(realLib, "Group");
    auto& doc = makeDoc(base / "foo.bas");
    auto* file = project.addFile(&doc, nullptr);

    ASSERT_TRUE(project.moveNode(file, virtualGroup, 0).has_value());
    EXPECT_EQ(file->path, base / "lib" / "foo.bas");
    EXPECT_TRUE(fs::exists(base / "lib" / "foo.bas"));
    EXPECT_FALSE(fs::exists(base / "foo.bas"));
}

TEST_F(ProjectTest, MoveIntoFullyVirtualAncestrySkipsDiskMove) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto base = fs::path(m_tmp.path().ToStdString());
    const auto filePath = base / "stays.bas";
    { std::ofstream { filePath } << "x"; }

    // Persistent root has empty path. The whole ancestry of `nest` is
    // virtual, so the move skips fs::rename.
    auto* outer = project.addFolder(nullptr, "Outer");
    auto* nest = project.addFolder(outer, "Nest");
    auto& doc = makeDoc(filePath);
    auto* file = project.addFile(&doc, nullptr);

    ASSERT_TRUE(project.moveNode(file, nest, 0).has_value());
    EXPECT_EQ(file->path, filePath);
    EXPECT_TRUE(fs::exists(filePath));
}

// --- isUnderRoot ---------------------------------------------------------

TEST_F(ProjectTest, IsUnderRootEmptyRootAcceptsEverything) {
    const auto project = makeProject(Project::Mode::Persistent);
    EXPECT_TRUE(project.isUnderRoot(fs::path { "/anywhere/foo.bas" }));
    EXPECT_TRUE(project.isUnderRoot({}));
}

TEST_F(ProjectTest, IsUnderRootEphemeralPreFileAcceptsEverything) {
    const auto project = makeProject(Project::Mode::Ephemeral);
    // m_root is null before the first addFile — isUnderRoot short-circuits
    // to true rather than null-derefing.
    EXPECT_TRUE(project.isUnderRoot(fs::path { "/anywhere/foo.bas" }));
}

// --- removeNode disk-delete walks descendants ---------------------------

TEST_F(ProjectTest, RemoveVirtualFolderDeletesRealDescendantsOnDisk) {
    auto project = makeProject(Project::Mode::Persistent);
    const auto base = fs::path(m_tmp.path().ToStdString());
    const auto filePath = base / "buried.bas";
    { std::ofstream { filePath } << "x"; }

    auto* virtualGroup = project.addFolder(nullptr, "VG");
    auto& doc = makeDoc(filePath);
    auto* file = project.addFile(&doc, virtualGroup);
    project.clearNodeDocument(file);

    ASSERT_TRUE(project.removeNode(virtualGroup, /*deleteOnDisk=*/true).has_value());
    EXPECT_FALSE(fs::exists(filePath));
}
