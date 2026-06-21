//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CrashHandler.hpp"

using namespace fbide;

#ifdef __WXMSW__
#include <windows.h>
#include <dbghelp.h> // MINIDUMP_* types + enums; MiniDumpWriteDump resolved at runtime

namespace {

/// Crash-artefact directory captured at install() time. A fixed buffer,
/// not a wxString, so the filter touches no heap after a fault.
wchar_t g_crashDir[MAX_PATH] = { 0 };

/// Append one wide line to `<g_crashDir>\fbide_crash.log` via raw Win32.
/// Best-effort: any failure is silently ignored.
void writeCrashLine(const wchar_t* line) {
    wchar_t path[MAX_PATH * 2];
    wsprintfW(path, L"%s\\fbide_crash.log", static_cast<const wchar_t*>(g_crashDir));
    const HANDLE file = CreateFileW(
        path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
    );
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(lstrlenW(line) * static_cast<int>(sizeof(wchar_t))), &written, nullptr);
    CloseHandle(file);
}

/// Write a minidump to `<g_crashDir>\fbide_crash_<pid>.dmp`. dbghelp is
/// loaded on demand so the binary carries no link-time dependency on it.
void writeMiniDump(EXCEPTION_POINTERS* info) {
    wchar_t path[MAX_PATH * 2];
    wsprintfW(path, L"%s\\fbide_crash_%lu.dmp", static_cast<const wchar_t*>(g_crashDir), GetCurrentProcessId());
    const HANDLE file = CreateFileW(
        path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
    );
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    const HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
    if (dbghelp != nullptr) {
        using WriteDumpFn = BOOL(WINAPI*)(
            HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
            PMINIDUMP_EXCEPTION_INFORMATION,
            PMINIDUMP_USER_STREAM_INFORMATION,
            PMINIDUMP_CALLBACK_INFORMATION
        );
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
        const auto writeDump = reinterpret_cast<WriteDumpFn>(GetProcAddress(dbghelp, "MiniDumpWriteDump"));
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        if (writeDump != nullptr) {
            MINIDUMP_EXCEPTION_INFORMATION mei {};
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = info;
            mei.ClientPointers = FALSE;
            writeDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &mei, nullptr, nullptr);
        }
        FreeLibrary(dbghelp);
    }
    CloseHandle(file);
}

LONG WINAPI unhandledFilter(EXCEPTION_POINTERS* info) {
    const auto* record = info->ExceptionRecord;
    const auto* address = record->ExceptionAddress;

    // Resolve the module owning the faulting address + the offset into it,
    // so a stripped release build is still placeable from a .map / .pdb.
    wchar_t moduleName[MAX_PATH] = L"<unknown>";
    uintptr_t base = 0;
    HMODULE module = nullptr;
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address), &module
        ) != 0) {
        GetModuleFileNameW(module, moduleName, MAX_PATH);
        base = reinterpret_cast<uintptr_t>(module);
    }

    wchar_t line[MAX_PATH * 2];
    wsprintfW(
        line,
        L"CRASH: code=0x%08X addr=0x%p module=%s offset=0x%IX\r\n",
        static_cast<unsigned>(record->ExceptionCode),
        address,
        static_cast<const wchar_t*>(moduleName),
        reinterpret_cast<uintptr_t>(address) - base
    );
    writeCrashLine(line);
    writeMiniDump(info);

    return EXCEPTION_EXECUTE_HANDLER; // stop here; let the process terminate
}

} // namespace

void CrashHandler::install(const wxString& crashDir) {
    lstrcpynW(g_crashDir, crashDir.wc_str(), MAX_PATH);
    SetUnhandledExceptionFilter(unhandledFilter);
}

#else // non-Windows: no field crash handler

void CrashHandler::install(const wxString& /*crashDir*/) {}

#endif
