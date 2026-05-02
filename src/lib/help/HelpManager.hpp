//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#ifdef __WXMSW__
#include <wx/msw/helpchm.h>
#endif

namespace fbide {
class Context;

/**
 * Help dispatcher: opens the FreeBASIC manual via CHM on Windows or
 * the web wiki on other platforms.
 *
 * **Owns:** the lazy `wxCHMHelpController` (Windows only).
 * **Owned by:** `Context`.
 * **Threading:** UI thread only.
 */
class HelpManager final {
public:
    NO_COPY_AND_MOVE(HelpManager)

    explicit HelpManager(Context& ctx);

    /// Open help for keyword at cursor. Tries CHM on Windows, falls back to wiki.
    void open();

    /// Open FreeBASIC wiki in the default browser.
    void openWiki(const wxString& query = {});

#ifdef __WXMSW__
    /// Check if a help file is accessible.
    /// On Windows, shows a warning if the CHM file is blocked.
    /// Returns true if the file is accessible (or doesn't exist).
    static auto verifyHelpFileAccessible(wxWindow* parent, const wxString& path) -> bool;

private:
    /// Open CHM help file. Returns false if CHM is unavailable.
    auto openChm(const wxString& query) -> bool;

    std::unique_ptr<wxCHMHelpController> m_help;
#endif

    Context& m_ctx;
};

} // namespace fbide
