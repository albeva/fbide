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
class Context;
class Document;
class IntellisenseService;

/**
 * Owns every open `Project` in the IDE, the shared background
 * `IntellisenseService`, and the bookkeeping for which project (if any)
 * the user is currently focused on.
 *
 * Phase 3 adds the `IntellisenseService` ownership; project lifecycle
 * (`createEphemeral` / `destroyEphemeral` / `closeProject`), liveness
 * (`contains`), and file resolution (`resolveOrOpen`) arrive in later
 * phases — nothing yet creates projects, so the collection stays empty
 * and `getActiveProject()` always returns `nullptr`.
 *
 * **Owns:** `m_projects` (every open `Project`) and `m_intellisense`
 * (the background lex/parse worker shared across all FreeBASIC docs).
 * **Owned by:** `Context`.
 * **Threading:** UI thread only. The intellisense worker lives here
 * but runs on its own thread; `WorkspaceManager` itself doesn't cross
 * threads.
 * **Field order in `Context`:** declared *after* `DocumentManager` so
 * destruction runs *before* it — the intellisense worker must stop
 * and join before the documents it might race with go away.
 * **Field order inside this class:** `m_intellisense` is declared
 * *last* so its destructor (which joins the worker) runs *first*,
 * before the project map it might post results about goes away.
 *
 * See @ref project-refactor.
 */
class WorkspaceManager final {
public:
    NO_COPY_AND_MOVE(WorkspaceManager)

    /// Construct an empty workspace bound to the application `Context`
    /// and spin up the background intellisense worker. The worker
    /// posts results to the `DocumentManager` as its sink (already
    /// constructed by the time `Context` reaches this line).
    explicit WorkspaceManager(Context& ctx);

    /// Out-of-line so the destructor sees the full `IntellisenseService`
    /// definition when `m_intellisense` tears down (the type is only
    /// forward-declared in this header).
    ~WorkspaceManager();

    /// Access the shared background intellisense worker.
    [[nodiscard]] auto getIntellisense() -> IntellisenseService& { return *m_intellisense; }

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
    /// Declared last so destruction runs first — worker thread stops
    /// and joins before the projects and documents it might race with
    /// go away.
    std::unique_ptr<IntellisenseService> m_intellisense;
};

} // namespace fbide
