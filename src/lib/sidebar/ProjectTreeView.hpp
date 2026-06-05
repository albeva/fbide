//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "workspace/Project.hpp"

namespace fbide {
class Context;

/**
 * Sidebar tree view of the open persistent project — a thin **view** over
 * the `Project` view-model.
 *
 * It renders the node tree, keeps a two-way item↔`Project::Node*` map,
 * raises a context menu built from `Project::contextActions`, and drives the
 * add / remove / move operations: it gathers user input (name prompts, file
 * pickers, confirmations), calls the matching `Project` model method, and
 * reflects the result in the tree. All tree rules and filesystem
 * side-effects live in `Project`; this class owns only the wx UI.
 */
class ProjectTreeView final : public wxTreeCtrl {
public:
    NO_COPY_AND_MOVE(ProjectTreeView)

    /// Build the tree for `project`, parented to the sidebar notebook.
    ProjectTreeView(wxWindow* parent, Context& ctx, Project& project);

private:
    /// Image indices into the tree's image list (order = list order).
    enum Icon : int {
        IconRoot = 0,
        IconFolder = 1,
        IconFile = 2,
    };

    /// Rebuild the whole tree from the model (used after multi-item or move
    /// operations; loses expansion state).
    void rebuild();
    /// Recursively add `node` under `parentItem` (invalid parent → tree root).
    auto addNode(const wxTreeItemId& parentItem, Project::Node* node) -> wxTreeItemId;
    /// Insert the freshly added `child` at its sorted position under `parent`,
    /// map it, and select it.
    void insertChildItem(Project::Node* parent, Project::Node* child);
    /// Erase the item↔node map entries for `node` and its descendants.
    void forgetSubtree(Project::Node* node);

    [[nodiscard]] auto iconFor(const Project::Node* node) const -> int;
    [[nodiscard]] auto nodeFor(const wxTreeItemId& item) const -> Project::Node*;
    [[nodiscard]] auto labelFor(Project::Action action) const -> wxString;
    [[nodiscard]] auto mainFrame() const -> wxWindow*;

    // Context menu + actions ----------------------------------------------
    /// Double-click / Enter on a file node opens (or focuses) its editor.
    void onItemActivated(wxTreeEvent& event);
    void onItemMenu(wxTreeEvent& event);
    /// EVT_MENU_RANGE handler for the context-menu items — recovers the
    /// `Action` from the menu id and the node from the current selection.
    void onMenuAction(wxCommandEvent& event);
    void runAction(Project::Action action, Project::Node* node);
    void addFolder(Project::Node* parent);
    void addFile(Project::Node* parent, const wxString& extension);
    void addExisting();
    void removeNode(Project::Node* node);
    /// Ask whether to add the already-existing item named `name`. `msgKey` is
    /// the localisation key for the "... already exists, add it?" message.
    [[nodiscard]] auto confirmAddExisting(const char* msgKey, const wxString& name) const -> bool;
    /// Close any open documents bound in `node`'s subtree. Returns false if
    /// the user cancelled a save (caller then aborts the removal).
    [[nodiscard]] auto closeBoundDocuments(Project::Node* node) -> bool;

    // Drag & drop ----------------------------------------------------------
    void onBeginDrag(wxTreeEvent& event);
    void onEndDrag(wxTreeEvent& event);

    Context& m_ctx;                                                      ///< Application context.
    Project& m_project;                                                  ///< The bound project (view-model).
    std::unordered_map<wxTreeItemId::Type, Project::Node*> m_itemToNode; ///< Tree item → node.
    std::unordered_map<Project::Node*, wxTreeItemId> m_nodeToItem;       ///< Node → tree item.
    wxTreeItemId m_dragItem;                                             ///< Source item of an in-progress drag (invalid when none).

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
