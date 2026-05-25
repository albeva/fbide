//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "ai/ToolDispatchLoop.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {

/// Test-only provider that emits a scripted sequence of (text deltas +
/// tool calls + final status) per `send()` call. Lets the dispatch
/// loop be driven through multi-round scenarios without standing up
/// a real HTTP backend.
class ScriptedProvider final : public AiProvider {
public:
    NO_COPY_AND_MOVE(ScriptedProvider)

    struct ScriptedTurn {
        std::vector<wxString> deltas;
        std::vector<AiToolCall> toolCalls;
        bool ok = true;
        wxString error;
    };

    ScriptedProvider() = default;
    ~ScriptedProvider() override = default;

    void script(std::vector<ScriptedTurn> turns) {
        m_turns = std::move(turns);
        m_cursor = 0;
        m_sendCount = 0;
    }

    void send(const AiRequest& request, ChunkHandler onChunk, ToolCallHandler onToolCall, ResponseHandler onComplete) override {
        m_sendCount++;
        m_lastRequest = request;
        ScriptedTurn turn;
        if (m_cursor < m_turns.size()) {
            turn = std::move(m_turns[m_cursor]);
            m_cursor++;
        }
        for (const auto& delta : turn.deltas) {
            onChunk(delta);
        }
        for (auto& call : turn.toolCalls) {
            onToolCall(std::move(call));
        }
        AiResponse response;
        response.ok = turn.ok;
        response.error = turn.error;
        onComplete(std::move(response));
    }

    [[nodiscard]] auto supportsTools() const -> bool override { return true; }

    [[nodiscard]] auto sendCount() const -> int { return m_sendCount; }
    [[nodiscard]] auto lastRequest() const -> const AiRequest& { return m_lastRequest; }

private:
    std::vector<ScriptedTurn> m_turns;
    std::size_t m_cursor = 0;
    int m_sendCount = 0;
    AiRequest m_lastRequest;
};

/// Captures FinishHandler invocations so the test can assert on the
/// final response after the loop terminates.
struct FinishCapture {
    AiResponse response;
    bool fired = false;

    auto handler() -> ToolDispatchLoop::FinishHandler {
        return [this](AiResponse value) {
            response = std::move(value);
            fired = true;
        };
    }
};

/// A trivial tool invoker that fires the handler synchronously with a
/// canned result whose `content` equals the call name — enough to
/// exercise the dispatch path without needing a real registry.
auto echoToolInvoker() -> ToolDispatchLoop::ToolInvoker {
    return [](AiToolCall call, Tool::ResultHandler handler) {
        auto callName = call.name;
        handler(AiToolResult {
            .toolUseId = std::move(call.id),
            .content = callName,
            .isError = false,
        });
    };
}

/// Build a minimal request — the loop calls `requestFactory` each
/// turn; tests don't need full system / tools content to exercise the
/// dispatch sequencing.
auto bareRequestFactory() -> ToolDispatchLoop::RequestFactory {
    return [] {
        return AiRequest { .model = "test-model" };
    };
}

} // namespace

// ---------------------------------------------------------------------------
// Single-turn termination
// ---------------------------------------------------------------------------

TEST(ToolDispatchLoop, ReplyWithoutToolCallsTerminatesAfterOneSend) {
    ScriptedProvider provider;
    provider.script({ ScriptedProvider::ScriptedTurn {
        .deltas = { "hello world" },
        .toolCalls = {},
        .ok = true,
    } });

    std::vector<AiMessage> history { { .role = AiRole::User, .content = "hi" } };
    ToolDispatchLoop loop(provider, echoToolInvoker());
    FinishCapture finish;
    loop.run(&history, bareRequestFactory(), [](const wxString&) {}, finish.handler());

    EXPECT_TRUE(finish.fired);
    EXPECT_TRUE(finish.response.ok);
    EXPECT_EQ(1, provider.sendCount());
    ASSERT_EQ(2U, history.size()); // user + assistant
    EXPECT_EQ(AiRole::Assistant, history.at(1).role);
    EXPECT_EQ("hello world", history.at(1).content);
    EXPECT_TRUE(history.at(1).toolCalls.empty());
}

TEST(ToolDispatchLoop, ProviderErrorOnFirstTurnFinishesImmediatelyWithoutAppending) {
    ScriptedProvider provider;
    provider.script({ ScriptedProvider::ScriptedTurn {
        .ok = false,
        .error = "boom",
    } });

    std::vector<AiMessage> history { { .role = AiRole::User, .content = "hi" } };
    ToolDispatchLoop loop(provider, echoToolInvoker());
    FinishCapture finish;
    loop.run(&history, bareRequestFactory(), [](const wxString&) {}, finish.handler());

    EXPECT_TRUE(finish.fired);
    EXPECT_FALSE(finish.response.ok);
    EXPECT_EQ("boom", finish.response.error);
    // No assistant turn appended — history stays at just the user message.
    EXPECT_EQ(1U, history.size());
}

// ---------------------------------------------------------------------------
// Tool-driven multi-round dispatch
// ---------------------------------------------------------------------------

TEST(ToolDispatchLoop, ToolCallTriggersSecondSendWithToolResults) {
    ScriptedProvider provider;
    provider.script({
        ScriptedProvider::ScriptedTurn {
            .deltas = { "let me check" },
            .toolCalls = { AiToolCall { .id = "call-1", .name = "read_file", .argumentsJson = R"({"path":"x"})" } },
        },
        ScriptedProvider::ScriptedTurn {
            .deltas = { "done" },
        },
    });

    std::vector<AiMessage> history { { .role = AiRole::User, .content = "hi" } };
    ToolDispatchLoop loop(provider, echoToolInvoker());
    FinishCapture finish;
    loop.run(&history, bareRequestFactory(), [](const wxString&) {}, finish.handler());

    EXPECT_TRUE(finish.fired);
    EXPECT_TRUE(finish.response.ok);
    EXPECT_EQ(2, provider.sendCount());

    // History sequence: user → assistant(text+tool_use) → user(tool_result) → assistant(text).
    ASSERT_EQ(4U, history.size());
    EXPECT_EQ(AiRole::User, history.at(0).role);
    EXPECT_EQ(AiRole::Assistant, history.at(1).role);
    EXPECT_EQ("let me check", history.at(1).content);
    ASSERT_EQ(1U, history.at(1).toolCalls.size());
    EXPECT_EQ("read_file", history.at(1).toolCalls.at(0).name);

    EXPECT_EQ(AiRole::User, history.at(2).role);
    ASSERT_EQ(1U, history.at(2).toolResults.size());
    EXPECT_EQ("call-1", history.at(2).toolResults.at(0).toolUseId);
    EXPECT_EQ("read_file", history.at(2).toolResults.at(0).content);

    EXPECT_EQ(AiRole::Assistant, history.at(3).role);
    EXPECT_EQ("done", history.at(3).content);
}

TEST(ToolDispatchLoop, MultipleToolCallsInOneTurnAllInvoked) {
    ScriptedProvider provider;
    provider.script({
        ScriptedProvider::ScriptedTurn {
            .toolCalls = {
                AiToolCall { .id = "a", .name = "alpha", .argumentsJson = "{}" },
                AiToolCall { .id = "b", .name = "beta", .argumentsJson = "{}" },
            },
        },
        ScriptedProvider::ScriptedTurn {},
    });

    std::vector<AiMessage> history { { .role = AiRole::User, .content = "hi" } };
    ToolDispatchLoop loop(provider, echoToolInvoker());
    FinishCapture finish;
    loop.run(&history, bareRequestFactory(), [](const wxString&) {}, finish.handler());

    EXPECT_TRUE(finish.fired);
    // user + assistant(text+2 tool_use) + user(2 tool_result) + final assistant
    ASSERT_EQ(4U, history.size());
    ASSERT_EQ(2U, history.at(2).toolResults.size());
    EXPECT_EQ("a", history.at(2).toolResults.at(0).toolUseId);
    EXPECT_EQ("b", history.at(2).toolResults.at(1).toolUseId);
}

// ---------------------------------------------------------------------------
// Round cap
// ---------------------------------------------------------------------------

TEST(ToolDispatchLoop, BailsWithErrorAfterReachingRoundCap) {
    ScriptedProvider provider;
    // Each turn emits one tool call — the loop keeps re-sending until
    // the cap fires.
    std::vector<ScriptedProvider::ScriptedTurn> script;
    for (int round = 0; round < ToolDispatchLoop::kMaxRounds + 5; ++round) {
        script.push_back(ScriptedProvider::ScriptedTurn {
            .toolCalls = { AiToolCall { .id = wxString::Format("c%d", round), .name = "loop", .argumentsJson = "{}" } },
        });
    }
    provider.script(std::move(script));

    std::vector<AiMessage> history { { .role = AiRole::User, .content = "hi" } };
    ToolDispatchLoop loop(provider, echoToolInvoker());
    FinishCapture finish;
    loop.run(&history, bareRequestFactory(), [](const wxString&) {}, finish.handler());

    EXPECT_TRUE(finish.fired);
    EXPECT_FALSE(finish.response.ok);
    EXPECT_NE(wxString::npos, finish.response.error.find("cap"));
    // Provider was called kMaxRounds times — the cap kicks in before
    // a kMaxRounds+1-th send.
    EXPECT_EQ(ToolDispatchLoop::kMaxRounds, provider.sendCount());
}

// ---------------------------------------------------------------------------
// Streaming callback
// ---------------------------------------------------------------------------

TEST(ToolDispatchLoop, ForwardsChunkCallbackForEveryDelta) {
    ScriptedProvider provider;
    provider.script({ ScriptedProvider::ScriptedTurn {
        .deltas = { "abc", "def", "ghi" },
    } });

    std::vector<AiMessage> history { { .role = AiRole::User, .content = "hi" } };
    ToolDispatchLoop loop(provider, echoToolInvoker());
    FinishCapture finish;
    std::vector<wxString> received;
    loop.run(
        &history,
        bareRequestFactory(),
        [&received](const wxString& delta) { received.push_back(delta); },
        finish.handler()
    );

    ASSERT_EQ(3U, received.size());
    EXPECT_EQ("abc", received.at(0));
    EXPECT_EQ("def", received.at(1));
    EXPECT_EQ("ghi", received.at(2));
    EXPECT_EQ("abcdefghi", history.back().content);
}
