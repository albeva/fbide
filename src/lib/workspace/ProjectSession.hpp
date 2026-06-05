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
 * Per-project session state, persisted to `<project-root>/.fbide/session.ini`.
 *
 * Records the open documents (with their editor state), the active editor tab,
 * and the project tree's expanded folders + selected node — all keyed by
 * `Node::Id`. Per-document state is delegated to `Document::setSessionAttributes`
 * / `loadSessionAttributes`, the same code the `.fbs` session uses.
 *
 * Created and owned by a persistent `Project` (an ephemeral project has none);
 * loaded when the project opens, saved when it closes (manual close, project
 * switch, or app exit).
 */
class ProjectSession final {
public:
    NO_COPY_AND_MOVE(ProjectSession)
    using Id = Project::Node::Id;

    /// `projectFile` is the `.fbp`; the session lives in its `.fbide/` sibling.
    explicit ProjectSession(const std::filesystem::path& projectFile);
    ~ProjectSession();

    /// Read `.fbide/session.ini` into memory (no-op when it doesn't exist).
    void load();
    /// Write the in-memory state to `.fbide/session.ini`, creating `.fbide/`.
    void save();

    /// Apply `doc`'s stored editor state from its `files/<node id>` group.
    /// No-op when `doc` isn't a persistent-project member.
    void applyTo(Document& doc);
    /// Capture `doc`'s editor state into its `files/<node id>` group. No-op when
    /// `doc` isn't a persistent-project member.
    void capture(Document& doc);

    /// Drop all session state for node `id` — its `files/<id>` group plus any
    /// reference in the open / expanded lists and the active / selected slots.
    /// Called when the node is removed from the project.
    void forget(Id id);

    /// The open documents, in tab order.
    void setOpenDocuments(const std::vector<Id>& ids);
    [[nodiscard]] auto openDocuments() const -> std::vector<Id>;

    /// The focused editor tab (invalid id when none).
    void setActiveDocument(Id id);
    [[nodiscard]] auto activeDocument() const -> Id;

    /// The project-tree selected node — file or folder (invalid id when none).
    void setSelectedNode(Id id);
    [[nodiscard]] auto selectedNode() const -> Id;

    /// The expanded folder nodes.
    void setExpandedFolders(const std::vector<Id>& ids);
    [[nodiscard]] auto expandedFolders() const -> std::vector<Id>;

private:
    std::filesystem::path m_path;        ///< `.fbide/session.ini`.
    std::unique_ptr<wxFileConfig> m_cfg; ///< In-memory session config.
};

} // namespace fbide
