//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/tools/ToolRegistry.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {

/// Minimal in-test Tool. `descriptor()` returns the name passed at
/// construction; `invoke()` either calls the handler with a canned
/// result (sync path) or stores it for the test to fire later (async
/// path the dispatch loop will need in Phase 5).
class StubTool final : public Tool {
public:
    NO_COPY_AND_MOVE(StubTool)
    explicit StubTool(wxString name, wxString reply)
    : m_name(std::move(name))
    , m_reply(std::move(reply)) {}
    ~StubTool() override = default;

    [[nodiscard]] auto descriptor() const -> AiTool override {
        return AiTool { .name = m_name, .description = "stub", .inputSchemaJson = "{}" };
    }

    void invoke(AiToolCall call, ResultHandler handler) override {
        m_lastCall = call;
        handler(AiToolResult {
            .toolUseId = std::move(call.id),
            .content = m_reply,
            .isError = false,
        });
    }

    [[nodiscard]] auto lastCall() const -> const AiToolCall& { return m_lastCall; }

private:
    wxString m_name;
    wxString m_reply;
    AiToolCall m_lastCall;
};

} // namespace

// ---------------------------------------------------------------------------
// Registration + descriptor enumeration
// ---------------------------------------------------------------------------

TEST(ToolRegistry, EmptyRegistryHasNoDescriptors) {
    const ToolRegistry registry;
    EXPECT_EQ(0U, registry.size());
    EXPECT_TRUE(registry.descriptors().empty());
}

TEST(ToolRegistry, AddedToolsAppearInDescriptorsInRegistrationOrder) {
    ToolRegistry registry;
    registry.add(std::make_unique<StubTool>("alpha", "first"));
    registry.add(std::make_unique<StubTool>("beta", "second"));

    ASSERT_EQ(2U, registry.size());
    const auto descriptors = registry.descriptors();
    ASSERT_EQ(2U, descriptors.size());
    EXPECT_EQ("alpha", descriptors.at(0).name);
    EXPECT_EQ("beta", descriptors.at(1).name);
}

// ---------------------------------------------------------------------------
// invoke dispatch
// ---------------------------------------------------------------------------

TEST(ToolRegistry, InvokeRoutesByNameAndForwardsCallId) {
    ToolRegistry registry;
    auto tool = std::make_unique<StubTool>("alpha", "ok");
    auto* observer = tool.get();
    registry.add(std::move(tool));

    AiToolResult captured;
    registry.invoke(
        AiToolCall { .id = "call-1", .name = "alpha", .argumentsJson = "{}" },
        [&](AiToolResult result) { captured = std::move(result); }
    );

    EXPECT_EQ("call-1", observer->lastCall().id);
    EXPECT_EQ("call-1", captured.toolUseId);
    EXPECT_EQ("ok", captured.content);
    EXPECT_FALSE(captured.isError);
}

TEST(ToolRegistry, InvokeUnknownToolReturnsErrorResult) {
    ToolRegistry registry;
    registry.add(std::make_unique<StubTool>("known", "ok"));

    AiToolResult captured;
    registry.invoke(
        AiToolCall { .id = "call-2", .name = "missing", .argumentsJson = "{}" },
        [&](AiToolResult result) { captured = std::move(result); }
    );

    EXPECT_TRUE(captured.isError);
    EXPECT_EQ("call-2", captured.toolUseId);
    EXPECT_NE(wxString::npos, captured.content.find("missing"));
}
