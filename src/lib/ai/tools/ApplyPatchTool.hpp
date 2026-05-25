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
 * `apply_patch(search, replace)` — let the model edit the pinned edit
 * target directly with a SEARCH/REPLACE patch.
 *
 * **Gating.** Refuses unless agent mode is on AND an edit target is
 * pinned. Both checks return `isError = true` so the model sees a
 * structured failure cue rather than guessing.
 *
 * **Coupling.** The tool calls into the host through three callables
 * supplied at construction (agent-mode probe, edit-target probe,
 * apply function) so it can be tested without standing up a full
 * `AiManager` / `DocumentManager`.
 *
 * **Match feedback.** The result JSON reports `applied` or `no_match`.
 * Phase 6 will surface finer `match_kind` (`trimmed_newline`,
 * `normalised_whitespace`, ...) once the matcher gains those modes.
 */
class ApplyPatchTool final : public Tool {
public:
    NO_COPY_AND_MOVE(ApplyPatchTool)

    /// Tool name as the model invokes it. Public so the registry,
    /// gating predicate, and tests can refer to it without string
    /// duplication.
    static constexpr auto kName = "apply_patch";

    /// Host-side hooks. All called on the UI thread.
    struct Hooks {
        std::function<bool()> isAgentMode;                                ///< True iff agent mode toggle is on.
        std::function<bool()> hasEditTarget;                              ///< True iff an edit target is pinned.
        std::function<bool(const wxString&, const wxString&)> applyPatch; ///< Returns true on successful apply.
    };

    explicit ApplyPatchTool(Hooks hooks);
    ~ApplyPatchTool() override = default;

    [[nodiscard]] auto descriptor() const -> AiTool override;
    void invoke(AiToolCall call, ResultHandler handler) override;

private:
    Hooks m_hooks;
};

} // namespace fbide::ai
