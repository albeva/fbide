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

class wxTreeEvent;

namespace fbide {
class Context;
class FocusableDirCtrl;
class FlatButton;

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
    /// Tree leaf activated (double-click / Enter) — open it like the
    /// context-menu Open: fbide for supported types, else the OS default app.
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
    /// Focus toolbar button — focus the selected folder, or unfocus.
    void onFocusButton(wxCommandEvent& event);
    /// Tree selection changed — refresh the focus button's state.
    void onSelectionChanged(wxTreeEvent& event);

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

    // --- Context menu (right-click on a node) --------------------------------

    /// Build and show the per-node context menu, then run the chosen action.
    void onItemMenu(wxTreeEvent& event);

    /// Open a file: in fbide when it is a supported document type, otherwise via
    /// the OS default application.
    void openNode(const wxString& path);
    /// Prompt for a new name and rename the file/dir (validates the name and
    /// confirms an extension change for files).
    void renameNode(const wxString& path);
    /// Confirm, then send the file/dir to the recycle bin (permanent fallback).
    void deleteNode(const wxString& path, bool isDir);
    /// Create a sub-folder in `dir` (prompts for a name).
    void newFolderIn(const wxString& dir);
    /// Create a new document of `typeKey` in `dir` (appends the type's
    /// extension) and open it.
    void newDocumentIn(const wxString& dir, std::string_view typeKey);
    /// Create an arbitrarily-named empty file in `dir` (does not open it).
    void newEmptyFileIn(const wxString& dir);
    /// Open the configured terminal with its working directory at `dir`.
    static void openTerminalIn(const wxString& dir);
    /// Show `dir` as the tree root (focused view).
    void focusFolder(const wxString& dir);
    /// Leave the focused view — restore the full tree, re-expanding the
    /// previously focused folder and the subfolders that were open under it,
    /// then scrolling the focused folder back into view.
    void unfocus();
    /// Sync the toolbar button's label + visibility to the current state:
    /// "Unfocus" while focused; "Focus" (only when a folder is selected) while
    /// showing the full tree.
    void updateFocusButton();

    /// Create `name` in `dir` as an empty file; returns its full path, or ""
    /// on an invalid/conflicting name or I/O failure (an error is shown).
    [[nodiscard]] auto createFileIn(const wxString& dir, const wxString& name) -> wxString;
    /// Validate a new file/dir name and check it does not collide with a
    /// sibling in `dir`. `ignorePath`, when set, is the item being renamed and
    /// is excluded from the collision check (so a no-op or case-only rename is
    /// allowed). Shows an error and returns false when rejected.
    [[nodiscard]] auto validateName(const wxString& name, const wxString& dir, const wxString& ignorePath = {}) -> bool;
    /// Re-read `parentDir` after a mutation and select `revealPath` if given.
    void afterMutation(const wxString& parentDir, const wxString& revealPath);
    /// First extension for an editor file-type key ("freebasic" → ".bas").
    [[nodiscard]] auto firstExtension(std::string_view typeKey) const -> wxString;
    /// Platform label for the "reveal in file manager" entry.
    [[nodiscard]] auto revealLabel() const -> wxString;
    /// Localized `fileBrowserContext.<key>` string with an English fallback.
    [[nodiscard]] auto menuText(const char* key, const wxString& fallback) const -> wxString;
    /// Put `text` on the clipboard.
    static void copyToClipboard(const wxString& text);

    Context& m_ctx;
    Unowned<FocusableDirCtrl> m_dirCtrl;               ///< The directory tree (child of this panel).
    Unowned<wxPanel> m_focusBar;                       ///< Toolbar above the tree (always shown).
    Unowned<FlatButton> m_focusButton;                 ///< Focus / Unfocus button inside the toolbar.
    std::unique_ptr<wxFileSystemWatcher> m_fsWatcher;  ///< Non-null only while watching (panel visible).
    wxTimer m_refreshTimer;                            ///< Debounces filesystem-change refreshes.
    std::set<wxString> m_watchedDirs;                  ///< Folders currently watched (the expanded ones).
    std::set<wxString> m_pendingDirs;                  ///< Changed folders awaiting a debounced refresh.
    bool m_suppressWatch = false;                      ///< Ignore expand/collapse churn during a rebuild.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
