//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ProjectBase.hpp"

namespace fbide {
class Context;
class Document;
class Project;
class IntellisenseService;

/**
 * Owns every open `ProjectBase` and the shared background
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

    /// The single front door for opening a path. Canonicalises, then
    /// dispatches by extension:
    ///   - `.fbs` → load a session (`FileSession::load`); returns nullptr.
    ///   - `.fbp` → load a persistent project (`loadProject`); returns nullptr.
    ///   - otherwise → an ordinary document:
    ///       1. already-open document with that path → select + return it;
    ///       2. *(future)* a member of the open persistent project → open
    ///          and bind to it;
    ///       3. fallback → `DocumentManager::openDocument`, which creates a
    ///          tab and, for FreeBASIC, an Ephemeral project to host it.
    /// Returns the document for the ordinary case (nullptr on failure), and
    /// nullptr for the session / project dispatches.
    auto openFile(const std::filesystem::path& path) -> Document*;

    /// Show the open-file dialog and route every selected path through
    /// `openFile`.
    void openFile();

    /// Load a persistent project from a `.fbp` file. When a different
    /// project is already open, prompts the user to close it, open the new
    /// project in a separate window, or cancel. Re-opening the already-open
    /// project is a no-op that returns it. Returns the loaded project, or
    /// nullptr when the user cancels or chooses a new window.
    auto loadProject(const std::filesystem::path& path) -> Project*;

    /// The currently open persistent project, or nullptr when none is open.
    [[nodiscard]] auto getProject() const -> Project* { return m_project; }

    /// Allocate an Ephemeral project bound to `doc`. Preconditions:
    /// the document must be unbound (`getProject() == nullptr`) and
    /// FreeBASIC (`getType() == FreeBASIC`). The current path stored
    /// in the document is lifted onto the project's single file node
    /// before the document is rebound to that node — `getFilePath()`
    /// returns the same value either side of the call.
    auto createEphemeral(Document& doc) -> ProjectBase*;

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
    void closeProject(ProjectBase& project);

    /// Resolve `id` to a live `ProjectBase*`, or `nullptr` if no project
    /// with that id is currently owned. Exposed for the serialisation
    /// boundary and external references — internal callers hold
    /// `ProjectBase*` directly and validate liveness via `contains` instead.
    [[nodiscard]] auto find(ProjectBase::Id id) -> ProjectBase*;

    /// Liveness probe for a raw `ProjectBase*` held across async work.
    /// Safe to call with a stale (possibly destroyed) pointer — does
    /// not dereference `project`. Returns true when the pointer still
    /// names an owned project.
    [[nodiscard]] auto contains(const ProjectBase* project) const -> bool;

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
    [[nodiscard]] auto getActiveProject() const -> ProjectBase*;

private:
    Context& m_ctx;
    std::unordered_map<ProjectBase::Id, std::unique_ptr<ProjectBase>> m_projects;
    /// The open persistent project (owned in `m_projects`; this is just an
    /// O(1) handle) and its `.fbp` path for same-file re-open detection.
    /// Null / empty when no project is open.
    Project* m_project = nullptr;
    std::filesystem::path m_projectPath;
    /// Declared last so destruction runs first — worker thread stops
    /// and joins before the projects and documents it might race with
    /// go away.
    std::unique_ptr<IntellisenseService> m_intellisense;
};

} // namespace fbide
