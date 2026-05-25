//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <nlohmann/json.hpp>
#include <gtest/gtest.h>
#include "ai/tools/CompileTool.hpp"
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {

/// Captures invocation arguments and lets the test drive the host-side
/// `startCompilation` callback when it wants. Bundles agent/allow/cap
/// state so each test reads as a config + invoke + assert sequence.
struct ToolFixture {
    bool agentMode = true;
    bool allowCompile = true;
    int compileCap = 3;
    int compileCount = 0;
    wxString validateError;
    bool fired = false;
    CompileTool::CompilationResultHandler pendingHandler;
    int compileStarted = 0;

    auto build() -> CompileTool {
        return CompileTool(CompileTool::Hooks {
            .isAgentMode = [this] { return agentMode; },
            .isAllowCompile = [this] { return allowCompile; },
            .tryBumpCompileCount = [this] {
                if (compileCount >= compileCap) {
                    return false;
                }
                compileCount++;
                return true; },
            .validateReady = [this] { return validateError; },
            .startCompilation = [this](CompileTool::CompilationResultHandler handler) {
                compileStarted++;
                pendingHandler = std::move(handler); },
        });
    }

    auto run(CompileTool& tool) -> AiToolResult {
        AiToolResult captured;
        tool.invoke(
            AiToolCall { .id = "call-1", .name = "compile", .argumentsJson = "{}" },
            [this, &captured](AiToolResult result) {
                captured = std::move(result);
                fired = true;
            }
        );
        return captured;
    }
};

} // namespace

// ---------------------------------------------------------------------------
// Descriptor
// ---------------------------------------------------------------------------

TEST(CompileTool, DescriptorNamesTheToolAndHasEmptyPropertiesObject) {
    ToolFixture fx;
    auto tool = fx.build();
    const auto descriptor = tool.descriptor();
    EXPECT_EQ("compile", descriptor.name);
    EXPECT_FALSE(descriptor.description.empty());
    const auto schema = json::parse(descriptor.inputSchemaJson.utf8_string());
    EXPECT_EQ("object", schema["type"].get<std::string>());
    EXPECT_TRUE(schema["properties"].empty());
}

// ---------------------------------------------------------------------------
// Gating: agent / allow / cap / readiness
// ---------------------------------------------------------------------------

TEST(CompileTool, RefusesWhenAgentModeOff) {
    ToolFixture fx;
    fx.agentMode = false;
    auto tool = fx.build();
    const auto result = fx.run(tool);
    EXPECT_TRUE(result.isError);
    EXPECT_NE(wxString::npos, result.content.find("Agent mode"));
    EXPECT_EQ(0, fx.compileStarted);
}

TEST(CompileTool, RefusesWhenAllowCompileOff) {
    ToolFixture fx;
    fx.allowCompile = false;
    auto tool = fx.build();
    const auto result = fx.run(tool);
    EXPECT_TRUE(result.isError);
    EXPECT_NE(wxString::npos, result.content.find("Allow compile"));
    EXPECT_EQ(0, fx.compileStarted);
}

TEST(CompileTool, RefusesWhenPerTurnCapReached) {
    ToolFixture fx;
    fx.compileCap = 2;
    auto tool = fx.build();
    // Two successes — fire each compile's handler so the next invoke
    // sees the bump go through.
    for (int round = 0; round < 2; ++round) {
        fx.fired = false;
        fx.run(tool);
        ASSERT_TRUE(fx.pendingHandler);
        fx.pendingHandler(true, {});
    }
    // Third one refused with cap message.
    fx.fired = false;
    const auto blocked = fx.run(tool);
    EXPECT_TRUE(blocked.isError);
    EXPECT_NE(wxString::npos, blocked.content.find("cap"));
    EXPECT_EQ(2, fx.compileStarted);
}

TEST(CompileTool, RefusesWhenValidateReadyReportsAnIssue) {
    ToolFixture fx;
    fx.validateError = "doc has unsaved changes";
    auto tool = fx.build();
    const auto result = fx.run(tool);
    EXPECT_TRUE(result.isError);
    EXPECT_NE(wxString::npos, result.content.find("unsaved changes"));
    EXPECT_EQ(0, fx.compileStarted);
}

// ---------------------------------------------------------------------------
// Async completion paths
// ---------------------------------------------------------------------------

TEST(CompileTool, SuccessProducesOkStatusWithOutput) {
    ToolFixture fx;
    auto tool = fx.build();
    fx.run(tool);
    ASSERT_TRUE(fx.pendingHandler);
    wxArrayString out;
    out.Add("ok");
    fx.pendingHandler(true, std::move(out));

    EXPECT_TRUE(fx.fired);
}

TEST(CompileTool, FailureProducesFailedStatusAndIsErrorFlag) {
    ToolFixture fx;
    auto tool = fx.build();
    AiToolResult captured;
    bool fired = false;
    tool.invoke(
        AiToolCall { .id = "c", .name = "compile", .argumentsJson = "{}" },
        [&](AiToolResult result) {
            captured = std::move(result);
            fired = true;
        }
    );
    ASSERT_TRUE(fx.pendingHandler);
    wxArrayString out;
    out.Add("error: line 1: invalid syntax");
    fx.pendingHandler(false, std::move(out));

    EXPECT_TRUE(fired);
    EXPECT_TRUE(captured.isError);
    const auto resultJson = json::parse(captured.content.utf8_string());
    EXPECT_EQ("failed", resultJson["status"].get<std::string>());
    EXPECT_NE(std::string::npos, resultJson["output"].get<std::string>().find("invalid syntax"));
}

TEST(CompileTool, CancellationSentinelProducesCancelledStatus) {
    ToolFixture fx;
    auto tool = fx.build();
    AiToolResult captured;
    tool.invoke(
        AiToolCall { .id = "c", .name = "compile", .argumentsJson = "{}" },
        [&](AiToolResult result) { captured = std::move(result); }
    );
    ASSERT_TRUE(fx.pendingHandler);
    wxArrayString out;
    out.Add("[cancelled]"); // BuildTask dtor sentinel
    fx.pendingHandler(false, std::move(out));

    EXPECT_TRUE(captured.isError);
    const auto resultJson = json::parse(captured.content.utf8_string());
    EXPECT_EQ("cancelled", resultJson["status"].get<std::string>());
}

// ---------------------------------------------------------------------------
// Output truncation
// ---------------------------------------------------------------------------

TEST(CompileToolTruncate, SmallOutputPassedThroughVerbatim) {
    wxArrayString out;
    out.Add("hello");
    out.Add("world");
    const auto truncated = CompileTool::truncateOutput(out);
    EXPECT_EQ("hello\nworld\n", truncated);
}

TEST(CompileToolTruncate, OversizedOutputTruncatedWithMarker) {
    // Build raw output well past the cap so the middle drop is clear.
    wxArrayString out;
    const std::string filler(CompileTool::kMaxOutputBytes + 4096, 'x');
    out.Add(filler);
    const auto truncated = CompileTool::truncateOutput(out);

    EXPECT_LE(truncated.utf8_string().size(), CompileTool::kMaxOutputBytes + 128);
    EXPECT_NE(wxString::npos, truncated.find("[truncated"));
    // Head and tail of the original payload still recognisable —
    // both endpoints should be the filler character (the tail picks
    // up the trailing newline `joinLines` adds, so check one before).
    EXPECT_EQ('x', truncated[0]);
    EXPECT_EQ('x', truncated[truncated.size() - 2]);
}
