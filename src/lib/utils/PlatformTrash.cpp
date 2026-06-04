//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "utils/PlatformTrash.hpp"

#if defined(_WIN32)

#include <shellapi.h>
#include <windows.h>

auto fbide::moveToTrash(const std::filesystem::path& path) -> bool {
    // SHFileOperation expects a double-null-terminated list of source paths.
    std::wstring buffer = path.wstring();
    buffer.push_back(L'\0');
    buffer.push_back(L'\0');

    SHFILEOPSTRUCTW op {};
    op.wFunc = FO_DELETE;
    op.pFrom = buffer.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationW(&op) == 0 && op.fAnyOperationsAborted == FALSE;
}

#elif !defined(__APPLE__)

// Linux / other Unix: defer to the desktop's trash via `gio trash` (GLib/GIO,
// present on all mainstream desktops). The argv form avoids shell quoting.
auto fbide::moveToTrash(const std::filesystem::path& path) -> bool {
    wxArrayString argv;
    argv.Add("gio");
    argv.Add("trash");
    argv.Add("--");
    argv.Add(wxString::FromUTF8(path.string()));
    return wxExecute(argv, wxEXEC_SYNC) == 0;
}

#endif
// On Apple platforms moveToTrash is defined in PlatformTrash.mm.
