//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
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
constexpr std::uintptr_t kFakeDocAlt     = 0xBEEF;
constexpr std::uintptr_t kFakeDocMiddle  = 0x1234;

} // namespace

// --- ID types --------------------------------------------------------------

TEST(ProjectIdTest, DefaultIsInvalid) {
    constexpr Project::Id id;
    EXPECT_FALSE(static_cast<bool>(id));
}

TEST(ProjectIdTest, ExplicitValueIsValid) {
    const auto uuid = Uuid::generate();
    const Project::Id id { uuid };
    EXPECT_TRUE(static_cast<bool>(id));
    EXPECT_EQ(id.value(), uuid);
}

TEST(ProjectIdTest, EqualityWithSameUuid) {
    const auto uuid = Uuid::generate();
    const Project::Id lhs { uuid };
    const Project::Id rhs { uuid };
    EXPECT_EQ(lhs, rhs);
}

TEST(ProjectIdTest, InequalityForDistinctUuids) {
    const Project::Id lhs { Uuid::generate() };
    const Project::Id rhs { Uuid::generate() };
    EXPECT_NE(lhs, rhs);
}

TEST(ProjectIdTest, Hashable) {
    std::unordered_map<Project::Id, int> map;
    const Project::Id first { Uuid::generate() };
    const Project::Id second { Uuid::generate() };
    map[first] = 1;
    map[second] = 2;
    EXPECT_EQ(map[first], 1);
    EXPECT_EQ(map[second], 2);
}

TEST(ProjectNodeIdTest, DefaultIsInvalid) {
    constexpr Project::Node::Id id;
    EXPECT_FALSE(static_cast<bool>(id));
}

TEST(ProjectNodeIdTest, EqualityWithSameUuid) {
    const auto uuid = Uuid::generate();
    const Project::Node::Id lhs { uuid };
    const Project::Node::Id rhs { uuid };
    EXPECT_EQ(lhs, rhs);
}

TEST(ProjectNodeIdTest, InequalityForDistinctUuids) {
    const Project::Node::Id lhs { Uuid::generate() };
    const Project::Node::Id rhs { Uuid::generate() };
    EXPECT_NE(lhs, rhs);
}

TEST(ProjectNodeIdTest, Hashable) {
    std::unordered_map<Project::Node::Id, int> map;
    const Project::Node::Id first { Uuid::generate() };
    const Project::Node::Id second { Uuid::generate() };
    map[first] = 1;
    map[second] = 2;
    EXPECT_EQ(map[first], 1);
    EXPECT_EQ(map[second], 2);
}

// --- Project construction --------------------------------------------------

class ProjectTest : public testing::Test {};

TEST_F(ProjectTest, EphemeralMode) {
    const Project project { Project::Mode::Ephemeral };
    EXPECT_TRUE(project.isEphemeral());
    EXPECT_EQ(project.getMode(), Project::Mode::Ephemeral);
}

TEST_F(ProjectTest, PersistentMode) {
    const Project project { Project::Mode::Persistent };
    EXPECT_FALSE(project.isEphemeral());
    EXPECT_EQ(project.getMode(), Project::Mode::Persistent);
}

TEST_F(ProjectTest, IdIsValid) {
    const Project project { Project::Mode::Ephemeral };
    EXPECT_TRUE(static_cast<bool>(project.getId()));
}

TEST_F(ProjectTest, IdsAreUniquePerInstance) {
    const Project lhs { Project::Mode::Ephemeral };
    const Project rhs { Project::Mode::Ephemeral };
    EXPECT_NE(lhs.getId(), rhs.getId());
}

// --- addFile / getNodePath / setNodePath -----------------------------------

TEST_F(ProjectTest, AddFileWithPathYieldsValidNodeId) {
    Project project { Project::Mode::Ephemeral };
    const auto id = project.addFile(fs::path { "/tmp/main.bas" });
    EXPECT_TRUE(static_cast<bool>(id));
    EXPECT_EQ(project.getNodePath(id), fs::path { "/tmp/main.bas" });
}

TEST_F(ProjectTest, AddFileUntitledHasEmptyPath) {
    Project project { Project::Mode::Ephemeral };
    const auto id = project.addFile(std::nullopt);
    EXPECT_TRUE(static_cast<bool>(id));
    EXPECT_TRUE(project.getNodePath(id).empty());
}

TEST_F(ProjectTest, AddFileNodeIdsAreUnique) {
    Project project { Project::Mode::Persistent };
    const auto first = project.addFile(fs::path { "/tmp/a.bas" });
    const auto second = project.addFile(fs::path { "/tmp/b.bas" });
    EXPECT_NE(first, second);
}

TEST_F(ProjectTest, SetNodePathSetsPathOnUntitledNode) {
    Project project { Project::Mode::Ephemeral };
    const auto id = project.addFile(std::nullopt);
    project.setNodePath(id, fs::path { "/tmp/saved.bas" });
    EXPECT_EQ(project.getNodePath(id), fs::path { "/tmp/saved.bas" });
}

TEST_F(ProjectTest, SetNodePathReplacesExistingPath) {
    Project project { Project::Mode::Ephemeral };
    const auto id = project.addFile(fs::path { "/tmp/old.bas" });
    project.setNodePath(id, fs::path { "/tmp/new.bas" });
    EXPECT_EQ(project.getNodePath(id), fs::path { "/tmp/new.bas" });
}

// --- getPrimarySource ------------------------------------------------------

TEST_F(ProjectTest, EphemeralPrimarySourceReturnsBoundDoc) {
    Project project { Project::Mode::Ephemeral };
    auto* doc = fakeDoc(kFakeDocPrimary);
    project.addFile(fs::path { "/tmp/main.bas" }, doc);
    EXPECT_EQ(project.getPrimarySource(), doc);
}

TEST_F(ProjectTest, EphemeralPrimarySourceNullWhenUnbound) {
    Project project { Project::Mode::Ephemeral };
    project.addFile(fs::path { "/tmp/main.bas" });
    EXPECT_EQ(project.getPrimarySource(), nullptr);
}

TEST_F(ProjectTest, EphemeralPrimarySourceNullWhenEmpty) {
    const Project project { Project::Mode::Ephemeral };
    EXPECT_EQ(project.getPrimarySource(), nullptr);
}

// --- getDocuments ----------------------------------------------------------

TEST_F(ProjectTest, GetDocumentsIncludesBound) {
    Project project { Project::Mode::Ephemeral };
    auto* doc = fakeDoc(kFakeDocAlt);
    project.addFile(fs::path { "/tmp/main.bas" }, doc);
    const auto docs = project.getDocuments();
    ASSERT_EQ(docs.size(), 1U);
    EXPECT_EQ(docs.front(), doc);
}

TEST_F(ProjectTest, GetDocumentsSkipsUnbound) {
    Project project { Project::Mode::Persistent };
    project.addFile(fs::path { "/tmp/a.bas" });
    project.addFile(fs::path { "/tmp/b.bas" }, fakeDoc(kFakeDocMiddle));
    project.addFile(fs::path { "/tmp/c.bas" });
    const auto docs = project.getDocuments();
    ASSERT_EQ(docs.size(), 1U);
    EXPECT_EQ(docs.front(), fakeDoc(kFakeDocMiddle));
}

TEST_F(ProjectTest, GetDocumentsEmptyOnFreshProject) {
    const Project project { Project::Mode::Persistent };
    EXPECT_TRUE(project.getDocuments().empty());
}
