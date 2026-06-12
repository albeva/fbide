//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <wx/fswatcher.h>

namespace fbide {
class Context;
class Document;

/**
 * Watches every open document's file for external changes and applies the
 * auto-reload policy: a clean buffer is silently reloaded; a dirty buffer
 * surfaces a non-modal conflict bar (now if the tab is focused, deferred
 * with a tab marker otherwise). Deletion is handled too.
 *
 * **Mechanism:** a `wxFileSystemWatcher` (GUI-thread events, no worker
 * thread) watches the *parent directories* of open files — the platform
 * APIs are directory-oriented — and a short debounce coalesces the burst
 * of events an atomic save (temp-file + rename) produces. As a safety net
 * for missed events (network drives, NFS), the document set is also
 * re-stat'd whenever the application regains focus.
 *
 * **Owned by:** `DocumentManager`. **Threading:** UI thread only.
 *
 * Toggling the `editor.autoReload` setting calls `applyConfig()`, which
 * creates or destroys the underlying watcher.
 */
class DocumentWatcher final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(DocumentWatcher)

    explicit DocumentWatcher(Context& ctx);
    ~DocumentWatcher() override;

    /// Read `editor.autoReload` and start or stop the watcher accordingly.
    void applyConfig();

    /// Begin watching a freshly opened document's directory.
    void addDocument(Document& doc);
    /// Stop watching a document's directory (call before its path changes
    /// or the document is closed, while it still holds the old path).
    void removeDocument(const Document& doc);

    /// Re-stat every open document immediately (used on app activation and
    /// after a settings change). No-op when disabled.
    void syncAll();

    /// Show a deferred conflict/deleted bar for a document that just became
    /// the active tab. No-op when it has no pending external change.
    void flushPending(Document& doc) const;

    /// True when the underlying watcher is live (feature enabled).
    [[nodiscard]] auto isEnabled() const -> bool { return m_watcher != nullptr; }

    /// Release the watcher and unbind app hooks. Must be called while the
    /// event loop is still alive (e.g. on frame close) — a wxFileSystemWatcher
    /// destroyed during late app teardown faults on its event source.
    void shutdown() { stop(); }

private:
    void start();
    void stop();

    void watchDir(const std::filesystem::path& dir);
    void unwatchDir(const std::filesystem::path& dir);

    void onFsEvent(wxFileSystemWatcherEvent& event);
    void onDebounce(wxTimerEvent& event);
    void onActivateApp(wxActivateEvent& event);

    /// Apply the reload policy to a single document.
    void handleChange(Document& doc);
    /// Mark a document changed/deleted: show the bar now if it is the active
    /// tab, otherwise defer behind a tab marker.
    void presentChange(Document& doc, bool deleted);

    Context& m_ctx;
    std::unique_ptr<wxFileSystemWatcher> m_watcher;     ///< Null while the feature is off.
    wxTimer m_debounce;                                 ///< Coalesces event bursts from atomic saves.
    std::map<std::filesystem::path, int> m_watchedDirs; ///< Watched directory → open-file refcount.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
