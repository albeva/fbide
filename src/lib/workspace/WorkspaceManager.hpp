//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "Project.hpp"

namespace fbide {
class Document;

/**
 * Owns every open `Project` in the IDE and tracks which one (if any)
 * the user is currently focused on.
 *
 * This phase ships the skeleton only — the manager is a member of
 * `Context`, all `Project`s live in `m_projects`, and the active-project
 * accessor is hooked up — but nothing yet creates projects, so the
 * collection stays empty at runtime and `getActiveProject()` always
 * returns `nullptr`. Behaviour preservation, not surface area, is the
 * Phase 2 goal.
 *
 * Subsequent phases grow the API: Phase 3 absorbs the shared
 * `IntellisenseService` from `DocumentManager`; Phase 5 adds
 * `createEphemeral` / `destroyEphemeral` / `closeProject` and the
 * lifecycle wiring; Phase 6 adds `contains` for `BuildTask` liveness
 * probes; Phase 7 adds `resolveOrOpen` for the error-navigation path.
 *
 * **Owns:** `m_projects` (every open `Project`).
 * **Owned by:** `Context`.
 * **Threading:** UI thread only. (Phase 3 onwards the intellisense
 * worker lives here but runs on its own thread.)
 * **Field order in `Context`:** declared *after* `DocumentManager` so
 * destruction runs *before* it — once `IntellisenseService` moves in
 * (Phase 3), the worker must join before the documents it might race
 * with go away.
 *
 * See @ref project-refactor.
 */
class WorkspaceManager final {
public:
    NO_COPY_AND_MOVE(WorkspaceManager)

    /// Construct an empty workspace. The application `Context` will be
    /// passed in starting at Phase 3 when `IntellisenseService` moves
    /// in and Phase 5 wires up project lifecycle — neither needs it
    /// yet, so the ctor stays argument-free for now.
    WorkspaceManager() = default;

    /// Default destructor — `Project` is complete via the header include,
    /// so `unique_ptr<Project>` teardown can stay inline.
    ~WorkspaceManager() = default;

    /// The currently-active project, or `nullptr` when the active
    /// document has no project bound to it (always `nullptr` until
    /// Phase 5 wires the lifecycle).
    [[nodiscard]] auto getActiveProject() const -> Project* { return m_activeProject; }

    /// Recompute the active project from `doc`. Called by
    /// `DocumentManager` on tab change. Stub in this phase — once
    /// documents start carrying a `Project*` back-link (Phase 4 / 5),
    /// this resolves to `doc != nullptr ? doc->getProject() : nullptr`.
    void setActiveDocument(Document* doc);

private:
    std::unordered_map<Project::Id, std::unique_ptr<Project>> m_projects;
    Project* m_activeProject = nullptr;
};

} // namespace fbide
