//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FileBrowser.hpp"
#include <wx/dirctrl.h>
#include "app/Context.hpp"
#include "document/DocumentManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {
// Coalesce a burst of filesystem events (atomic saves fire several) before
// rebuilding the tree.
constexpr int kDebounceMs = 300;
constexpr int kFsEvents = wxFSW_EVENT_CREATE | wxFSW_EVENT_DELETE | wxFSW_EVENT_RENAME | wxFSW_EVENT_MODIFY;
} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(FileBrowser, Layout<wxPanel>)
    EVT_SHOW                 (FileBrowser::onShow)
    EVT_DIRCTRL_FILEACTIVATED(wxID_ANY, FileBrowser::onFileActivated)
    EVT_FSWATCHER            (wxID_ANY, FileBrowser::onFsEvent)
    EVT_TIMER                (wxID_ANY, FileBrowser::onRefreshTimer)
    EVT_TREE_ITEM_EXPANDED   (wxID_ANY, FileBrowser::onItemExpanded)
    EVT_TREE_ITEM_COLLAPSING (wxID_ANY, FileBrowser::onItemCollapsing)
wxEND_EVENT_TABLE()
// clang-format on

FileBrowser::FileBrowser(wxWindow* parent, Context& ctx)
: Layout<wxPanel>(parent)
, m_ctx(ctx) {
    // Tree fills the panel edge-to-edge; future controls (path bar, filter)
    // can be added around it through the Layout DSL.
    static_cast<SmartBoxSizer*>(currentSizer())->setOptions({ .gap = 0, .margin = false });

    m_dirCtrl = make_unowned<wxGenericDirCtrl>(
        // Empty dir == the platform default root (home / drive list).
        this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxDIRCTRL_3D_INTERNAL, wxEmptyString
    );
    add(m_dirCtrl, { .proportion = 1 });
    SetSizer(currentSizer());

    // Expand/collapse are caught through the event table above: they bubble up
    // from the inner tree control to this panel (see the table comment).
    m_refreshTimer.SetOwner(this);
}

FileBrowser::~FileBrowser() {
    m_refreshTimer.Stop();
    m_fsWatcher.reset();
}

void FileBrowser::setWatchEnabled(const bool enabled) {
    if (enabled == (m_fsWatcher != nullptr)) {
        return; // already in the requested state
    }
    if (enabled) {
        m_fsWatcher = std::make_unique<wxFileSystemWatcher>();
        m_fsWatcher->SetOwner(this);
        for (const auto& path : collectExpandedPaths()) {
            watchFolder(path);
        }
    } else {
        m_refreshTimer.Stop();
        m_fsWatcher.reset(); // drops every OS watch
        m_watchedDirs.clear();
    }
}

void FileBrowser::locateFile(const wxString& path) {
    if (path.IsEmpty()) {
        return;
    }
    m_dirCtrl->ExpandPath(path);
    m_dirCtrl->SelectPath(path);
}

void FileBrowser::onShow(wxShowEvent& event) {
    event.Skip();
    // The notebook shows/hides this page on tab switches; watch only while
    // visible. event.IsShown() is the page's own new state — no parent walk, so
    // this stays safe during frame teardown.
    setWatchEnabled(event.IsShown());
}

void FileBrowser::onFileActivated(wxTreeEvent& event) {
    event.Skip();
    if (const auto path = m_dirCtrl->GetFilePath(); !path.IsEmpty()) {
        m_ctx.getDocumentManager().openFile(path);
    }
}

void FileBrowser::onItemExpanded(wxTreeEvent& event) {
    event.Skip();
    // m_dirCtrl can still be null here: the tree may fire an expand from inside
    // its own construction, before the m_dirCtrl assignment in the constructor.
    if (!m_suppressWatch && m_dirCtrl != nullptr) {
        watchFolder(m_dirCtrl->GetPath(event.GetItem()));
    }
}

void FileBrowser::onItemCollapsing(wxTreeEvent& event) {
    event.Skip();
    if (!m_suppressWatch && m_dirCtrl != nullptr) {
        unwatchFolder(m_dirCtrl->GetPath(event.GetItem()));
    }
}

void FileBrowser::onFsEvent(wxFileSystemWatcherEvent& event) {
    event.Skip();
    // Queue the folder whose listing changed (the directory containing the
    // affected entry). The debounced refresh drops any queued folder nested
    // inside another and re-reads the rest.
    if (const wxString dir = event.GetPath().GetPath(); !dir.IsEmpty()) {
        m_pendingDirs.insert(dir);
    }
    if (event.GetChangeType() == wxFSW_EVENT_RENAME) {
        if (const wxString dir = event.GetNewPath().GetPath(); !dir.IsEmpty()) {
            m_pendingDirs.insert(dir);
        }
    }
    m_refreshTimer.StartOnce(kDebounceMs);
}

void FileBrowser::onRefreshTimer(wxTimerEvent& /*event*/) {
    if (m_pendingDirs.empty()) {
        return;
    }
    std::set<wxString> dirs;
    dirs.swap(m_pendingDirs);
    refreshFolders(outermostDirs(dirs));
}

void FileBrowser::watchFolder(const wxString& path) {
    if (m_fsWatcher == nullptr || path.IsEmpty() || m_watchedDirs.contains(path)) {
        return;
    }
    if (wxFileName::DirExists(path)) {
        m_fsWatcher->Add(wxFileName::DirName(path), kFsEvents);
        m_watchedDirs.insert(path);
    }
}

void FileBrowser::unwatchFolder(const wxString& path) {
    if (m_fsWatcher != nullptr && m_watchedDirs.erase(path) > 0) {
        m_fsWatcher->Remove(wxFileName::DirName(path));
    }
}

void FileBrowser::refreshFolders(const std::vector<wxString>& dirs) {
    auto* tree = m_dirCtrl->GetTreeCtrl();
    if (tree == nullptr) {
        return;
    }
    const auto thaw = FreezeLock(this);

    // Snapshot selection + scroll once around the whole batch.
    const wxString selected = m_dirCtrl->GetPath();
    const int scrollV = tree->GetScrollPos(wxVERTICAL);
    const int scrollH = tree->GetScrollPos(wxHORIZONTAL);

    // The collapse/expand churns expand/collapse events; ignore them — survivors
    // keep their existing watch and dead folders get dropped inline below.
    m_suppressWatch = true;
    for (const auto& dir : dirs) {
        refreshFolder(dir);
    }
    if (!selected.IsEmpty()) {
        m_dirCtrl->SelectPath(selected); // no-op if it vanished
    }

    tree->SetScrollPos(wxVERTICAL, scrollV, false);
    tree->SetScrollPos(wxHORIZONTAL, scrollH, false);
    m_suppressWatch = false;
}

void FileBrowser::refreshFolder(const wxString& dir) {
    auto* tree = m_dirCtrl->GetTreeCtrl();
    const auto item = findItemByPath(dir);
    if (!item.IsOk() || !tree->IsExpanded(item)) {
        unwatchFolder(dir); // gone or collapsed — no longer watched/visible
        return;
    }
    // Remember the open folders under this node, then collapse + expand to make
    // wxGenericDirCtrl re-read the directory from disk, and re-open them.
    const auto reopen = collectExpandedFrom(item);
    tree->Collapse(item);
    tree->Expand(item);
    for (const auto& path : reopen) {
        if (wxFileName::DirExists(path)) {
            m_dirCtrl->ExpandPath(path); // still there — re-open it
        } else {
            unwatchFolder(path); // vanished — stop watching it
        }
    }
}

auto FileBrowser::outermostDirs(const std::set<wxString>& dirs) -> std::vector<wxString> {
    std::vector<wxString> roots;
    for (const auto& dir : dirs) {
        // Drop a folder that is nested inside another changed folder — refreshing
        // the outer one already re-reads it. Siblings are kept independently.
        const bool nested = std::ranges::any_of(dirs, [&dir](const wxString& other) {
            return other != dir && isUnder(dir, other);
        });
        if (!nested) {
            roots.push_back(dir);
        }
    }
    return roots;
}

auto FileBrowser::isUnder(const wxString& child, const wxString& parent) -> bool {
    const wxFileName childName = wxFileName::DirName(child);
    const wxFileName parentName = wxFileName::DirName(parent);
    if (childName.GetVolume() != parentName.GetVolume()) {
        return false;
    }
    const wxArrayString& childDirs = childName.GetDirs();
    const wxArrayString& parentDirs = parentName.GetDirs();
    // Strictly nested: parent has fewer components and is a prefix of the child.
    return parentDirs.GetCount() < childDirs.GetCount() && std::ranges::starts_with(childDirs, parentDirs);
}

auto FileBrowser::collectExpandedPaths() const -> std::vector<wxString> {
    const auto* tree = m_dirCtrl->GetTreeCtrl();
    return tree != nullptr ? collectExpandedFrom(tree->GetRootItem()) : std::vector<wxString> {};
}

auto FileBrowser::collectExpandedFrom(const wxTreeItemId start) const -> std::vector<wxString> {
    std::vector<wxString> paths;
    const auto* tree = m_dirCtrl->GetTreeCtrl();
    if (tree == nullptr || !start.IsOk()) {
        return paths;
    }
    // Iterative walk; descend only into expanded nodes (only their children are
    // populated). `start` itself is excluded.
    std::vector<wxTreeItemId> stack { start };
    while (!stack.empty()) {
        const auto item = stack.back();
        stack.pop_back();
        const bool isStart = item == start;
        if (!isStart && tree->IsExpanded(item)) {
            if (const wxString path = m_dirCtrl->GetPath(item); !path.IsEmpty()) {
                paths.push_back(path);
            }
        }
        if (isStart || tree->IsExpanded(item)) {
            wxTreeItemIdValue cookie = nullptr;
            for (auto child = tree->GetFirstChild(item, cookie); child.IsOk(); child = tree->GetNextChild(item, cookie)) {
                stack.push_back(child);
            }
        }
    }
    return paths;
}

auto FileBrowser::findItemByPath(const wxString& path) const -> wxTreeItemId {
    const auto* tree = m_dirCtrl->GetTreeCtrl();
    if (tree == nullptr) {
        return {};
    }
    const auto root = tree->GetRootItem();
    if (!root.IsOk()) {
        return {};
    }
    // Compare as filenames so a tree path matches an fs-event-derived path despite
    // trailing-separator or (on Windows) case differences between the two sources.
    const wxFileName target = wxFileName::DirName(path);
    std::vector<wxTreeItemId> stack { root };
    while (!stack.empty()) {
        const auto item = stack.back();
        stack.pop_back();
        if (item != root && wxFileName::DirName(m_dirCtrl->GetPath(item)).SameAs(target)) {
            return item;
        }
        // Only descend into expanded nodes — the target must be visible for a
        // localized refresh to apply.
        if (item == root || tree->IsExpanded(item)) {
            wxTreeItemIdValue cookie = nullptr;
            for (auto child = tree->GetFirstChild(item, cookie); child.IsOk(); child = tree->GetNextChild(item, cookie)) {
                stack.push_back(child);
            }
        }
    }
    return {};
}
