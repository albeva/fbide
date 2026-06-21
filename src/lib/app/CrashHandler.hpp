//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/**
 * Windows-only field crash diagnostics. Installs a process-wide
 * unhandled-exception filter that, on a fault, writes a one-line crash
 * summary (exception code + faulting module + offset within it) to
 * `<crashDir>/fbide_crash.log` and a minidump to
 * `<crashDir>/fbide_crash_<pid>.dmp` — so a silent termination on a
 * user's machine leaves a post-mortem artefact instead of nothing.
 *
 * The filter writes via raw Win32 (no CRT/heap, no wxLog) so it stays
 * usable after heap-corrupting faults. It cannot catch fail-fast
 * (`0xC0000409`) crashes or an external `TerminateProcess` (e.g. AV) —
 * the startup breadcrumbs in `App::OnInit` cover those by showing how
 * far startup got. No-op on non-Windows.
 */
class CrashHandler final {
public:
    /// Install the unhandled-exception filter. `crashDir` is where the
    /// crash log + minidump are written (the app log directory). Call
    /// once, early in startup.
    static void install(const wxString& crashDir);
};

} // namespace fbide
