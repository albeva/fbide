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
 * An active FBIde session — the open documents mirrored to a `.fbs` file
 * (which files are open, caret position, encoding/EOL choices).
 *
 * Lifetime *is* the session, and `DocumentManager` owns one `unique_ptr` to it:
 *   - **Construct = load.** Binding to an existing `.fbs` opens the documents it
 *     lists; binding to a new path creates the file from the currently open
 *     documents. Either way the session is now active.
 *   - **Destroy = save.** The destructor writes the open documents back.
 *
 * So `DocumentManager` starts a session by creating the object and ends one by
 * resetting it — it holds no session logic itself. The chrome (title, the
 * Close-Session command) is refreshed by `DocumentManager` around those calls,
 * because at shutdown the `CommandManager` dies before this object does.
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
 * Legacy text formats (v0.1 unversioned, v0.2 XML-ish header) still load for
 * backwards compatibility. Every save writes v3.
 *
 * **Owned by:** `DocumentManager` (`m_session`).
 *
 * See @ref documents.
 */
class FileSession final {
public:
    NO_COPY_AND_MOVE(FileSession)

    /// Bind a session to `path`. Construction does no I/O — call `load()` to
    /// activate it; the destructor writes the open documents back.
    FileSession(Context& ctx, wxString path);

    /// Activate the session: enable the Close-Session command and, when the file
    /// exists, open the documents it lists and refresh the title.
    void load();

    /// Save the open documents back to the session file, then deactivate (clear
    /// the session from the title, disable Close Session). Never throws.
    ~FileSession();

    /// The session file path.
    [[nodiscard]] auto getPath() const -> const wxString& { return m_path; }

    /// Display name — the `.fbs` filename stem.
    [[nodiscard]] auto getName() const -> wxString;

private:
    /// Current session format version.
    static constexpr int Version = 3;

    /// Set command entry state
    void updateUi(bool loaded);

    /// Snapshot the open documents to the session file. Records paths + caret
    /// state only — modified buffers are not auto-saved. Logs on I/O failure.
    void save() const;

    /// Load the v3 INI format.
    void loadV3() const;

    /// Load the v0.1/v0.2 legacy text format.
    void loadLegacy() const;

    Context& m_ctx;          ///< Application context.
    wxString m_path;         ///< Session `.fbs` path this object mirrors.
    bool m_isLoaded = false; ///< Session has been loaded
};

} // namespace fbide
