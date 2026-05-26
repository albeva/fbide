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
 * Phase 5 wires up the Ephemeral project lifecycle: every FreeBASIC
 * document is bound to a one-source Ephemeral project on creation
 * (`createEphemeral`), unbound and destroyed on type-out
 * (`destroyEphemeral`), and active-project tracking follows the
 * notebook's active tab via `setActiveDocument`. Persistent projects,
 * `closeProject` for user-initiated closure, and the `contains`
 * liveness probe round out the surface area even though their
 * Persistent-side use cases arrive later.
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

    /// Resolve `path` to an open document, opening it if necessary.
    /// Lookup rules, in order:
    ///   1. Already-open `Document` whose path matches → return it
    ///      (no file I/O, no duplicate tab).
    ///   2. *(Persistent, future)* The path is a member of an open
    ///      persistent project → open it and bind to that project.
    ///   3. Fallback: `DocumentManager::openFile` — creates a fresh
    ///      tab which, if the type is FreeBASIC, gets its own
    ///      Ephemeral project via the standard openFile flow.
    /// Returns `nullptr` only when even rule 3 fails (file missing,
    /// load error, etc.). Today rule 2 is unreachable; the hook is
    /// in the right place for Persistent projects to slot in later.
    auto resolveOrOpen(const std::filesystem::path& path) -> Document*;

    /// Allocate an Ephemeral project bound to `doc`. Preconditions:
    /// the document must be unbound (`getProject() == nullptr`) and
    /// FreeBASIC (`getType() == FreeBASIC`). The current path stored
    /// in the document is lifted onto the project's single file node
    /// before the document is rebound to that node — `getFilePath()`
    /// returns the same value either side of the call.
    auto createEphemeral(Document& doc) -> Project&;

    /// Tear down the Ephemeral project bound to `doc`. Unbinds the
    /// document first (so its path is restored to the variant) and
    /// then routes through `closeProject` for the actual cleanup.
    /// Precondition: `doc.getProject()` must be a non-null Ephemeral
    /// project — the caller is expected to check first.
    void destroyEphemeral(Document& doc);

    /// Close a project of any mode: unbind every bound document, ask
    /// `DocumentManager` to close their open tabs, then drop the
    /// project. Iteration skips documents whose back-link has already
    /// drifted away (unbound out-of-band by other code), since closing
    /// those isn't this project's responsibility.
    void closeProject(Project& project);

    /// Is `project` currently owned by this manager? Liveness probe
    /// for callers (e.g. `BuildTask` in Phase 6) holding long-lived
    /// `Project*` references.
    [[nodiscard]] auto contains(const Project* project) const -> bool;

    /// React to a document's type having just been set. Brings the
    /// project binding back in sync with the new type:
    /// - FreeBASIC + no project        → `createEphemeral(doc)`.
    /// - non-FreeBASIC + ephemeral     → `destroyEphemeral(doc)`.
    /// - Persistent projects           → untouched (their members
    ///   survive type changes).
    /// Called by `DocumentManager::onDocumentTypeChanged` after its
    /// own bookkeeping completes.
    void onDocumentTypeChanged(Document& doc);

    /// The currently-active project, or `nullptr` when the active
    /// document has no project bound to it.
    [[nodiscard]] auto getActiveProject() const -> Project* { return m_activeProject; }

    /// Recompute the active project from `doc`. Called by
    /// `DocumentNotebook` on tab change. `nullptr` doc clears the
    /// active project; otherwise it's `doc->getProject()`.
    void setActiveDocument(Document* doc);

private:
    Context& m_ctx;
    std::unordered_map<Project::Id, std::unique_ptr<Project>> m_projects;
    Project* m_activeProject = nullptr;
    /// Declared last so destruction runs first — worker thread stops
    /// and joins before the projects and documents it might race with
    /// go away.
    std::unique_ptr<IntellisenseService> m_intellisense;
};

} // namespace fbide
