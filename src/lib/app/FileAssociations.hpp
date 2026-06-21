//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Windows-only: register per-user (HKCU) associations for FBIde's document
/// types, using icons embedded in the executable. Sets the per-user class
/// default ProgID for .bas/.bi/.fbs so they show FBIde's icons and open with
/// FBIde, and lists FBIde in the Open-with menu. Does not override the user's
/// explicit default-app choice (the OS-protected UserChoice still wins).
class FileAssociations final {
public:
    /// Ensure the registry entries exist and point at this executable. Cheap
    /// and idempotent; only refreshes the shell when something actually changed.
    /// No-op on non-Windows platforms.
    static void ensureRegistered();
};

} // namespace fbide
