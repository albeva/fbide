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
class EphemeralProject;
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
    ///   - otherwise → an ordinary document: opens (or focuses) it. A path
    ///     that belongs to the open persistent project opens under that
    ///     project; anything else opens as a standalone document in the
    ///     shared ephemeral project.
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

    /// Create a brand-new empty project: prompt for the `.fbp` save location,
    /// then (if a project is already open) confirm and close it first, write the
    /// empty project file, and open it. Returns the new project, or nullptr when
    /// the user cancels at any step.
    auto newProject() -> Project*;

    /// The currently open persistent project, or nullptr when none is open.
    [[nodiscard]] auto getProject() const -> Project* { return m_project; }

    /// The shared, session-long ephemeral project that owns every standalone
    /// document. Lazily created on first use (the compiler catalog isn't
    /// available at construction time) and permanent thereafter.
    [[nodiscard]] auto ephemeral() -> EphemeralProject&;

    /// Hand a freshly-created standalone document to the shared ephemeral
    /// project, which takes ownership and binds it. Returns the (non-owning)
    /// pointer for the caller's continued use.
    auto adoptStandalone(std::unique_ptr<Document> doc) -> Document*;

    /// Destroy a standalone document owned by the shared ephemeral project —
    /// called when its tab closes.
    void closeStandalone(Document* doc);

    /// Every open document, across the shared ephemeral project and the open
    /// persistent project. Backs whole-application operations (save-all,
    /// close-all, quit, path lookup, liveness checks).
    [[nodiscard]] auto documents() const -> std::vector<Document*>;

    /// Close a persistent project: prompt to save each open, modified member,
    /// tear down its editors/tabs, then drop the project (its nodes own the
    /// documents, so they are destroyed with it). Returns false — leaving the
    /// project open — when the user cancels a save prompt. The shared ephemeral
    /// project is permanent and never closed this way.
    auto closeProject(Project& project) -> bool;

    /// Apply a document's saved per-project session state (scroll, caret,
    /// folds, encoding, …). No-op unless `doc` is a persistent-project member.
    /// Called by `DocumentManager` when an editor is opened.
    void applyDocumentSession(Document& doc);

    /// Capture a document's editor state into its project session. No-op unless
    /// `doc` is a persistent-project member. Called when an editor closes.
    void captureDocumentSession(Document& doc);

    /// Re-save the `.fbp` of `doc`'s persistent project (no-op for standalone
    /// documents). Called when a member's type override changes — that override
    /// is project data stored in the project file, not the session.
    void persistProjectFile(Document& doc);

    /// Flush the open project's session (open documents, active tab, expanded
    /// folders, selected node) to its `.fbide/session.ini`. No-op when no
    /// persistent project is open. Run on project close, project switch, and
    /// app exit — while the editors and tree are still alive.
    void saveProjectSession();

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

    /// The active document's project — the shared ephemeral for a standalone
    /// document, or the persistent project for a project file; `nullptr` when
    /// no document is active. Pure query: it does not touch the ephemeral's
    /// build context — call `onActiveDocumentChanged` first to keep that in
    /// sync.
    [[nodiscard]] auto getActiveProject() const -> ProjectBase*;

    /// Single funnel for an active-document change: point the shared
    /// ephemeral's build context (sources / capabilities / compiler-config) at
    /// `doc` when it is a standalone document, else clear it. Must run before
    /// the new active document's capabilities/sources are read (e.g. before
    /// `UIManager::syncBuildCommands`).
    void onActiveDocumentChanged(Document* doc);

private:
    /// Reopen the documents recorded in the just-loaded project's session and
    /// restore the active tab. The tree's expanded/selected state is restored
    /// by `ProjectTreeView` on construction.
    void restoreProjectSession();

    Context& m_ctx;
    /// Open persistent projects (currently at most one at a time).
    std::unordered_map<ProjectBase::Id, std::unique_ptr<ProjectBase>> m_projects;
    /// The open persistent project (owned in `m_projects`; this is just an
    /// O(1) handle) and its `.fbp` path for same-file re-open detection.
    /// Null / empty when no project is open.
    Project* m_project = nullptr;
    std::filesystem::path m_projectPath;
    /// The shared ephemeral project owning all standalone documents — lazily
    /// created (see `ephemeral()`), permanent once created.
    std::unique_ptr<EphemeralProject> m_ephemeral;
    /// Declared last so destruction runs first — worker thread stops
    /// and joins before the projects and documents it might race with
    /// go away.
    std::unique_ptr<IntellisenseService> m_intellisense;
};

} // namespace fbide
