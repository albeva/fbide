//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/AiTypes.hpp"
#include "ai/provider/MockProvider.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {

/// Build a one-message AiRequest with `prompt` as the user content —
/// every pickReply test wants the same shape.
auto requestFor(const wxString& prompt) -> AiRequest {
    AiRequest request;
    request.messages.push_back({ .role = AiRole::User, .content = prompt });
    return request;
}

/// True when `text` starts with `prefix` — used to assert the dispatch
/// table lands on the right canned reply without comparing the entire
/// blob byte-for-byte (the reply text is large and may evolve).
auto startsWith(const wxString& text, const wxString& prefix) -> bool {
    return text.StartsWith(prefix);
}

} // namespace

// ---------------------------------------------------------------------------
// Each known command lands on its dedicated reply.
// ---------------------------------------------------------------------------

TEST(MockProviderPickReply, EmptyRequestReturnsDefaultReply) {
    AiRequest request;
    const auto picked = MockProvider::pickReply(request);
    EXPECT_TRUE(startsWith(picked.text, "Here is a"));
    EXPECT_FALSE(picked.fast);
}

TEST(MockProviderPickReply, UnknownPromptReturnsDefaultReply) {
    const auto picked = MockProvider::pickReply(requestFor("anything else entirely"));
    EXPECT_TRUE(startsWith(picked.text, "Here is a"));
}

TEST(MockProviderPickReply, HelpMapsToHelpReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("help")).text, "**Mock provider**"));
}

TEST(MockProviderPickReply, ListIsAnAliasForHelp) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("list")).text, "**Mock provider**"));
}

TEST(MockProviderPickReply, QuestionMarkIsAnAliasForHelp) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("?")).text, "**Mock provider**"));
}

TEST(MockProviderPickReply, ShortMapsToShortReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("short")).text, "Got it"));
}

TEST(MockProviderPickReply, LongMapsToLongReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("long")).text, "# Long mock reply"));
}

TEST(MockProviderPickReply, TextMapsToTextReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("text")).text, "Plain prose"));
}

TEST(MockProviderPickReply, FbMapsToFbReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("fb")).text, "A small **FreeBASIC**"));
}

TEST(MockProviderPickReply, FreebasicIsAnAliasForFb) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("freebasic")).text, "A small **FreeBASIC**"));
}

TEST(MockProviderPickReply, BigCodeMapsToBigCodeReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("bigcode")).text, "A longer **FreeBASIC**"));
}

TEST(MockProviderPickReply, BigIsAnAliasForBigCode) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("big")).text, "A longer **FreeBASIC**"));
}

TEST(MockProviderPickReply, LongFbIsAnAliasForBigCode) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("long fb")).text, "A longer **FreeBASIC**"));
}

TEST(MockProviderPickReply, JsonMapsToJsonReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("json")).text, "Example JSON"));
}

TEST(MockProviderPickReply, TableMapsToTableReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("table")).text, "Feature status"));
}

TEST(MockProviderPickReply, EmojiMapsToEmojiReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("emoji")).text, "Status report"));
}

TEST(MockProviderPickReply, TasksMapsToTasksReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("tasks")).text, "### v0.6"));
}

TEST(MockProviderPickReply, TodoIsAnAliasForTasks) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("todo")).text, "### v0.6"));
}

TEST(MockProviderPickReply, SetextMapsToSetextReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("setext")).text, "Setext-style"));
}

TEST(MockProviderPickReply, HeadingsIsAnAliasForSetext) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("headings")).text, "Setext-style"));
}

TEST(MockProviderPickReply, ImagesMapsToImagesReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("images")).text, "A few screenshots"));
}

TEST(MockProviderPickReply, GalleryIsAnAliasForImages) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("gallery")).text, "A few screenshots"));
}

TEST(MockProviderPickReply, PatchMapsToPatchReply) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("patch")).text, "Tightening the greeting"));
}

TEST(MockProviderPickReply, AllMapsToConcatenatedReply) {
    const auto picked = MockProvider::pickReply(requestFor("all"));
    // The concatenated reply starts with the help blob.
    EXPECT_TRUE(startsWith(picked.text, "**Mock provider**"));
    EXPECT_FALSE(picked.fast);
}

TEST(MockProviderPickReply, AllFastSkipsStreaming) {
    EXPECT_TRUE(MockProvider::pickReply(requestFor("allf")).fast);
}

TEST(MockProviderPickReply, AllSpaceFastIsAnAliasForAllFast) {
    EXPECT_TRUE(MockProvider::pickReply(requestFor("all fast")).fast);
}

// ---------------------------------------------------------------------------
// Normalisation — case + surrounding whitespace.
// ---------------------------------------------------------------------------

TEST(MockProviderPickReply, MatchIsCaseInsensitive) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("HELP")).text, "**Mock provider**"));
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("Help")).text, "**Mock provider**"));
}

TEST(MockProviderPickReply, MatchIgnoresSurroundingWhitespace) {
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("  help  ")).text, "**Mock provider**"));
    EXPECT_TRUE(startsWith(MockProvider::pickReply(requestFor("\thelp\n")).text, "**Mock provider**"));
}
