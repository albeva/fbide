//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "analyses/intellisense/SourceGraph.hpp"

using namespace fbide;

namespace {
// Opaque identity tags — the graph never dereferences them.
auto doc(const std::uintptr_t id) -> Document* { return reinterpret_cast<Document*>(id); }
const std::filesystem::path kA = "/p/a.bas";
const std::filesystem::path kB = "/p/b.bi";
const std::filesystem::path kC = "/p/c.bi";
const std::filesystem::path kD = "/p/d.bas";
} // namespace

TEST(SourceGraphTests, OpenAndSubmitEnqueuesOnce) {
    SourceGraph graph;
    graph.openDocument(kA, doc(1));
    graph.submit(kA, "source one");
    graph.submit(kA, "source one"); // unchanged content
    EXPECT_NE(graph.takeNext(), nullptr);
    EXPECT_EQ(graph.takeNext(), nullptr); // only enqueued once
}

TEST(SourceGraphTests, ChangedContentReEnqueuesUnchangedDoesNot) {
    SourceGraph graph;
    graph.openDocument(kA, doc(1));
    graph.submit(kA, "v1");
    auto* entry = graph.takeNext();
    ASSERT_NE(entry, nullptr);
    entry->parsedStamp = entry->contentStamp; // simulate a completed parse

    graph.submit(kA, "v1"); // identical -> no work
    EXPECT_EQ(graph.takeNext(), nullptr);

    graph.submit(kA, "v2"); // changed -> re-enqueued
    EXPECT_NE(graph.takeNext(), nullptr);
}

TEST(SourceGraphTests, SingleEntryPerPath) {
    SourceGraph graph;
    auto* first = graph.openDocument(kA, doc(1));
    graph.submit(kA, "x");
    EXPECT_EQ(graph.find(kA), first);
    EXPECT_EQ(graph.size(), 1U);
}

TEST(SourceGraphTests, SetIncludesWiresEdgesAndParents) {
    SourceGraph graph;
    auto* a = graph.openDocument(kA, doc(1));
    const auto created = graph.setIncludes(a, { kB, kC });
    EXPECT_EQ(created.size(), 2U); // both brand new

    auto* b = graph.find(kB);
    auto* c = graph.find(kC);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(a->includes.size(), 2U);
    ASSERT_EQ(b->parents.size(), 1U);
    EXPECT_EQ(b->parents[0], a);
}

TEST(SourceGraphTests, SetIncludesDiffRemovesDropped) {
    SourceGraph graph;
    auto* a = graph.openDocument(kA, doc(1));
    graph.setIncludes(a, { kB, kC });
    graph.setIncludes(a, { kB }); // drop C

    auto* c = graph.find(kC);
    ASSERT_EQ(a->includes.size(), 1U);
    EXPECT_EQ(a->includes[0], graph.find(kB));
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->parents.empty()); // back-link removed
}

TEST(SourceGraphTests, SharedIncludeHasMultipleParents) {
    SourceGraph graph;
    auto* a = graph.openDocument(kA, doc(1));
    auto* c = graph.openDocument(kC, doc(2)); // C is itself an open document
    graph.setIncludes(a, { kB });
    graph.setIncludes(c, { kB });

    auto* b = graph.find(kB);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->parents.size(), 2U);
    EXPECT_EQ(graph.size(), 3U);
}

TEST(SourceGraphTests, CloseCollectsSoleInclude) {
    SourceGraph graph;
    auto* a = graph.openDocument(kA, doc(1));
    graph.setIncludes(a, { kB });
    EXPECT_EQ(graph.size(), 2U);
    graph.closeDocument(kA, doc(1));
    EXPECT_EQ(graph.size(), 0U); // A and its sole include B collected
}

TEST(SourceGraphTests, SharedIncludeSurvivesUntilLastParentCloses) {
    SourceGraph graph;
    auto* a = graph.openDocument(kA, doc(1));
    auto* c = graph.openDocument(kC, doc(2));
    graph.setIncludes(a, { kB });
    graph.setIncludes(c, { kB });

    graph.closeDocument(kA, doc(1));
    EXPECT_EQ(graph.find(kA), nullptr);
    EXPECT_NE(graph.find(kB), nullptr); // still reachable via C
    EXPECT_NE(graph.find(kC), nullptr);

    graph.closeDocument(kC, doc(2));
    EXPECT_EQ(graph.find(kB), nullptr); // now orphaned
    EXPECT_EQ(graph.size(), 0U);
}

TEST(SourceGraphTests, CircularIncludesCollectedSafely) {
    SourceGraph graph;
    auto* a = graph.openDocument(kA, doc(1));
    graph.setIncludes(a, { kB });
    auto* b = graph.find(kB);
    ASSERT_NE(b, nullptr);
    graph.setIncludes(b, { kA }); // A <-> B cycle

    graph.collectOrphans(); // A is owned -> both reachable, none collected
    EXPECT_EQ(graph.size(), 2U);

    graph.closeDocument(kA, doc(1)); // unowned cycle -> both collected, no hang
    EXPECT_EQ(graph.size(), 0U);
}

TEST(SourceGraphTests, CloseWithMismatchedOwnerIsNoop) {
    SourceGraph graph;
    graph.openDocument(kA, doc(1));
    graph.closeDocument(kA, doc(2)); // wrong owner
    EXPECT_NE(graph.find(kA), nullptr);
}

TEST(SourceGraphTests, NormalisesPathKeys) {
    SourceGraph graph;
    auto* a = graph.openDocument("/p/sub/../a.bas", doc(1));
    EXPECT_EQ(graph.find(kA), a); // "/p/sub/../a.bas" normalises to "/p/a.bas"
    EXPECT_EQ(graph.size(), 1U);
}

TEST(SourceGraphTests, SurvivorParentLinkScrubbedWhenParentSwept) {
    SourceGraph graph;
    auto* a = graph.openDocument(kA, doc(1)); // owned, includes B directly
    auto* d = graph.openDocument(kD, doc(2)); // owned, reaches B via C
    graph.setIncludes(a, { kB });
    graph.setIncludes(d, { kC });
    auto* c = graph.find(kC);
    ASSERT_NE(c, nullptr);
    graph.setIncludes(c, { kB });
    auto* b = graph.find(kB);
    ASSERT_NE(b, nullptr);
    ASSERT_EQ(b->parents.size(), 2U); // A and C

    graph.closeDocument(kD, doc(2)); // D, C unreachable -> swept; B survives via A
    EXPECT_EQ(graph.find(kC), nullptr);
    auto* survivor = graph.find(kB);
    ASSERT_NE(survivor, nullptr);
    ASSERT_EQ(survivor->parents.size(), 1U); // C's dangling back-link scrubbed
    EXPECT_EQ(survivor->parents[0], a);
}

TEST(SourceGraphTests, SetIncludesDeduplicatesRepeatedTarget) {
    SourceGraph graph;
    auto* a = graph.openDocument(kA, doc(1));
    const auto created = graph.setIncludes(a, { kB, kB }); // same path twice in one call
    EXPECT_EQ(created.size(), 1U);
    EXPECT_EQ(a->includes.size(), 1U);
    auto* b = graph.find(kB);
    ASSERT_NE(b, nullptr);
    ASSERT_EQ(b->parents.size(), 1U);
    EXPECT_EQ(b->parents[0], a);
    EXPECT_EQ(graph.size(), 2U);
}

TEST(SourceGraphTests, SetIncludesReAddsSurvivedInclude) {
    SourceGraph graph;
    auto* a = graph.openDocument(kA, doc(1));
    graph.setIncludes(a, { kB, kC });
    auto* before = graph.find(kC);
    ASSERT_NE(before, nullptr);
    graph.setIncludes(a, { kB }); // drop C (survives unowned, no parents, not collected here)
    EXPECT_TRUE(before->parents.empty());
    graph.setIncludes(a, { kB, kC }); // re-add C
    EXPECT_EQ(graph.find(kC), before); // same entry reused, not recreated
    EXPECT_EQ(a->includes.size(), 2U);
    ASSERT_EQ(before->parents.size(), 1U);
    EXPECT_EQ(before->parents[0], a);
}

TEST(SourceGraphTests, PureIncludePathsExcludesOwnedDocuments) {
    SourceGraph graph;
    auto* a = graph.openDocument(kA, doc(1)); // owned document
    graph.setIncludes(a, { kB, kC });         // two pure includes

    auto paths = graph.pureIncludePaths();
    std::ranges::sort(paths);
    EXPECT_EQ(paths, (std::vector<std::filesystem::path> { kB, kC })); // includes only, not kA

    // Opening an include as a document drops it from the pure-include set.
    graph.openDocument(kB, doc(2));
    paths = graph.pureIncludePaths();
    EXPECT_EQ(paths, std::vector<std::filesystem::path> { kC });
}

TEST(SourceGraphTests, SetIncludesReAddsCollectedInclude) {
    SourceGraph graph;
    auto* a = graph.openDocument(kA, doc(1));
    graph.setIncludes(a, { kC });
    graph.setIncludes(a, {}); // drop C
    graph.collectOrphans(); // C is now unowned + parentless -> collected
    EXPECT_EQ(graph.find(kC), nullptr);
    const auto created = graph.setIncludes(a, { kC }); // re-add -> brand new entry
    EXPECT_EQ(created.size(), 1U);
    auto* c = graph.find(kC);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->contentStamp, 0U); // fresh, awaiting content
    ASSERT_EQ(c->parents.size(), 1U);
    EXPECT_EQ(c->parents[0], a);
}
