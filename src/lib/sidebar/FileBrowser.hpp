//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ui/controls/Layout.hpp"
#include <wx/fswatcher.h>
#include <set>

class wxGenericDirCtrl;
class wxTreeEvent;

namespace fbide {
class Context;

/**
 * Browse Files tab: a directory tree that opens files on activation and
 * auto-refreshes when a visible (expanded) folder changes on disk.
 *
 * A `Layout<wxPanel>` hosting a `wxGenericDirCtrl` child (so the panel can
 * grow more controls later — a path bar, filter, toolbar, …).
 *
 * **Watch scope:** only the folders the user has expanded, from the root down
 * — a folder is watched when expanded and unwatched when collapsed. The watch
 * is further gated on panel visibility: the owning `SideBarManager` calls
 * `setWatchEnabled` so a hidden Browser holds no filesystem watches.
 *
 * **Owned by:** `SideBarManager` (wx-parented to the sidebar notebook).
 */
class FileBrowser final : public Layout<wxPanel> {
public:
    NO_COPY_AND_MOVE(FileBrowser)

    FileBrowser(wxWindow* parent, Context& ctx);
    /// Stops the timer and closes the filesystem watcher deterministically on
    /// teardown (rather than leaving it to member-destruction order on quit).
    ~FileBrowser() override;

    /// Reveal and select `path` in the tree.
    void locateFile(const wxString& path);

private:
    /// Panel shown/hidden (notebook tab switch) — start/stop watching to match.
    void onShow(wxShowEvent& event);
    /// Tree leaf activated — open the file in a new editor tab.
    void onFileActivated(wxTreeEvent& event);
    /// A folder was expanded — start watching it.
    void onItemExpanded(wxTreeEvent& event);
    /// A folder is about to collapse — stop watching it. (wxGenericDirCtrl eats
    /// the COLLAPSED event without skipping, so we hook COLLAPSING instead.)
    void onItemCollapsing(wxTreeEvent& event);
    /// A change in a watched folder — debounce a refresh.
    void onFsEvent(wxFileSystemWatcherEvent& event);
    /// Debounced refresh.
    void onRefreshTimer(wxTimerEvent& event);

    /// Enable or disable filesystem watching (driven by panel visibility):
    /// enabling watches the currently-expanded folders, disabling drops them.
    void setWatchEnabled(bool enabled);

    /// Start watching `path` (a single folder, non-recursive). No-op when
    /// disabled, already watched, or the folder no longer exists.
    void watchFolder(const wxString& path);
    /// Stop watching `path`.
    void unwatchFolder(const wxString& path);
    /// Re-read each given folder, preserving selection + scroll across the batch.
    void refreshFolders(const std::vector<wxString>& dirs);
    /// Re-read one folder node: collapse/expand to force a disk re-read, then
    /// re-open the folders that were open under it (dropping watches for any that
    /// vanished). Drops this node's watch if it is itself gone or collapsed.
    void refreshFolder(const wxString& dir);
    /// The outermost of the changed folders: drop any nested inside another in the
    /// set (the outer one's refresh re-reads it), keeping siblings independent.
    [[nodiscard]] static auto outermostDirs(const std::set<wxString>& dirs) -> std::vector<wxString>;
    /// True if `child` is strictly nested inside `parent`.
    [[nodiscard]] static auto isUnder(const wxString& child, const wxString& parent) -> bool;

    /// Collect the paths of every currently-expanded directory node.
    [[nodiscard]] auto collectExpandedPaths() const -> std::vector<wxString>;
    /// Collect expanded directory paths in the subtree under `start` (excludes
    /// `start` itself).
    [[nodiscard]] auto collectExpandedFrom(wxTreeItemId start) const -> std::vector<wxString>;
    /// Find the visible tree item for `path`, or an invalid id if not present.
    [[nodiscard]] auto findItemByPath(const wxString& path) const -> wxTreeItemId;

    Context& m_ctx;
    Unowned<wxGenericDirCtrl> m_dirCtrl;               ///< The directory tree (child of this panel).
    std::unique_ptr<wxFileSystemWatcher> m_fsWatcher;  ///< Non-null only while watching (panel visible).
    wxTimer m_refreshTimer;                            ///< Debounces filesystem-change refreshes.
    std::set<wxString> m_watchedDirs;                  ///< Folders currently watched (the expanded ones).
    std::set<wxString> m_pendingDirs;                  ///< Changed folders awaiting a debounced refresh.
    bool m_suppressWatch = false;                      ///< Ignore expand/collapse churn during a rebuild.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
