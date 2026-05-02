//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;

/**
 * Loads and saves FBIde `.fbs` session files (which files are open,
 * caret position, encoding/EOL choices).
 *
 * Session v3 is INI-based:
 *
 * @code
 * [session]
 * version=3
 * selectedTab=0
 *
 * [file_000]
 * path=C:/path/to/foo.bas
 * scroll=10
 * cursor=250
 * encoding=UTF-8
 * eolMode=LF
 * @endcode
 *
 * Legacy text formats (v0.1 unversioned, v0.2 XML-ish header) still
 * load for backwards compatibility. Every save writes v3.
 *
 * **Owned by:** `Context`.
 *
 * See @ref documents.
 */
class FileSession final {
public:
    NO_COPY_AND_MOVE(FileSession)

    /// Current session format version.
    static constexpr int Version = 3;

    /// Construct without loading anything.
    explicit FileSession(Context& ctx);

    /// Load a session file, dispatching on detected format.
    void load(const wxString& path);

    /// Save currently open documents as a v3 session.
    /// Returns `false` when the user cancels an in-flight "save changes?"
    /// prompt for a modified named file — the session file is not
    /// written in that case, so callers can use the result to abort
    /// any quit/restart flow they were about to trigger.
    [[nodiscard]] auto save(const wxString& path) -> bool;

    /// File dialog → load selected session.
    void showLoadDialog();

    /// File dialog → save current session.
    void showSaveDialog();

private:
    /// Load the v3 INI format.
    void loadV3(const wxString& path);
    /// Load the v0.1/v0.2 legacy text format.
    void loadLegacy(const wxString& path);

    Context& m_ctx; ///< Application context.
};

} // namespace fbide
