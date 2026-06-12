//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "SystemShell.hpp"
#include "utils/PathConversions.hpp"

#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

using namespace fbide;

namespace {
/// Quote a path for a shell command line.
auto quoted(const wxString& path) -> wxString {
    return "\"" + path + "\"";
}
} // namespace

void SystemShell::revealInFileManager(const wxString& path) {
#ifdef __WXMSW__
    // `explorer /select,<path>` opens the parent folder with the item selected.
    wxExecute("explorer /select," + quoted(path), wxEXEC_ASYNC);
#elif defined(__WXMAC__)
    // `open -R` reveals (selects) the item in Finder.
    wxExecute("open -R " + quoted(path), wxEXEC_ASYNC);
#else
    // No portable "select"; open the containing directory.
    const wxString dir = wxFileName(path).GetPath();
    wxExecute("xdg-open " + quoted(dir), wxEXEC_ASYNC);
#endif
}

auto SystemShell::moveToTrash(const wxString& path) -> bool {
#ifdef __WXMSW__
    // SHFileOperationW with FOF_ALLOWUNDO sends to the Recycle Bin. pFrom must
    // be double-null-terminated; the extra push_back plus c_str()'s implicit
    // terminator provide the two trailing NULs.
    std::wstring from = path.ToStdWstring();
    from.push_back(L'\0');
    SHFILEOPSTRUCTW op {};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = static_cast<FILEOP_FLAGS>(FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT);
    return SHFileOperationW(&op) == 0 && op.fAnyOperationsAborted == FALSE;
#elif defined(__WXMAC__)
    // Ask Finder to move the item to the Trash.
    const wxString script = "tell application \"Finder\" to delete POSIX file " + quoted(path);
    return wxExecute("osascript -e " + quoted(script), wxEXEC_SYNC) == 0;
#else
    // gio (GLib/GVfs) ships with most Linux desktops and honours the trash spec.
    return wxExecute("gio trash " + quoted(path), wxEXEC_SYNC) == 0;
#endif
}

void SystemShell::showProperties(const wxString& path) {
#ifdef __WXMSW__
    SHObjectProperties(nullptr, SHOP_FILEPATH, path.wc_str(), nullptr);
#else
    (void) path;
#endif
}

auto SystemShell::propertiesSupported() -> bool {
#ifdef __WXMSW__
    return true;
#else
    return false;
#endif
}

void SystemShell::openTerminal(const wxString& dir, const wxString& terminalCommand) {
    // Mirrors CommandManager::onCmdPrompt — launch the configured terminal with
    // its working directory set to the folder.
    wxExecuteEnv env;
    env.cwd = dir;
    wxExecute(terminalCommand, wxEXEC_ASYNC, nullptr, &env);
}
