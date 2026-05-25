//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <nlohmann/json.hpp>
#include <gtest/gtest.h>
#include "ai/tools/ApplyPatchTool.hpp"
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {

/// Bundle the three hooks ApplyPatchTool needs and the call/result
/// machinery into one fixture so each test reads as a config + invoke
/// + assert sequence.
struct ToolFixture {
    bool agentMode = true;
    bool hasTarget = true;
    bool applyResult = true;
    int applyCalls = 0;
    wxString lastSearch;
    wxString lastReplace;

    auto build() -> ApplyPatchTool {
        return ApplyPatchTool(ApplyPatchTool::Hooks {
            .isAgentMode = [this] { return agentMode; },
            .hasEditTarget = [this] { return hasTarget; },
            .applyPatch = [this](const wxString& search, const wxString& replace) {
                applyCalls++;
                lastSearch = search;
                lastReplace = replace;
                return applyResult; },
        });
    }

    /// Invoke the tool with `argumentsJson` and capture the result.
    auto run(ApplyPatchTool& tool, const wxString& argumentsJson) -> AiToolResult {
        AiToolResult captured;
        tool.invoke(
            AiToolCall { .id = "id-1", .name = "apply_patch", .argumentsJson = argumentsJson },
            [&](AiToolResult result) { captured = std::move(result); }
        );
        return captured;
    }
};

} // namespace

// ---------------------------------------------------------------------------
// Descriptor
// ---------------------------------------------------------------------------

TEST(ApplyPatchTool, DescriptorNamesTheToolAndIncludesBothFieldsInSchema) {
    ToolFixture fx;
    auto tool = fx.build();
    const auto descriptor = tool.descriptor();
    EXPECT_EQ("apply_patch", descriptor.name);
    EXPECT_FALSE(descriptor.description.empty());
    const auto schema = json::parse(descriptor.inputSchemaJson.utf8_string());
    EXPECT_EQ(2U, schema["required"].size());
    EXPECT_TRUE(schema["properties"].contains("search"));
    EXPECT_TRUE(schema["properties"].contains("replace"));
}

// ---------------------------------------------------------------------------
// Gating
// ---------------------------------------------------------------------------

TEST(ApplyPatchTool, RefusesWhenAgentModeOff) {
    ToolFixture fx;
    fx.agentMode = false;
    auto tool = fx.build();

    const auto result = fx.run(tool, R"({"search":"a","replace":"b"})");
    EXPECT_TRUE(result.isError);
    EXPECT_EQ("id-1", result.toolUseId);
    EXPECT_NE(wxString::npos, result.content.find("Agent mode"));
    EXPECT_EQ(0, fx.applyCalls);
}

TEST(ApplyPatchTool, RefusesWhenNoEditTargetPinned) {
    ToolFixture fx;
    fx.hasTarget = false;
    auto tool = fx.build();

    const auto result = fx.run(tool, R"({"search":"a","replace":"b"})");
    EXPECT_TRUE(result.isError);
    EXPECT_NE(wxString::npos, result.content.find("edit target"));
    EXPECT_EQ(0, fx.applyCalls);
}

// ---------------------------------------------------------------------------
// Argument validation
// ---------------------------------------------------------------------------

TEST(ApplyPatchTool, ErrorsOnMalformedArgumentsJson) {
    ToolFixture fx;
    auto tool = fx.build();
    const auto result = fx.run(tool, "not-json");
    EXPECT_TRUE(result.isError);
    EXPECT_EQ(0, fx.applyCalls);
}

TEST(ApplyPatchTool, ErrorsWhenSearchFieldMissing) {
    ToolFixture fx;
    auto tool = fx.build();
    const auto result = fx.run(tool, R"({"replace":"b"})");
    EXPECT_TRUE(result.isError);
    EXPECT_NE(wxString::npos, result.content.find("search"));
    EXPECT_EQ(0, fx.applyCalls);
}

TEST(ApplyPatchTool, ErrorsWhenReplaceFieldNotAString) {
    ToolFixture fx;
    auto tool = fx.build();
    const auto result = fx.run(tool, R"({"search":"a","replace":42})");
    EXPECT_TRUE(result.isError);
    EXPECT_NE(wxString::npos, result.content.find("replace"));
    EXPECT_EQ(0, fx.applyCalls);
}

// ---------------------------------------------------------------------------
// Successful apply / no-match
// ---------------------------------------------------------------------------

TEST(ApplyPatchTool, AppliedResultCarriesStatusAndForwardsBothFields) {
    ToolFixture fx;
    auto tool = fx.build();
    const auto result = fx.run(tool, R"({"search":"old text","replace":"new text"})");

    EXPECT_FALSE(result.isError);
    EXPECT_EQ(1, fx.applyCalls);
    EXPECT_EQ("old text", fx.lastSearch);
    EXPECT_EQ("new text", fx.lastReplace);
    const auto resultJson = json::parse(result.content.utf8_string());
    EXPECT_EQ("applied", resultJson["status"].get<std::string>());
}

TEST(ApplyPatchTool, NoMatchResultMarksIsErrorTrueAndReportsStatus) {
    ToolFixture fx;
    fx.applyResult = false;
    auto tool = fx.build();
    const auto result = fx.run(tool, R"({"search":"missing","replace":"x"})");

    EXPECT_TRUE(result.isError);
    EXPECT_EQ(1, fx.applyCalls);
    const auto resultJson = json::parse(result.content.utf8_string());
    EXPECT_EQ("no_match", resultJson["status"].get<std::string>());
}
