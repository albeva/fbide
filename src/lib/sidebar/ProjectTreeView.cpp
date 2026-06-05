//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ProjectTreeView.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "document/DocumentManager.hpp"
#include "document/DocumentPath.hpp"
#include "ui/ArtiProvider.hpp"
#include "ui/UIManager.hpp"

using namespace fbide;

namespace {
/// Id range for the per-action context-menu items (one id per `Action`).
/// `Remove` is the last `Project::Action`, so it bounds the EVT_MENU_RANGE.
constexpr int kMenuActionBase = wxID_HIGHEST + 1;
constexpr int kMenuActionLast = kMenuActionBase + static_cast<int>(Project::Action::Remove);
} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(ProjectTreeView, wxTreeCtrl)
    EVT_TREE_ITEM_ACTIVATED(wxID_ANY, ProjectTreeView::onItemActivated)
    EVT_TREE_ITEM_MENU(wxID_ANY, ProjectTreeView::onItemMenu)
    EVT_TREE_BEGIN_DRAG(wxID_ANY, ProjectTreeView::onBeginDrag)
    EVT_TREE_END_DRAG(wxID_ANY, ProjectTreeView::onEndDrag)
    EVT_MENU_RANGE(kMenuActionBase, kMenuActionLast, ProjectTreeView::onMenuAction)
wxEND_EVENT_TABLE()
// clang-format on

ProjectTreeView::ProjectTreeView(wxWindow* parent, Context& ctx, Project& project)
: wxTreeCtrl(
      parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_FULL_ROW_HIGHLIGHT
  )
, m_ctx(ctx)
, m_project(project) {
    // Image list — order must match the `Icon` enum (Root, Folder, File).
    const auto& art = m_ctx.getUIManager().getArtProvider();
    const auto sample = art.getBitmap(ProjectIcon::Folder);
    const int width = sample.IsOk() ? sample.GetWidth() : 16;
    const int height = sample.IsOk() ? sample.GetHeight() : 16;
    auto* images = make_unowned<wxImageList>(width, height, true, 3).get();
    images->Add(art.getBitmap(ProjectIcon::Root));
    images->Add(art.getBitmap(ProjectIcon::Folder));
    images->Add(art.getBitmap(ProjectIcon::File));
    AssignImageList(images); // tree takes ownership

    rebuild();
}

// --- population ------------------------------------------------------------

void ProjectTreeView::rebuild() {
    DeleteAllItems();
    m_itemToNode.clear();
    m_nodeToItem.clear();
    if (auto* root = m_project.getRoot(); root != nullptr) {
        Expand(addNode(wxTreeItemId {}, root));
    }
}

auto ProjectTreeView::addNode(const wxTreeItemId& parentItem, Project::Node* node) -> wxTreeItemId {
    const int icon = iconFor(node);
    // The root shows the project's name; other nodes show their file/folder name.
    const wxString label = (node == m_project.getRoot())
                             ? m_project.getName()
                             : wxString::FromUTF8(node->name());
    const wxTreeItemId item = parentItem.IsOk()
                                ? AppendItem(parentItem, label, icon, icon)
                                : AddRoot(label, icon, icon);

    m_itemToNode[item.GetID()] = node;
    m_nodeToItem[node] = item;

    if (const auto* folder = node->getFolder()) {
        for (auto* child : folder->children) {
            addNode(item, child);
        }
    }
    return item;
}

void ProjectTreeView::insertChildItem(Project::Node* parent, Project::Node* child) {
    const auto parentIt = m_nodeToItem.find(parent);
    if (parentIt == m_nodeToItem.end()) {
        return;
    }
    const auto& children = parent->getFolder()->children;
    const auto pos = std::ranges::find(children, child);
    const auto index = static_cast<size_t>(pos - children.begin());

    const int icon = iconFor(child);
    const wxString label = wxString::FromUTF8(child->name());
    const wxTreeItemId item = InsertItem(parentIt->second, index, label, icon, icon);
    m_itemToNode[item.GetID()] = child;
    m_nodeToItem[child] = item;
    if (const auto* folder = child->getFolder()) {
        for (auto* grandchild : folder->children) {
            addNode(item, grandchild);
        }
    }
    Expand(parentIt->second);
    SelectItem(item);
}

void ProjectTreeView::forgetSubtree(Project::Node* node) {
    if (const auto it = m_nodeToItem.find(node); it != m_nodeToItem.end()) {
        m_itemToNode.erase(it->second.GetID());
        m_nodeToItem.erase(it);
    }
    if (const auto* folder = node->getFolder()) {
        for (auto* child : folder->children) {
            forgetSubtree(child);
        }
    }
}

auto ProjectTreeView::iconFor(const Project::Node* node) const -> int {
    if (node == m_project.getRoot()) {
        return IconRoot;
    }
    return node->isFolder() ? IconFolder : IconFile;
}

auto ProjectTreeView::nodeFor(const wxTreeItemId& item) const -> Project::Node* {
    if (!item.IsOk()) {
        return nullptr;
    }
    const auto it = m_itemToNode.find(item.GetID());
    return it != m_itemToNode.end() ? it->second : nullptr;
}

auto ProjectTreeView::mainFrame() const -> wxWindow* {
    return m_ctx.getUIManager().getMainFrame();
}

// --- context menu ----------------------------------------------------------

auto ProjectTreeView::labelFor(const Project::Action action) const -> wxString {
    switch (action) {
    case Project::Action::AddFolder:
        return m_ctx.tr("project.menu.addFolder");
    case Project::Action::AddSourceFile:
        return m_ctx.tr("project.menu.addSourceFile");
    case Project::Action::AddHeaderFile:
        return m_ctx.tr("project.menu.addHeaderFile");
    case Project::Action::AddExisting:
        return m_ctx.tr("project.menu.addExisting");
    case Project::Action::Remove:
        return m_ctx.tr("project.menu.remove");
    }
    return {};
}

void ProjectTreeView::onItemActivated(wxTreeEvent& event) {
    auto* node = nodeFor(event.GetItem());
    if (node == nullptr || node->isFolder()) {
        event.Skip(); // let folders expand / collapse
        return;
    }
    if (auto* doc = node->document()) {
        m_ctx.getDocumentManager().openEditorFor(*doc);
    }
}

void ProjectTreeView::onItemMenu(wxTreeEvent& event) {
    auto* node = nodeFor(event.GetItem());
    if (node == nullptr) {
        return;
    }
    SelectItem(event.GetItem());

    wxMenu menu;
    for (const auto action : m_project.contextActions(node)) {
        menu.Append(kMenuActionBase + static_cast<int>(action), labelFor(action));
    }
    PopupMenu(&menu);
}

void ProjectTreeView::onMenuAction(wxCommandEvent& event) {
    auto* node = nodeFor(GetSelection());
    if (node == nullptr) {
        return;
    }
    runAction(static_cast<Project::Action>(event.GetId() - kMenuActionBase), node);
}

void ProjectTreeView::runAction(const Project::Action action, Project::Node* node) {
    switch (action) {
    case Project::Action::AddFolder:
        addFolder(node);
        break;
    case Project::Action::AddSourceFile:
        addFile(node, ".bas");
        break;
    case Project::Action::AddHeaderFile:
        addFile(node, ".bi");
        break;
    case Project::Action::AddExisting:
        addExisting();
        break;
    case Project::Action::Remove:
        removeNode(node);
        break;
    }
}

auto ProjectTreeView::confirmAddExisting(const char* msgKey, const wxString& name) const -> bool {
    wxMessageDialog dlg(
        mainFrame(),
        wxString::Format(m_ctx.tr(msgKey), name),
        m_ctx.tr("project.exists.title"),
        wxYES_NO | wxICON_QUESTION
    );
    dlg.SetYesNoLabels(m_ctx.tr("project.exists.add"), m_ctx.tr("project.exists.cancel"));
    return dlg.ShowModal() == wxID_YES;
}

void ProjectTreeView::addFolder(Project::Node* parent) {
    const wxString name = wxGetTextFromUser(
        m_ctx.tr("project.prompt.folderName"),
        m_ctx.tr("project.prompt.folderTitle"),
        wxEmptyString,
        mainFrame()
    );
    if (name.empty()) {
        return;
    }

    auto result = m_project.addFolder(parent, name.utf8_string());
    if (!result) {
        if (result.error() == Project::Error::Clash) {
            if (!confirmAddExisting("project.exists.folder", name)) {
                return;
            }
            result = m_project.addExisting(parent->path / name.utf8_string());
        }
        if (!result) {
            wxMessageBox(m_ctx.tr("project.error.addFailed"), m_ctx.tr("project.error.title"), wxICON_ERROR | wxOK, mainFrame());
            return;
        }
    }
    insertChildItem((*result)->parent, *result);
}

void ProjectTreeView::addFile(Project::Node* parent, const wxString& extension) {
    wxString name = wxGetTextFromUser(
        m_ctx.tr("project.prompt.fileName"),
        m_ctx.tr("project.prompt.fileTitle"),
        wxEmptyString,
        mainFrame()
    );
    if (name.empty()) {
        return;
    }
    // Default the extension when the user didn't type one.
    if (!name.Contains(".")) {
        name += extension;
    }

    auto result = m_project.addFile(parent, name.utf8_string());
    if (!result) {
        if (result.error() == Project::Error::Clash) {
            if (!confirmAddExisting("project.exists.file", name)) {
                return;
            }
            result = m_project.addExisting(parent->path / name.utf8_string());
        }
        if (!result) {
            wxMessageBox(m_ctx.tr("project.error.addFailed"), m_ctx.tr("project.error.title"), wxICON_ERROR | wxOK, mainFrame());
            return;
        }
    }
    insertChildItem((*result)->parent, *result);
}

void ProjectTreeView::addExisting() {
    wxFileDialog dlg(
        mainFrame(),
        m_ctx.tr("project.addExisting.title"),
        toWxString(m_project.getRoot()->path),
        "",
        m_ctx.getConfigManager().filePatterns({ "freebasic", "all" }),
        wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST
    );
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    wxArrayString paths;
    dlg.GetPaths(paths);
    bool skipped = false;
    for (const auto& path : paths) {
        if (const auto result = m_project.addExisting(toFsPath(path)); !result) {
            skipped = true;
        }
    }
    // Intermediary folders may have been created at arbitrary depths — a full
    // rebuild is the simplest way to reflect a multi-item add correctly.
    rebuild();

    if (skipped) {
        wxMessageBox(m_ctx.tr("project.error.outOfTree"), m_ctx.tr("project.error.title"), wxICON_WARNING | wxOK, mainFrame());
    }
}

void ProjectTreeView::removeNode(Project::Node* node) {
    wxMessageDialog dlg(
        mainFrame(),
        wxString::Format(m_ctx.tr("project.remove.message"), wxString::FromUTF8(node->name())),
        m_ctx.tr("project.remove.title"),
        wxYES_NO | wxCANCEL | wxICON_WARNING
    );
    dlg.SetYesNoCancelLabels(
        m_ctx.tr("project.remove.fromProject"),
        m_ctx.tr("project.remove.andDelete"),
        m_ctx.tr("project.remove.cancel")
    );
    const int choice = dlg.ShowModal();
    if (choice == wxID_CANCEL) {
        return;
    }
    // Close any open tabs in the subtree first so the model invariant holds.
    if (!closeBoundDocuments(node)) {
        return;
    }

    const auto mode = (choice == wxID_NO) ? Project::RemoveMode::AndTrash : Project::RemoveMode::FromProjectOnly;
    const auto itemIt = m_nodeToItem.find(node);
    const wxTreeItemId item = itemIt != m_nodeToItem.end() ? itemIt->second : wxTreeItemId {};

    forgetSubtree(node); // erase map entries while the model nodes are alive
    const auto result = m_project.removeNode(node, mode);
    if (item.IsOk()) {
        Delete(item);
    }
    if (!result) {
        wxMessageBox(m_ctx.tr("project.error.trashFailed"), m_ctx.tr("project.error.title"), wxICON_ERROR | wxOK, mainFrame());
    }
}

auto ProjectTreeView::closeBoundDocuments(Project::Node* node) -> bool {
    if (const auto* file = node->getFile()) {
        if (file->doc != nullptr) {
            return m_ctx.getDocumentManager().closeFile(*file->doc);
        }
        return true;
    }
    for (auto* child : node->getFolder()->children) {
        if (!closeBoundDocuments(child)) {
            return false;
        }
    }
    return true;
}

// --- drag & drop -----------------------------------------------------------

void ProjectTreeView::onBeginDrag(wxTreeEvent& event) {
    auto* node = nodeFor(event.GetItem());
    if (node == nullptr || node == m_project.getRoot()) {
        return; // the root cannot be moved
    }
    m_dragItem = event.GetItem();
    event.Allow();
}

void ProjectTreeView::onEndDrag(wxTreeEvent& event) {
    const wxTreeItemId dragItem = m_dragItem;
    m_dragItem = wxTreeItemId {};
    if (!dragItem.IsOk()) {
        return;
    }

    auto* source = nodeFor(dragItem);
    auto* target = nodeFor(event.GetItem());
    if (source == nullptr || target == nullptr) {
        return;
    }
    // Drop onto a file means "into its containing folder".
    Project::Node* newParent = target->isFolder() ? target : target->parent;
    if (newParent == nullptr || newParent == source->parent) {
        return; // nowhere to go / already there
    }
    // Refuse to drop a folder into itself or one of its descendants.
    for (const auto* cursor = newParent; cursor != nullptr; cursor = cursor->parent) {
        if (cursor == source) {
            return;
        }
    }

    if (const auto result = m_project.moveNode(source, newParent); !result) {
        if (result.error() == Project::Error::Clash) {
            wxMessageBox(
                wxString::Format(m_ctx.tr("project.error.moveClash"), wxString::FromUTF8(source->name())),
                m_ctx.tr("project.error.title"),
                wxICON_WARNING | wxOK,
                mainFrame()
            );
        }
        return;
    }
    rebuild();
}
