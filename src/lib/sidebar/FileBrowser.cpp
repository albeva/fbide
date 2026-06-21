//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FileBrowser.hpp"
#include "FocusableDirCtrl.hpp"
#ifdef __WXOSX__
#include "MacFileIcons.hpp"
#endif
#include "app/Context.hpp"
#include "compiler/AsyncProcess.hpp"
#include "config/ConfigManager.hpp"
#include "document/DocumentManager.hpp"
#include "document/FileSession.hpp"
#include "ui/UIManager.hpp"
#include "ui/controls/FlatButton.hpp"
#include "ui/utilities/SystemShell.hpp"
#include "utils/PathConversions.hpp"
using namespace fbide;

namespace {
// Coalesce a burst of filesystem events (atomic saves fire several) before
// rebuilding the tree.
constexpr int kDebounceMs = 300;
constexpr int kFsEvents = wxFSW_EVENT_CREATE | wxFSW_EVENT_DELETE | wxFSW_EVENT_RENAME | wxFSW_EVENT_MODIFY;
constexpr int kFocusButtonId = wxID_HIGHEST + 1; // focus-toolbar Focus/Unfocus button
constexpr auto kFocusIcon = wxART_ADD_BOOKMARK;
constexpr auto kUnFocusIcon = wxART_DEL_BOOKMARK;

// True when `path` is a file the OS can execute directly: by extension on
// Windows, by the execute permission bit elsewhere. Directories (including macOS
// .app bundles) are excluded — they go to the default handler.
auto isExecutableFile(const wxString& path) -> bool {
    const auto fsPath = toFsPath(path);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(fsPath, ec)) {
        return false;
    }
#ifdef __WXMSW__
    const auto ext = wxFileName(path).GetExt().Lower();
    return ext == "exe" || ext == "com" || ext == "bat" || ext == "cmd";
#else
    constexpr auto execBits = std::filesystem::perms::owner_exec
                            | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;
    return (std::filesystem::status(fsPath, ec).permissions() & execBits) != std::filesystem::perms::none;
#endif
}

// Run an executable detached, with its working directory set to its own folder
// so it resolves relative paths against where it lives. Fire-and-forget: fbide
// neither captures its output nor tracks its lifetime.
void launchExecutable(const wxString& path) {
    const auto folder = toWxString(toFsPath(path).parent_path());
    if (auto* proc = AsyncProcess::exec("\"" + path + "\"", folder, false, [](ProcessResult) {}); proc != nullptr) {
        proc->detach();
    }
}
} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(FileBrowser, Layout<wxPanel>)
    EVT_SHOW                 (FileBrowser::onShow)
    EVT_DIRCTRL_FILEACTIVATED(wxID_ANY, FileBrowser::onFileActivated)
    EVT_FSWATCHER            (wxID_ANY, FileBrowser::onFsEvent)
    EVT_TIMER                (wxID_ANY, FileBrowser::onRefreshTimer)
    EVT_TREE_ITEM_EXPANDED   (wxID_ANY, FileBrowser::onItemExpanded)
    EVT_TREE_ITEM_COLLAPSING (wxID_ANY, FileBrowser::onItemCollapsing)
    EVT_TREE_ITEM_MENU       (wxID_ANY, FileBrowser::onItemMenu)
    EVT_BUTTON               (kFocusButtonId, FileBrowser::onFocusButton)
    EVT_DIRCTRL_SELECTIONCHANGED(wxID_ANY, FileBrowser::onSelectionChanged)
wxEND_EVENT_TABLE()
// clang-format on

FileBrowser::FileBrowser(wxWindow* parent, Context& ctx)
: Layout<wxPanel>(parent)
, m_ctx(ctx) {
    // Tree fills the panel edge-to-edge; future controls (path bar, filter)
    // can be added around it through the Layout DSL.
    static_cast<SmartBoxSizer*>(currentSizer())->setOptions({ .gap = 0, .margin = false });

    // Focus toolbar — always shown above the tree. Its single flat button
    // focuses the selected folder, or exits a focused view.
    m_focusBar = make_unowned<wxPanel>(this);
    {
        const auto barSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
        m_focusButton = make_unowned<FlatButton>(
            m_focusBar, kFocusButtonId, menuText("focus", "Focus"),
            wxArtProvider::GetBitmapBundle(kFocusIcon, wxART_BUTTON)
        );
        barSizer->Add(m_focusButton, 0, wxALL, 2);
        m_focusBar->SetSizer(barSizer);
    }
    add(m_focusBar, { .proportion = 0 });

    m_dirCtrl = make_unowned<FocusableDirCtrl>(
        // Empty dir == the platform default root (home / drive list).
        this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxDIRCTRL_3D_INTERNAL, wxEmptyString
    );
    add(m_dirCtrl, { .proportion = 1 });
    SetSizer(currentSizer());

    // Expand/collapse are caught through the event table above: they bubble up
    // from the inner tree control to this panel (see the table comment).
    m_refreshTimer.SetOwner(this);
    updateFocusButton(); // initial state: full tree, nothing selected
#ifdef __WXOSX__
    // Swap the generic tree icons for native Finder icons once the tree is
    // realized (deferred so the image list + DPI scale are ready).
    CallAfter([this] { reiconTree(); });
#endif
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
    // A focused view only shows one folder's subtree; ExpandPath/SelectPath
    // can't reach a target outside it. Leave the focused view first when the
    // file lies outside the focused folder, so the full tree can reveal it.
    if (const wxString& focused = m_dirCtrl->focusRoot(); !focused.IsEmpty()) {
        const auto rel = toFsPath(path).lexically_normal().lexically_relative(toFsPath(focused).lexically_normal());
        if (rel.empty() || *rel.begin() == "..") {
            unfocus();
        }
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
    // Same behaviour as the context-menu Open: fbide for supported document
    // types, otherwise hand off to the OS default application.
    if (const auto path = m_dirCtrl->GetFilePath(); !path.IsEmpty()) {
        openNode(path);
    }
}

void FileBrowser::onItemExpanded(wxTreeEvent& event) {
    event.Skip();
    // m_dirCtrl can still be null here: the tree may fire an expand from inside
    // its own construction, before the m_dirCtrl assignment in the constructor.
    if (!m_suppressWatch && m_dirCtrl != nullptr) {
        watchFolder(m_dirCtrl->GetPath(event.GetItem()));
    }
#ifdef __WXOSX__
    if (m_dirCtrl != nullptr) {
        reiconChildren(event.GetItem()); // native icons for the freshly-shown children
    }
#endif
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

auto FileBrowser::captureState() const -> State {
    return State {
        .focus = m_dirCtrl->focusRoot(),
        .selected = m_dirCtrl->GetPath(),
        .expanded = collectExpandedPaths(),
    };
}

void FileBrowser::restoreState(const State& state) {
    if (state.focus.IsEmpty() && state.selected.IsEmpty() && state.expanded.empty()) {
        return; // nothing to restore
    }
    {
        const auto thaw = FreezeLock(this);
        m_suppressWatch = true;
        if (!state.focus.IsEmpty()) {
            m_dirCtrl->setFocusRoot(state.focus); // re-root the tree at the focused folder
        }
        for (const auto& path : state.expanded) {
            m_dirCtrl->ExpandPath(path); // re-open each folder (no-op if it vanished)
        }
        if (!state.selected.IsEmpty()) {
            m_dirCtrl->SelectPath(state.selected);
        }
        m_suppressWatch = false;
    }
    updateFocusButton();
#ifdef __WXOSX__
    CallAfter([this] { reiconTree(); }); // native icons for the restored (e.g. focused) tree
#endif
    // Re-establish the watch set for the restored expansion, if currently watching.
    if (m_fsWatcher != nullptr) {
        setWatchEnabled(false);
        setWatchEnabled(true);
    }
}

void FileBrowser::store(FileSession& session) const {
    const auto state = captureState();
    auto& config = session.getConfig();
    config.SetPath("/browser");
    config.Write("focus", session.relative(state.focus));
    config.Write("selected", session.relative(state.selected));
    size_t index = 0;
    for (const auto& path : state.expanded) {
        config.Write(wxString::Format("expanded%03zu", index++), session.relative(path));
    }
}

void FileBrowser::load(FileSession& session) {
    auto& config = session.getConfig();
    config.SetPath("/browser");
    State state;
    wxString stored;
    if (config.Read("focus", &stored)) {
        state.focus = session.resolve(stored);
    }
    if (config.Read("selected", &stored)) {
        state.selected = session.resolve(stored);
    }
    for (size_t index = 0; config.Read(wxString::Format("expanded%03zu", index), &stored); index++) {
        if (!stored.empty()) {
            state.expanded.push_back(session.resolve(stored));
        }
    }
    restoreState(state);
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

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------

void FileBrowser::onItemMenu(wxTreeEvent& event) {
    auto* tree = m_dirCtrl->GetTreeCtrl();
    const auto item = event.GetItem();
    if (tree == nullptr || !item.IsOk()) {
        return;
    }
    tree->SelectItem(item);
    const wxString path = m_dirCtrl->GetPath(item);
    if (path.IsEmpty()) {
        return; // virtual root (e.g. the drive list) — nothing to act on
    }
    std::error_code ec;
    const auto fsPath = toFsPath(path);
    const bool isDir = std::filesystem::is_directory(fsPath, ec);
    const auto parent = fsPath.parent_path();
    const bool hasParent = !parent.empty() && parent != fsPath; // false for a volume root

    // Local menu ids, based above the reserved stock range so they never collide
    // with wxID_OK and friends. Type entries occupy a contiguous block.
    constexpr int kIdOpen = wxID_HIGHEST + 1;
    constexpr int kIdRename = wxID_HIGHEST + 2;
    constexpr int kIdDelete = wxID_HIGHEST + 3;
    constexpr int kIdCopyWin = wxID_HIGHEST + 4;
    constexpr int kIdCopyUnix = wxID_HIGHEST + 5;
    constexpr int kIdCopyName = wxID_HIGHEST + 6;
    constexpr int kIdReveal = wxID_HIGHEST + 7;
    constexpr int kIdProperties = wxID_HIGHEST + 8;
    constexpr int kIdNewFolder = wxID_HIGHEST + 9;
    constexpr int kIdNewEmpty = wxID_HIGHEST + 10;
    constexpr int kIdTerminal = wxID_HIGHEST + 11;
    constexpr int kIdRefresh = wxID_HIGHEST + 12;
    constexpr int kIdFocus = wxID_HIGHEST + 13;
    constexpr int kIdUnfocus = wxID_HIGHEST + 14;
    constexpr int kIdOpenInFbide = wxID_HIGHEST + 15;
    constexpr int kIdNewTypeBase = wxID_HIGHEST + 100;

    wxMenu menu;
    if (isDir) {
        auto newMenu = std::make_unique<wxMenu>();
        newMenu->Append(kIdNewFolder, menuText("newFolder", "Folder…"));
        newMenu->AppendSeparator();
        for (std::size_t i = 0; i < kEditorFileTypeKeys.size(); ++i) {
            const auto key = kEditorFileTypeKeys.at(i);
            const wxString ext = firstExtension(key);
            if (ext.IsEmpty()) {
                continue;
            }
            const wxString keyStr(key.data(), key.size());
            wxString name = m_ctx.tr("filetypes." + keyStr);
            if (name.IsEmpty()) {
                name = keyStr;
            }
            newMenu->Append(kIdNewTypeBase + static_cast<int>(i), name + " (" + ext + ")");
        }
        newMenu->AppendSeparator();
        newMenu->Append(kIdNewEmpty, menuText("newEmptyFile", "Empty File…"));
        menu.AppendSubMenu(newMenu.release(), menuText("newItem", "New"));
        menu.Append(kIdTerminal, menuText("openTerminal", "Open Terminal Here"));
        menu.Append(kIdRefresh, menuText("refresh", "Refresh"));
        const bool isFocusRoot = !m_dirCtrl->focusRoot().IsEmpty()
            && wxFileName(path).SameAs(wxFileName(m_dirCtrl->focusRoot()));
        menu.Append(isFocusRoot ? kIdUnfocus : kIdFocus,
                    isFocusRoot ? menuText("unfocus", "Unfocus") : menuText("focus", "Focus"));
        menu.AppendSeparator();
    } else {
        menu.Append(kIdOpen, menuText("open", "Open"));
        // Supported types already open in fbide via the default action; for the
        // rest, offer an explicit override next to the OS-default Open.
        if (!isSupportedFile(path)) {
            menu.Append(kIdOpenInFbide, menuText("openInFbide", "Open in FBIde"));
        }
        menu.AppendSeparator();
    }
    if (hasParent) {
        menu.Append(kIdRename, menuText("rename", "Rename…"));
        menu.Append(kIdDelete, menuText("del", "Delete…"));
        menu.AppendSeparator();
    }
    auto copyMenu = std::make_unique<wxMenu>();
    copyMenu->Append(kIdCopyWin, menuText("pathWindows", "Windows Path"));
    copyMenu->Append(kIdCopyUnix, menuText("pathUnix", "Unix Path"));
    copyMenu->AppendSeparator();
    copyMenu->Append(kIdCopyName, menuText("copyName", "Copy Name"));
    menu.AppendSubMenu(copyMenu.release(), menuText("copyPath", "Copy Path"));
    menu.Append(kIdReveal, revealLabel());
    if (SystemShell::propertiesSupported()) {
        menu.Append(kIdProperties, menuText("properties", "Properties"));
    }

    const int sel = GetPopupMenuSelectionFromUser(menu);
    switch (sel) {
    case wxID_NONE:
        return;
    case kIdOpen:
        openNode(path);
        break;
    case kIdOpenInFbide:
        m_ctx.getDocumentManager().openFile(path);
        break;
    case kIdRename:
        renameNode(path);
        break;
    case kIdDelete:
        deleteNode(path, isDir);
        break;
    case kIdCopyWin: {
        wxString win = path;
        win.Replace("/", "\\");
        copyToClipboard(win);
        break;
    }
    case kIdCopyUnix: {
        wxString unix = path;
        unix.Replace("\\", "/");
        copyToClipboard(unix);
        break;
    }
    case kIdCopyName:
        copyToClipboard(wxFileName(path).GetFullName());
        break;
    case kIdReveal:
        SystemShell::revealInFileManager(path);
        break;
    case kIdProperties:
        SystemShell::showProperties(path);
        break;
    case kIdNewFolder:
        newFolderIn(path);
        break;
    case kIdNewEmpty:
        newEmptyFileIn(path);
        break;
    case kIdTerminal:
        openTerminalIn(path);
        break;
    case kIdRefresh:
        refreshFolders({ path });
        break;
    case kIdFocus:
        focusFolder(path);
        break;
    case kIdUnfocus:
        unfocus();
        break;
    default:
        if (sel >= kIdNewTypeBase) {
            const auto idx = static_cast<std::size_t>(sel - kIdNewTypeBase);
            if (idx < kEditorFileTypeKeys.size()) {
                newDocumentIn(path, kEditorFileTypeKeys.at(idx));
            }
        }
        break;
    }
}

void FileBrowser::openNode(const wxString& path) {
    if (isSupportedFile(path)) {
        m_ctx.getDocumentManager().openFile(path);
    } else if (isExecutableFile(path)) {
        launchExecutable(path);
    } else {
        wxLaunchDefaultApplication(path); // hand off to the OS default app
    }
}

auto FileBrowser::isSupportedFile(const wxString& path) const -> bool {
    // Match the bare name against the editor-type globs (Makefile, README, etc.
    // are extensionless), delegating to the single matcher next to the patterns.
    return m_ctx.getConfigManager().isEditorFile(wxFileName(path).GetFullName());
}

void FileBrowser::renameNode(const wxString& path) {
    const auto srcFs = toFsPath(path);
    std::error_code ec;
    const bool isDir = std::filesystem::is_directory(srcFs, ec);
    const wxString oldName = toWxString(srcFs.filename());
    const wxString parentDir = toWxString(srcFs.parent_path());

    wxTextEntryDialog dlg(this, menuText("renamePrompt", "New name:"), menuText("renameTitle", "Rename"), oldName);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }
    const wxString newName = dlg.GetValue().Trim(true).Trim(false);
    if (newName.IsEmpty() || newName == oldName) {
        return;
    }
    if (!validateName(newName, parentDir, path)) {
        return;
    }
    if (!isDir) {
        const wxString oldExt = wxFileName(oldName).GetExt();
        const wxString newExt = wxFileName(newName).GetExt();
        if (!oldExt.IsSameAs(newExt, false)) {
            const wxString msg = wxString::Format(
                menuText("extChangeConfirm", R"(Change the extension from "%s" to "%s"?)"), oldExt, newExt
            );
            if (wxMessageBox(msg, menuText("extChangeTitle", "Change Extension?"), wxYES_NO | wxICON_QUESTION, this) != wxYES) {
                return;
            }
        }
    }

    const auto destFs = srcFs.parent_path() / toFsPath(newName);
    std::filesystem::rename(srcFs, destFs, ec);
    if (ec) {
        wxMessageBox(
            wxString::Format(menuText("renameFailed", R"(Could not rename "%s".)"), oldName),
            menuText("errorTitle", "Error"), wxOK | wxICON_ERROR, this
        );
        return;
    }
    // Keep any open document(s) at or under the old path pointed at the new one.
    m_ctx.getDocumentManager().handleExternalRename(srcFs, destFs);
    if (isDir) {
        unwatchFolder(path); // the old directory path is gone
    }
    afterMutation(parentDir, toWxString(destFs));
}

void FileBrowser::deleteNode(const wxString& path, const bool isDir) {
    const wxString name = wxFileName(path).GetFullName();
    const wxString msg = wxString::Format(
        isDir ? menuText("deleteFolderConfirm", R"(Send folder "%s" and its contents to the Recycle Bin?)")
              : menuText("deleteFileConfirm", R"(Send "%s" to the Recycle Bin?)"),
        name
    );
    if (wxMessageBox(msg, menuText("deleteTitle", "Delete"), wxYES_NO | wxICON_WARNING, this) != wxYES) {
        return;
    }
    const wxString parentDir = wxFileName(path).GetPath();
    if (!SystemShell::moveToTrash(path)) {
        // No trash support / failed — fall back to a permanent delete.
        std::error_code ec;
        std::filesystem::remove_all(toFsPath(path), ec);
        if (ec) {
            wxMessageBox(
                wxString::Format(menuText("deleteFailed", R"(Could not delete "%s".)"), name),
                menuText("errorTitle", "Error"), wxOK | wxICON_ERROR, this
            );
            return;
        }
    }
    m_ctx.getDocumentManager().handleExternalDelete(toFsPath(path)); // close any open docs
    unwatchFolder(path); // drop the watch if it was an expanded directory
    afterMutation(parentDir, wxEmptyString);
}

void FileBrowser::newFolderIn(const wxString& dir) {
    wxTextEntryDialog dlg(this, menuText("newFolderPrompt", "Folder name:"), menuText("newFolderTitle", "New Folder"));
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }
    const wxString name = dlg.GetValue().Trim(true).Trim(false);
    if (name.IsEmpty() || !validateName(name, dir)) {
        return;
    }
    const wxString full = toWxString(toFsPath(dir) / toFsPath(name));
    std::error_code ec;
    if (!std::filesystem::create_directory(toFsPath(full), ec) || ec) {
        wxMessageBox(
            wxString::Format(menuText("createFailed", R"(Could not create "%s".)"), name),
            menuText("errorTitle", "Error"), wxOK | wxICON_ERROR, this
        );
        return;
    }
    afterMutation(dir, full);
}

void FileBrowser::newDocumentIn(const wxString& dir, const std::string_view typeKey) {
    const wxString ext = firstExtension(typeKey);
    wxTextEntryDialog dlg(this, menuText("newFilePrompt", "File name:"), menuText("newFileTitle", "New File"));
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }
    wxString name = dlg.GetValue().Trim(true).Trim(false);
    if (name.IsEmpty()) {
        return;
    }
    if (!ext.IsEmpty() && !name.Lower().EndsWith(ext.Lower())) {
        name += ext; // append the type's extension automatically
    }
    const wxString full = createFileIn(dir, name);
    if (full.IsEmpty()) {
        return;
    }
    afterMutation(dir, full);
    m_ctx.getDocumentManager().openFile(full);
}

void FileBrowser::newEmptyFileIn(const wxString& dir) {
    wxTextEntryDialog dlg(this, menuText("newFilePrompt", "File name:"), menuText("newFileTitle", "New File"));
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }
    const wxString name = dlg.GetValue().Trim(true).Trim(false);
    if (name.IsEmpty()) {
        return;
    }
    const wxString full = createFileIn(dir, name);
    if (!full.IsEmpty()) {
        afterMutation(dir, full); // created but, per spec, not opened
    }
}

void FileBrowser::openTerminalIn(const wxString& dir) {
    SystemShell::openTerminal(dir, ConfigManager::getTerminal());
}

void FileBrowser::focusFolder(const wxString& dir) {
    if (dir.IsEmpty()) {
        return;
    }
    {
        const auto thaw = FreezeLock(this);
        m_suppressWatch = true;
        m_dirCtrl->setFocusRoot(dir); // re-root the tree at this folder
        m_suppressWatch = false;
    }
    updateFocusButton();
#ifdef __WXOSX__
    CallAfter([this] { reiconTree(); }); // re-icon the rebuilt tree
#endif
    // The tree was rebuilt with a fresh expansion — re-establish the watch set.
    if (m_fsWatcher != nullptr) {
        setWatchEnabled(false);
        setWatchEnabled(true);
    }
}

void FileBrowser::unfocus() {
    const wxString focused = m_dirCtrl->focusRoot();
    if (focused.IsEmpty()) {
        return;
    }
    // Snapshot the focused view's open folders + selection before switching back.
    const auto reopen = collectExpandedPaths();
    const wxString selected = m_dirCtrl->GetPath();

    {
        const auto thaw = FreezeLock(this);
        m_suppressWatch = true;
        m_dirCtrl->setFocusRoot(wxEmptyString); // restore the full tree (collapsed)
        m_dirCtrl->ExpandPath(focused);         // re-open the path down to the focused folder
        for (const auto& path : reopen) {
            m_dirCtrl->ExpandPath(path);        // re-open the subfolders that were open
        }
        if (!selected.IsEmpty()) {
            m_dirCtrl->SelectPath(selected); // restore selection if it still exists
        }
        m_suppressWatch = false;
    }
    // Scroll the previously focused folder back into view, now its parents exist.
    if (auto* tree = m_dirCtrl->GetTreeCtrl(); tree != nullptr) {
        if (const auto item = findItemByPath(focused); item.IsOk()) {
            tree->EnsureVisible(item);
        }
    }
    updateFocusButton();
#ifdef __WXOSX__
    CallAfter([this] { reiconTree(); }); // re-icon the rebuilt tree
#endif
    if (m_fsWatcher != nullptr) {
        setWatchEnabled(false);
        setWatchEnabled(true);
    }
}

void FileBrowser::onFocusButton(wxCommandEvent& /*event*/) {
    if (!m_dirCtrl->focusRoot().IsEmpty()) {
        unfocus();
        return;
    }
    if (const wxString sel = m_dirCtrl->GetPath(); !sel.IsEmpty() && wxFileName::DirExists(sel)) {
        focusFolder(sel);
    }
}

void FileBrowser::onSelectionChanged(wxTreeEvent& event) {
    event.Skip();
    // m_dirCtrl can still be null here: the tree fires a selection-changed from
    // inside its own construction (ExpandRoot), before the m_dirCtrl assignment
    // in the constructor. updateFocusButton() dereferences m_dirCtrl.
    if (m_dirCtrl != nullptr) {
        updateFocusButton();
    }
}

void FileBrowser::updateFocusButton() {
    const bool focused = !m_dirCtrl->focusRoot().IsEmpty();
    // Swap label + icon only when the focus state actually changes (selection
    // changes fire this often, and re-setting the label forces a relayout).
    const wxString want = focused ? menuText("unfocus", "Unfocus") : menuText("focus", "Focus");
    if (m_focusButton->GetLabel() != want) {
        m_focusButton->setLabelText(want);
        m_focusButton->setIcon(wxArtProvider::GetBitmapBundle(focused ? kUnFocusIcon : kFocusIcon, wxART_BUTTON));
        m_focusBar->Layout();
    }
    // Focused → always actionable (Unfocus). Unfocused → only when a folder is
    // selected; otherwise disabled (greyed) rather than hidden.
    const wxString selected = m_dirCtrl->GetPath();
    m_focusButton->Enable(focused || (!selected.IsEmpty() && wxFileName::DirExists(selected)));
}

auto FileBrowser::createFileIn(const wxString& dir, const wxString& name) -> wxString {
    if (!validateName(name, dir)) {
        return {};
    }
    wxString full = toWxString(toFsPath(dir) / toFsPath(name));
    wxFile file;
    if (!file.Create(full, false)) {
        wxMessageBox(
            wxString::Format(menuText("createFailed", R"(Could not create "%s".)"), name),
            menuText("errorTitle", "Error"), wxOK | wxICON_ERROR, this
        );
        return {};
    }
    file.Close();
    return full;
}

auto FileBrowser::validateName(const wxString& name, const wxString& dir, const wxString& ignorePath) -> bool {
    const auto fail = [&](const char* key, const wxString& fallback) -> bool {
        wxMessageBox(
            wxString::Format(menuText(key, fallback), name),
            menuText("errorTitle", "Error"), wxOK | wxICON_ERROR, this
        );
        return false;
    };
    if (name.IsEmpty() || name == "." || name == ".." || name.find_first_of(R"(\/:*?"<>|)") != wxString::npos) {
        return fail("invalidName", "Invalid name: %s");
    }
    std::error_code ec;
    const auto candidate = toFsPath(dir) / toFsPath(name);
    if (std::filesystem::exists(candidate, ec)) {
        // A rename must not collide with itself: on a case-insensitive
        // filesystem the candidate resolves to the same file being renamed,
        // which is not a real conflict.
        const bool isSelf = !ignorePath.IsEmpty()
            && std::filesystem::equivalent(candidate, toFsPath(ignorePath), ec);
        if (!isSelf) {
            return fail("nameExists", R"(An item named "%s" already exists.)");
        }
    }
    return true;
}

void FileBrowser::afterMutation(const wxString& parentDir, const wxString& revealPath) {
    m_dirCtrl->ExpandPath(parentDir); // ensure the folder is open (and watched)
    refreshFolders({ parentDir });    // re-read so the new/renamed entry appears
    if (!revealPath.IsEmpty()) {
        m_dirCtrl->ExpandPath(revealPath);
        m_dirCtrl->SelectPath(revealPath);
    }
}

auto FileBrowser::firstExtension(const std::string_view typeKey) const -> wxString {
    const wxString key(typeKey.data(), typeKey.size());
    wxString glob = m_ctx.getConfigManager().fileGlob(key).BeforeFirst(';');
    if (glob.StartsWith("*")) {
        glob.Remove(0, 1); // "*.bas" -> ".bas"
    }
    return glob;
}

auto FileBrowser::revealLabel() const -> wxString {
#ifdef __WXMSW__
    return menuText("showInExplorer", "Show in Explorer");
#elif defined(__WXMAC__)
    return menuText("showInFinder", "Reveal in Finder");
#else
    return menuText("showInFileManager", "Show in File Manager");
#endif
}

auto FileBrowser::menuText(const char* key, const wxString& fallback) const -> wxString {
    return m_ctx.getConfigManager().locale().get_or(wxString("fileBrowserContext.") + key, fallback);
}

void FileBrowser::copyToClipboard(const wxString& text) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(make_unowned<wxTextDataObject>(text));
        wxTheClipboard->Close();
    }
}

#ifdef __WXOSX__
void FileBrowser::reiconTree() {
    if (m_dirCtrl == nullptr) {
        return;
    }
    const auto* tree = m_dirCtrl->GetTreeCtrl();
    if (tree == nullptr || tree->GetImageList() == nullptr) {
        return;
    }
    const std::function<void(const wxTreeItemId&)> walk = [&](const wxTreeItemId& node) {
        wxTreeItemIdValue cookie;
        for (auto child = tree->GetFirstChild(node, cookie); child.IsOk(); child = tree->GetNextChild(node, cookie)) {
            reiconItem(child);
            if (tree->IsExpanded(child)) {
                walk(child);
            }
        }
    };
    walk(tree->GetRootItem());
}

void FileBrowser::reiconChildren(const wxTreeItemId parent) {
    const auto* tree = m_dirCtrl->GetTreeCtrl();
    if (tree == nullptr || tree->GetImageList() == nullptr) {
        return;
    }
    wxTreeItemIdValue cookie;
    for (auto child = tree->GetFirstChild(parent, cookie); child.IsOk(); child = tree->GetNextChild(parent, cookie)) {
        reiconItem(child);
    }
}

void FileBrowser::reiconItem(const wxTreeItemId item) {
    const wxString path = m_dirCtrl->GetPath(item);
    if (path.IsEmpty()) {
        return;
    }
    const bool isDir = wxFileName::DirExists(path);
    const wxString ext = isDir ? wxString {} : wxFileName(path).GetExt().Lower();
    const int index = nativeIconIndex(ext, isDir);
    if (index < 0) {
        return;
    }
    auto* tree = m_dirCtrl->GetTreeCtrl();
    // Set every state: wxGenericDirCtrl gives a directory a distinct selected
    // (open-folder) image, so a selected node — e.g. the focus root — would keep
    // its stock icon if only Normal were set.
    tree->SetItemImage(item, index, wxTreeItemIcon_Normal);
    tree->SetItemImage(item, index, wxTreeItemIcon_Selected);
    if (isDir) {
        tree->SetItemImage(item, index, wxTreeItemIcon_Expanded);
        tree->SetItemImage(item, index, wxTreeItemIcon_SelectedExpanded);
    }
}

auto FileBrowser::nativeIconIndex(const wxString& ext, const bool isDir) -> int {
    if (isDir && m_folderIconIndex >= 0) {
        return m_folderIconIndex;
    }
    if (!isDir) {
        if (const auto it = m_fileIconIndex.find(ext); it != m_fileIconIndex.end()) {
            return it->second;
        }
    }
    auto* images = m_dirCtrl->GetTreeCtrl()->GetImageList();
    int width = 16;
    int height = 16;
    images->GetSize(0, width, height);
    const wxBitmap bmp = isDir ? nativeFolderIcon(width) : nativeFileIcon(ext, width);
    if (!bmp.IsOk()) {
        return -1;
    }
    const int index = images->Add(bmp);
    if (isDir) {
        m_folderIconIndex = index;
    } else {
        m_fileIconIndex.emplace(ext, index);
    }
    return index;
}
#endif // __WXOSX__
