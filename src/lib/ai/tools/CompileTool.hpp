//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ToolRegistry.hpp"

namespace fbide::ai {

/**
 * `compile()` — let the agent verify its edits by triggering a build
 * and reading the captured output.
 *
 * **Gating.** Refuses unless agent mode is on AND the host's
 * "Allow compile" checkbox is enabled. A per-turn cap (host-supplied)
 * prevents the model from looping on compile errors indefinitely
 * within a single user message — once the cap fires, further calls
 * return an `isError` result naming the limit.
 *
 * **Async.** Invokes its result handler when the build finishes (or
 * is cancelled — `BuildTask`'s destructor fires a cancellation
 * result so the dispatch loop doesn't hang). Output is capped at
 * `kMaxOutputBytes` with the middle truncated.
 *
 * **Coupling.** All host integration is via the `Hooks` struct so
 * the tool can be tested without standing up a `CompilerManager`.
 */
class CompileTool final : public Tool {
public:
    NO_COPY_AND_MOVE(CompileTool)

    /// Tool name as the model invokes it. Public so the registry,
    /// gating predicate, and tests can refer to it without string
    /// duplication.
    static constexpr auto kName = "compile";

    /// Maximum captured-output bytes embedded in the tool result.
    /// Larger output is head+tail-truncated with a `[truncated N
    /// bytes]` marker — keeps the model's input budget bounded.
    static constexpr std::size_t kMaxOutputBytes = std::size_t { 16 } * 1024;

    /// Callback fired when the build finishes (or is cancelled).
    using CompilationResultHandler = std::function<void(bool ok, wxArrayString output)>;

    /// Host-side hooks. All called on the UI thread.
    struct Hooks {
        std::function<bool()> isAgentMode;                              ///< True iff agent mode toggle is on.
        std::function<bool()> isAllowCompile;                           ///< True iff the "Allow compile" checkbox is ticked.
        std::function<bool()> tryBumpCompileCount;                      ///< Increments the per-turn counter and returns true iff still under the cap.
        std::function<wxString()> validateReady;                        ///< "" when the target is pinned + saved; otherwise an error sentence.
        std::function<void(CompilationResultHandler)> startCompilation; ///< Kick off the async build.
    };

    explicit CompileTool(Hooks hooks);
    ~CompileTool() override = default;

    [[nodiscard]] auto descriptor() const -> AiTool override;
    void invoke(AiToolCall call, ResultHandler handler) override;

    /// Truncate `output` to `kMaxOutputBytes` keeping the head and tail
    /// with a `[truncated N bytes]` marker in the middle. Exposed for
    /// tests so the truncation logic can be exercised against arbitrary
    /// fixtures without invoking the tool.
    [[nodiscard]] static auto truncateOutput(const wxArrayString& output) -> wxString;

private:
    Hooks m_hooks;
};

} // namespace fbide::ai
