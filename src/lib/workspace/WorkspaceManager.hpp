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
 * Owns every open `Project` and the shared background
 * `IntellisenseService`; resolves the active project lazily from the
 * active document.
 *
 * **Destruction order:** declared after `DocumentManager` in `Context`
 * so it tears down first — the intellisense worker must stop and join
 * before the documents it may race with go away. Inside this class,
 * `m_intellisense` is the last field for the same reason.
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
    auto createEphemeral(Document& doc) -> Project*;

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

    /// Resolve `id` to a live `Project*`, or `nullptr` if no project
    /// with that id is currently owned. Exposed for the serialisation
    /// boundary and external references — internal callers hold
    /// `Project*` directly and validate liveness via `contains` instead.
    [[nodiscard]] auto find(Project::Id id) -> Project*;

    /// Liveness probe for a raw `Project*` held across async work.
    /// Safe to call with a stale (possibly destroyed) pointer — does
    /// not dereference `project`. Returns true when the pointer still
    /// names an owned project.
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

    /// The currently-active project, or `nullptr` when no document is
    /// active or the active document has no project bound to it.
    /// Resolved lazily through `DocumentManager::getActive()` —
    /// keeping the value cached invited stale-pointer bugs when a
    /// type-change destroyed-then-recreated the ephemeral project
    /// without an intervening tab change to refresh the cache.
    [[nodiscard]] auto getActiveProject() const -> Project*;

private:
    Context& m_ctx;
    std::unordered_map<Project::Id, std::unique_ptr<Project>> m_projects;
    /// Declared last so destruction runs first — worker thread stops
    /// and joins before the projects and documents it might race with
    /// go away.
    std::unique_ptr<IntellisenseService> m_intellisense;
};

} // namespace fbide
