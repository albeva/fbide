//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <algorithm>
#include <fstream>
#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <gtest/gtest.h>
#include "compiler/CompilerConfigCatalog.hpp"
#include "config/ConfigManager.hpp"
#include "document/Document.hpp"
#include "workspace/EphemeralProject.hpp"
#include "workspace/Project.hpp"
#include "workspace/ProjectBase.hpp"

using namespace fbide;
namespace fs = std::filesystem;

namespace {

/// RAII scratch directory used as the project root for tests. Auto-cleans
/// on destruction.
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
    constexpr ProjectBase::Id id;
    EXPECT_FALSE(static_cast<bool>(id));
}

TEST(ProjectIdTest, GeneratedIsValid) {
    const auto id = ProjectBase::Id::generate();
    EXPECT_TRUE(static_cast<bool>(id));
}

TEST(ProjectIdTest, GeneratedAreUnique) {
    const auto lhs = ProjectBase::Id::generate();
    const auto rhs = ProjectBase::Id::generate();
    EXPECT_NE(lhs, rhs);
}

TEST(ProjectIdTest, CopyIsEqual) {
    const auto original = ProjectBase::Id::generate();
    const auto copy = original;
    EXPECT_EQ(original, copy);
}

TEST(ProjectIdTest, Hashable) {
    std::unordered_map<ProjectBase::Id, int> map;
    const auto first = ProjectBase::Id::generate();
    const auto second = ProjectBase::Id::generate();
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

TEST(ProjectNodeIdTest, StringRoundTrip) {
    const auto id = Project::Node::Id::generate();
    EXPECT_EQ((Project::Node::Id { id.string() }), id);
}

TEST(ProjectNodeIdTest, FromStringThrowsOnMalformed) {
    const auto make = [] { return Project::Node::Id { "not-a-uuid" }; };
    EXPECT_THROW(make(), std::exception);
}

// --- Fixture ---------------------------------------------------------------

/// Spins up a disk-backed `ConfigManager` + `CompilerConfigCatalog` per test
/// and provides factories for `Project` (rooted at the scratch dir) and
/// `EphemeralProject`, plus fixture-owned `Document`s for binding tests.
class ProjectTest : public testing::Test {
protected:
    void SetUp() override {
        m_tmp.write("config.ini", "version=0.5.0\n");
        m_config = std::make_unique<ConfigManager>(m_tmp.path(), m_tmp.path(), "config.ini");
        m_catalog = std::make_unique<CompilerConfigCatalog>(*m_config);
        m_catalog->reload();
    }

    [[nodiscard]] auto makeEphemeral() -> EphemeralProject { return EphemeralProject { *m_catalog }; }
    [[nodiscard]] auto makePersistent() -> Project { return Project { *m_catalog, *m_config, "TestProject", rootDir() }; }

    /// Project root for persistent projects under test — the scratch dir.
    [[nodiscard]] auto rootDir() const -> fs::path { return fs::path { m_tmp.path().ToStdString() }; }

    /// A fresh editor-less document of the given type, for adoption tests.
    [[nodiscard]] auto makeDoc(DocumentType type = DocumentType::FreeBASIC) -> std::unique_ptr<Document> {
        return std::make_unique<Document>(*m_config, type, nullptr);
    }

    TempDir m_tmp;
    std::unique_ptr<ConfigManager> m_config;
    std::unique_ptr<CompilerConfigCatalog> m_catalog;
};

// --- construction ----------------------------------------------------------

TEST_F(ProjectTest, EphemeralMode) {
    const auto project = makeEphemeral();
    EXPECT_TRUE(project.isEphemeral());
}

TEST_F(ProjectTest, PersistentMode) {
    const auto project = makePersistent();
    EXPECT_FALSE(project.isEphemeral());
}

TEST_F(ProjectTest, IdIsValid) {
    const auto project = makeEphemeral();
    EXPECT_TRUE(static_cast<bool>(project.getId()));
}

TEST_F(ProjectTest, IdsAreUniquePerInstance) {
    const auto lhs = makeEphemeral();
    const auto rhs = makeEphemeral();
    EXPECT_NE(lhs.getId(), rhs.getId());
}

// --- shared ephemeral project ---------------------------------------------

TEST_F(ProjectTest, EphemeralOwnsAdoptedDocuments) {
    auto project = makeEphemeral();
    auto* first = project.adopt(makeDoc());
    auto* second = project.adopt(makeDoc());
    const auto docs = project.getDocuments();
    EXPECT_EQ(docs.size(), 2U);
    EXPECT_EQ(first->getProject(), &project);
    EXPECT_EQ(second->getProject(), &project);
}

TEST_F(ProjectTest, EphemeralRemoveDestroysDocument) {
    auto project = makeEphemeral();
    auto* doc = project.adopt(makeDoc());
    project.setActive(doc);
    project.remove(doc);
    EXPECT_TRUE(project.getDocuments().empty());
}

TEST_F(ProjectTest, EphemeralSourcesAndCapabilitiesFollowActiveFreeBasic) {
    auto project = makeEphemeral();
    auto* doc = project.adopt(makeDoc(DocumentType::FreeBASIC));
    project.setActive(doc);
    EXPECT_EQ(project.getSources(), std::vector<Document*> { doc });
    EXPECT_TRUE(project.getCapabilities() & +ProjectBase::Capability::Compile);
}

TEST_F(ProjectTest, EphemeralNoBuildContextForNonFreeBasicActive) {
    auto project = makeEphemeral();
    auto* doc = project.adopt(makeDoc(DocumentType::Text));
    project.setActive(doc);
    EXPECT_TRUE(project.getSources().empty());
    EXPECT_EQ(project.getCapabilities(), 0U);
}

TEST_F(ProjectTest, EphemeralNoBuildContextWithoutActive) {
    auto project = makeEphemeral();
    project.adopt(makeDoc(DocumentType::FreeBASIC));
    EXPECT_TRUE(project.getSources().empty());
    EXPECT_EQ(project.getCapabilities(), 0U);
}

TEST_F(ProjectTest, EphemeralDocumentOwnsItsPath) {
    auto project = makeEphemeral();
    auto* doc = project.adopt(makeDoc());
    doc->setFilePath(fs::path { "/over/there/b.bas" });
    EXPECT_EQ(doc->getFilePath(), fs::path { "/over/there/b.bas" });
    EXPECT_TRUE(project.isUnderRoot(fs::path { "/anywhere/x.bas" }));
}

// --- persistent root -------------------------------------------------------

TEST_F(ProjectTest, RootIsFolderAnchoredAtRootDir) {
    const auto project = makePersistent();
    auto* root = project.getRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->isFolder());
    EXPECT_EQ(root->path, rootDir());
}

TEST_F(ProjectTest, GetNameReturnsProjectName) {
    const auto project = makePersistent();
    EXPECT_EQ(project.getName(), "TestProject");
}

// --- addFolder -------------------------------------------------------------

TEST_F(ProjectTest, AddFolderCreatesRealDirectory) {
    auto project = makePersistent();
    const auto result = project.addFolder(project.getRoot(), "src");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->path, rootDir() / "src");
    EXPECT_EQ((*result)->parent, project.getRoot());
    EXPECT_TRUE(fs::is_directory(rootDir() / "src"));
}

TEST_F(ProjectTest, AddFolderClashWhenExists) {
    auto project = makePersistent();
    fs::create_directory(rootDir() / "existing");
    const auto result = project.addFolder(project.getRoot(), "existing");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::Clash);
}

TEST_F(ProjectTest, AddFolderNested) {
    auto project = makePersistent();
    auto* outer = project.addFolder(project.getRoot(), "outer").value();
    auto* inner = project.addFolder(outer, "inner").value();
    EXPECT_EQ(inner->parent, outer);
    EXPECT_EQ(inner->path, rootDir() / "outer" / "inner");
    EXPECT_TRUE(fs::is_directory(rootDir() / "outer" / "inner"));
}

TEST_F(ProjectTest, AddFolderNullParentDefaultsToRoot) {
    auto project = makePersistent();
    auto* folder = project.addFolder(nullptr, "x").value();
    EXPECT_EQ(folder->parent, project.getRoot());
}

TEST_F(ProjectTest, AddFolderRejectsInvalidName) {
    auto project = makePersistent();
    EXPECT_EQ(project.addFolder(project.getRoot(), "").error(), Project::Error::InvalidName);
    EXPECT_EQ(project.addFolder(project.getRoot(), "a/b").error(), Project::Error::InvalidName);
}

// --- addFile ---------------------------------------------------------------

TEST_F(ProjectTest, AddFileCreatesEmptyFile) {
    auto project = makePersistent();
    const auto result = project.addFile(project.getRoot(), "main.bas");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->path, rootDir() / "main.bas");
    EXPECT_TRUE((*result)->isFile());
    EXPECT_TRUE(fs::is_regular_file(rootDir() / "main.bas"));
}

TEST_F(ProjectTest, AddFileClashWhenExists) {
    auto project = makePersistent();
    { std::ofstream { rootDir() / "main.bas" } << "x"; }
    const auto result = project.addFile(project.getRoot(), "main.bas");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::Clash);
}

TEST_F(ProjectTest, AddFileNullParentDefaultsToRoot) {
    auto project = makePersistent();
    auto* file = project.addFile(nullptr, "a.bas").value();
    EXPECT_EQ(file->parent, project.getRoot());
}

// --- addExisting -----------------------------------------------------------

TEST_F(ProjectTest, AddExistingFileUnderRoot) {
    auto project = makePersistent();
    { std::ofstream { rootDir() / "a.bas" } << "x"; }
    auto* node = project.addExisting(rootDir() / "a.bas").value();
    EXPECT_TRUE(node->isFile());
    EXPECT_EQ(node->parent, project.getRoot());
    EXPECT_EQ(node->path, rootDir() / "a.bas");
}

TEST_F(ProjectTest, AddExistingRejectsOutOfTree) {
    auto project = makePersistent();
    const auto result = project.addExisting(fs::path { "/somewhere/else/a.bas" });
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::OutOfTree);
}

TEST_F(ProjectTest, AddExistingCreatesIntermediaryFolders) {
    auto project = makePersistent();
    fs::create_directories(rootDir() / "a" / "b");
    { std::ofstream { rootDir() / "a" / "b" / "c.bas" } << "x"; }
    auto* node = project.addExisting(rootDir() / "a" / "b" / "c.bas").value();
    auto* folderB = node->parent;
    auto* folderA = folderB->parent;
    EXPECT_EQ(folderB->path, rootDir() / "a" / "b");
    EXPECT_EQ(folderA->path, rootDir() / "a");
    EXPECT_EQ(folderA->parent, project.getRoot());
    EXPECT_TRUE(folderA->isFolder());
    EXPECT_TRUE(folderB->isFolder());
}

TEST_F(ProjectTest, AddExistingFolderAddsNodeOnlyNotContents) {
    auto project = makePersistent();
    fs::create_directory(rootDir() / "grp");
    { std::ofstream { rootDir() / "grp" / "inside.bas" } << "x"; }
    auto* node = project.addExisting(rootDir() / "grp").value();
    EXPECT_TRUE(node->isFolder());
    EXPECT_TRUE(node->getFolder()->children.empty());
}

TEST_F(ProjectTest, AddExistingIdempotent) {
    auto project = makePersistent();
    { std::ofstream { rootDir() / "a.bas" } << "x"; }
    auto* first = project.addExisting(rootDir() / "a.bas").value();
    auto* second = project.addExisting(rootDir() / "a.bas").value();
    EXPECT_EQ(first, second);
}

// --- findNode / findByPath -------------------------------------------------

TEST_F(ProjectTest, FindNodeReturnsAddedNode) {
    auto project = makePersistent();
    auto* node = project.addFile(project.getRoot(), "a.bas").value();
    EXPECT_EQ(project.findNode(node->id), node);
}

TEST_F(ProjectTest, FindNodeReturnsNullForUnknownId) {
    const auto project = makePersistent();
    EXPECT_EQ(project.findNode(Project::Node::Id::generate()), nullptr);
}

TEST_F(ProjectTest, FindByPathReturnsNode) {
    auto project = makePersistent();
    auto* node = project.addFile(project.getRoot(), "a.bas").value();
    EXPECT_EQ(project.findByPath(rootDir() / "a.bas"), node);
}

TEST_F(ProjectTest, FindByPathNullForUnknown) {
    auto project = makePersistent();
    EXPECT_EQ(project.findByPath(rootDir() / "nope.bas"), nullptr);
}

// --- removeNode ------------------------------------------------------------
// NOTE: RemoveMode::AndTrash is intentionally not exercised here — it would
// move scratch files into the real OS trash bin. Its disk side-effect is
// platform code validated manually; the tree-side removal is covered below.

TEST_F(ProjectTest, RemoveFileFromProjectOnlyLeavesDisk) {
    auto project = makePersistent();
    auto* file = project.addFile(project.getRoot(), "a.bas").value();
    const auto fileId = file->id;
    ASSERT_TRUE(project.removeNode(file).has_value());
    EXPECT_EQ(project.findNode(fileId), nullptr);
    EXPECT_TRUE(project.getRoot()->getFolder()->children.empty());
    EXPECT_TRUE(fs::exists(rootDir() / "a.bas"));
}

TEST_F(ProjectTest, RemoveFolderIsRecursive) {
    auto project = makePersistent();
    auto* folder = project.addFolder(project.getRoot(), "grp").value();
    auto* fileA = project.addFile(folder, "a.bas").value();
    auto* fileB = project.addFile(folder, "b.bas").value();
    const auto folderId = folder->id;
    const auto aId = fileA->id;
    const auto bId = fileB->id;
    ASSERT_TRUE(project.removeNode(folder).has_value());
    EXPECT_EQ(project.findNode(folderId), nullptr);
    EXPECT_EQ(project.findNode(aId), nullptr);
    EXPECT_EQ(project.findNode(bId), nullptr);
}

// --- moveNode --------------------------------------------------------------

TEST_F(ProjectTest, MoveReparentsAndMovesOnDisk) {
    auto project = makePersistent();
    auto* src = project.addFolder(project.getRoot(), "src").value();
    auto* lib = project.addFolder(project.getRoot(), "lib").value();
    auto* file = project.addFile(src, "foo.bas").value();
    ASSERT_TRUE(project.moveNode(file, lib).has_value());
    EXPECT_EQ(file->parent, lib);
    EXPECT_EQ(file->path, rootDir() / "lib" / "foo.bas");
    EXPECT_TRUE(fs::exists(rootDir() / "lib" / "foo.bas"));
    EXPECT_FALSE(fs::exists(rootDir() / "src" / "foo.bas"));
    EXPECT_TRUE(src->getFolder()->children.empty());
}

TEST_F(ProjectTest, MoveClashFails) {
    auto project = makePersistent();
    auto* src = project.addFolder(project.getRoot(), "src").value();
    auto* lib = project.addFolder(project.getRoot(), "lib").value();
    project.addFile(src, "foo.bas");
    project.addFile(lib, "foo.bas"); // blocker at destination
    auto* file = project.findByPath(rootDir() / "src" / "foo.bas");
    ASSERT_NE(file, nullptr);
    const auto result = project.moveNode(file, lib);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::Clash);
    EXPECT_EQ(file->parent, src);
    EXPECT_TRUE(fs::exists(rootDir() / "src" / "foo.bas"));
}

TEST_F(ProjectTest, MoveFolderRewritesDescendantPaths) {
    auto project = makePersistent();
    auto* src = project.addFolder(project.getRoot(), "src").value();
    auto* dst = project.addFolder(project.getRoot(), "dst").value();
    auto* inner = project.addFolder(src, "inner").value();
    auto* file = project.addFile(inner, "x.bas").value();
    ASSERT_TRUE(project.moveNode(src, dst).has_value());
    EXPECT_EQ(src->path, rootDir() / "dst" / "src");
    EXPECT_EQ(inner->path, rootDir() / "dst" / "src" / "inner");
    EXPECT_EQ(file->path, rootDir() / "dst" / "src" / "inner" / "x.bas");
    EXPECT_TRUE(fs::exists(rootDir() / "dst" / "src" / "inner" / "x.bas"));
}

// --- renameNode ------------------------------------------------------------

TEST_F(ProjectTest, RenameFileOnDisk) {
    auto project = makePersistent();
    auto* file = project.addFile(project.getRoot(), "old.bas").value();
    ASSERT_TRUE(project.renameNode(file, "new.bas").has_value());
    EXPECT_EQ(file->path, rootDir() / "new.bas");
    EXPECT_TRUE(fs::exists(rootDir() / "new.bas"));
    EXPECT_FALSE(fs::exists(rootDir() / "old.bas"));
}

TEST_F(ProjectTest, RenameFolderUpdatesDescendantPaths) {
    auto project = makePersistent();
    auto* folder = project.addFolder(project.getRoot(), "old").value();
    auto* file = project.addFile(folder, "x.bas").value();
    ASSERT_TRUE(project.renameNode(folder, "new").has_value());
    EXPECT_EQ(folder->path, rootDir() / "new");
    EXPECT_EQ(file->path, rootDir() / "new" / "x.bas");
    EXPECT_TRUE(fs::exists(rootDir() / "new" / "x.bas"));
}

TEST_F(ProjectTest, RenameClashReportsClash) {
    auto project = makePersistent();
    auto* fileA = project.addFile(project.getRoot(), "a.bas").value();
    project.addFile(project.getRoot(), "b.bas");
    const auto result = project.renameNode(fileA, "b.bas");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::Clash);
    EXPECT_EQ(fileA->path, rootDir() / "a.bas");
}

TEST_F(ProjectTest, RenameRejectsInvalidName) {
    auto project = makePersistent();
    auto* folder = project.addFolder(project.getRoot(), "old").value();
    EXPECT_EQ(project.renameNode(folder, "").error(), Project::Error::InvalidName);
    EXPECT_EQ(project.renameNode(folder, "sub/folder").error(), Project::Error::InvalidName);
}

// --- sorting ---------------------------------------------------------------

TEST_F(ProjectTest, ChildrenSortedFoldersFirstThenByName) {
    auto project = makePersistent();
    project.addFile(project.getRoot(), "zebra.bas");
    project.addFile(project.getRoot(), "apple.bas");
    project.addFolder(project.getRoot(), "mango");
    project.addFolder(project.getRoot(), "banana");
    const auto& children = project.getRoot()->getFolder()->children;
    ASSERT_EQ(children.size(), 4U);
    // Folders first (name-sorted), then files (name-sorted).
    EXPECT_EQ(children[0]->name(), "banana");
    EXPECT_EQ(children[1]->name(), "mango");
    EXPECT_EQ(children[2]->name(), "apple.bas");
    EXPECT_EQ(children[3]->name(), "zebra.bas");
}

// --- contextActions --------------------------------------------------------

TEST_F(ProjectTest, ContextActionsRootHasAddsNoRemove) {
    const auto project = makePersistent();
    const auto actions = project.contextActions(project.getRoot());
    EXPECT_NE(std::ranges::find(actions, Project::Action::AddFolder), actions.end());
    EXPECT_NE(std::ranges::find(actions, Project::Action::AddSourceFile), actions.end());
    EXPECT_NE(std::ranges::find(actions, Project::Action::AddHeaderFile), actions.end());
    EXPECT_NE(std::ranges::find(actions, Project::Action::AddExisting), actions.end());
    EXPECT_NE(std::ranges::find(actions, Project::Action::Settings), actions.end());
    EXPECT_EQ(std::ranges::find(actions, Project::Action::Remove), actions.end());
}

TEST_F(ProjectTest, ContextActionsFolderHasAddsAndRemove) {
    auto project = makePersistent();
    auto* folder = project.addFolder(project.getRoot(), "f").value();
    const auto actions = project.contextActions(folder);
    EXPECT_NE(std::ranges::find(actions, Project::Action::AddFolder), actions.end());
    EXPECT_NE(std::ranges::find(actions, Project::Action::Remove), actions.end());
    // Settings is a root-only action.
    EXPECT_EQ(std::ranges::find(actions, Project::Action::Settings), actions.end());
}

TEST_F(ProjectTest, ContextActionsFileHasRemoveOnly) {
    auto project = makePersistent();
    auto* file = project.addFile(project.getRoot(), "a.bas").value();
    const auto actions = project.contextActions(file);
    ASSERT_EQ(actions.size(), 1U);
    EXPECT_EQ(actions.front(), Project::Action::Remove);
}

// --- file-node documents ---------------------------------------------------

TEST_F(ProjectTest, GetDocumentsReflectsFileNodes) {
    auto project = makePersistent();
    auto* file = project.addFile(project.getRoot(), "main.bas").value();
    const auto docs = project.getDocuments();
    ASSERT_EQ(docs.size(), 1U);
    ASSERT_NE(file->document(), nullptr);
    EXPECT_EQ(docs.front(), file->document());
    EXPECT_EQ(file->document()->getProject(), &project);
}

TEST_F(ProjectTest, GetSourcesFiltersByExtension) {
    auto project = makePersistent();
    project.addFile(project.getRoot(), "a.bas");
    project.addFile(project.getRoot(), "notes.txt");
    const auto sources = project.getSources();
    ASSERT_EQ(sources.size(), 1U);
    EXPECT_EQ(sources.front()->getFilePath().filename(), "a.bas");
}

TEST_F(ProjectTest, GetDocumentsEmptyOnFreshProject) {
    const auto project = makePersistent();
    EXPECT_TRUE(project.getDocuments().empty());
}

// --- setFilePath -----------------------------------------------------------

TEST_F(ProjectTest, SetFilePathRekeysIndex) {
    auto project = makePersistent();
    auto* file = project.addFile(project.getRoot(), "a.bas").value();
    ASSERT_TRUE(project.setFilePath(file, rootDir() / "b.bas").has_value());
    EXPECT_EQ(file->path, rootDir() / "b.bas");
    EXPECT_EQ(project.findByPath(rootDir() / "b.bas"), file);
    EXPECT_EQ(project.findByPath(rootDir() / "a.bas"), nullptr);
}

TEST_F(ProjectTest, SetFilePathRejectsOutOfTree) {
    auto project = makePersistent();
    auto* file = project.addFile(project.getRoot(), "a.bas").value();
    const auto result = project.setFilePath(file, fs::path { "/elsewhere/x.bas" });
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::OutOfTree);
}

// --- isUnderRoot -----------------------------------------------------------

TEST_F(ProjectTest, IsUnderRootTrueForChild) {
    const auto project = makePersistent();
    EXPECT_TRUE(project.isUnderRoot(rootDir() / "sub" / "a.bas"));
    EXPECT_TRUE(project.isUnderRoot({}));
}

TEST_F(ProjectTest, IsUnderRootFalseForOutside) {
    const auto project = makePersistent();
    EXPECT_FALSE(project.isUnderRoot(fs::path { "/totally/elsewhere/a.bas" }));
}

TEST_F(ProjectTest, IsUnderRootEphemeralAcceptsEverything) {
    const auto project = makeEphemeral();
    EXPECT_TRUE(project.isUnderRoot(fs::path { "/anywhere/foo.bas" }));
}

// --- capabilities ----------------------------------------------------------

TEST_F(ProjectTest, PersistentAdvertisesNoCapabilities) {
    const auto project = makePersistent();
    EXPECT_EQ(project.getCapabilities(), 0U);
}

// --- .fbp persistence ------------------------------------------------------

TEST_F(ProjectTest, SaveAndReloadPreservesNameTreeAndIds) {
    const auto fbp = rootDir() / "proj.fbp";
    Project::Node::Id srcId;
    Project::Node::Id mainId;
    Project::Node::Id readmeId;
    {
        auto project = makePersistent();
        project.setName("My Game");
        auto* src = project.addFolder(project.getRoot(), "src").value();
        auto* main = project.addFile(src, "main.bas").value();
        auto* readme = project.addFile(project.getRoot(), "README.md").value();
        srcId = src->id;
        mainId = main->id;
        readmeId = readme->id;
        ASSERT_TRUE(project.saveTo(fbp).has_value());
    }

    const auto loaded = Project::loadFrom(fbp, *m_catalog, *m_config);
    ASSERT_TRUE(loaded.has_value());
    auto& project = **loaded;

    EXPECT_EQ(project.getName(), "My Game");

    const auto* src = project.findNode(srcId);
    ASSERT_NE(src, nullptr);
    EXPECT_TRUE(src->isFolder());
    EXPECT_EQ(src->name(), "src");
    EXPECT_EQ(src->path, rootDir() / "src");
    EXPECT_EQ(src->parent, project.getRoot());

    const auto* main = project.findNode(mainId);
    ASSERT_NE(main, nullptr);
    EXPECT_TRUE(main->isFile());
    EXPECT_EQ(main->parent, src);
    EXPECT_EQ(main->path, rootDir() / "src" / "main.bas");

    const auto* readme = project.findNode(readmeId);
    ASSERT_NE(readme, nullptr);
    EXPECT_EQ(readme->parent, project.getRoot());
    EXPECT_EQ(project.findByPath(rootDir() / "src" / "main.bas"), main);
}

TEST_F(ProjectTest, LoadFromMissingFileIsIoError) {
    const auto result = Project::loadFrom(rootDir() / "nope.fbp", *m_catalog, *m_config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::IoError);
}

TEST_F(ProjectTest, LoadFromMalformedIsFormatError) {
    const auto fbp = rootDir() / "bad.fbp";
    { std::ofstream { fbp } << "[folders]\nnot-a-uuid=src\n"; }
    const auto result = Project::loadFrom(fbp, *m_catalog, *m_config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Project::Error::FormatError);
}

TEST_F(ProjectTest, MutationAutoSavesWhenLoaded) {
    const auto fbp = rootDir() / "auto.fbp";
    { std::ofstream { fbp } << "format=1\nname=Auto\n"; }
    auto project = Project::loadFrom(fbp, *m_catalog, *m_config).value();
    ASSERT_TRUE(project->addFolder(project->getRoot(), "lib").has_value());

    const auto reloaded = Project::loadFrom(fbp, *m_catalog, *m_config);
    ASSERT_TRUE(reloaded.has_value());
    const auto* root = (*reloaded)->getRoot();
    ASSERT_EQ(root->getFolder()->children.size(), 1U);
    EXPECT_EQ(root->getFolder()->children.front()->name(), "lib");
}
