//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ai/AiTypes.hpp"

namespace fbide::ai {

/**
 * One model-invocable tool: declares its `AiTool` descriptor (name,
 * description, JSON-schema) and runs the call when the model asks for
 * it. `invoke` is async — sync tools (like `read_file`) call the
 * handler inline; async tools (`compile`, future Phase 5) call it when
 * their work finishes. Always on the UI thread.
 */
class Tool {
public:
    NO_COPY_AND_MOVE(Tool)
    Tool() = default;
    virtual ~Tool() = default;

    /// Callback fired when the tool's invocation finishes. Always on
    /// the UI thread. Called exactly once per `invoke`.
    using ResultHandler = std::function<void(AiToolResult)>;

    /// The wire descriptor — `AiManager::buildRequest` collects these
    /// to populate `AiRequest::tools` for the provider.
    [[nodiscard]] virtual auto descriptor() const -> AiTool = 0;

    /// Run the call. The tool reports its outcome through `handler`
    /// with the same `toolUseId` as `call.id`; on failure set
    /// `isError = true` so the model gets a structured retry cue.
    virtual void invoke(AiToolCall call, ResultHandler handler) = 0;
};

/**
 * Owns the set of model-invocable tools and dispatches calls by name.
 * Owned by `AiManager`. Construction is one-shot — tools are
 * registered up front and never unregistered.
 */
class ToolRegistry final {
public:
    NO_COPY_AND_MOVE(ToolRegistry)
    ToolRegistry() = default;
    ~ToolRegistry() = default;

    /// Append a tool. Tools are kept in registration order — the order
    /// shows up in `descriptors()` and matters only insofar as the
    /// model may anchor on it.
    void add(std::unique_ptr<Tool> tool);

    /// Descriptors for every registered tool, in registration order.
    /// Empty when no tools are registered.
    [[nodiscard]] auto descriptors() const -> std::vector<AiTool>;

    /// Look up `call.name` and forward to the matching tool. If no
    /// tool is registered with that name, invokes `handler` with an
    /// `isError = true` result naming the unknown tool — the model
    /// can then retry with a known one.
    void invoke(AiToolCall call, Tool::ResultHandler handler);

    /// Number of registered tools — for tests and the chat-panel
    /// "tools enabled" hint.
    [[nodiscard]] auto size() const -> std::size_t { return m_tools.size(); }

private:
    std::vector<std::unique_ptr<Tool>> m_tools;
};

} // namespace fbide::ai
