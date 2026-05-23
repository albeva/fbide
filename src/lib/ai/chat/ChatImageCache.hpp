//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <string>
#include <wx/webrequest.h>

namespace fbide {

/**
 * Async cache of inline chat images.
 *
 * Markdown image inlines (`![alt](url)`) are downloaded via `wxWebRequest`
 * with `Storage_File` — wx writes the response to a temp file under the
 * OS temp directory and removes it automatically when the request handle
 * is destroyed. Once the file lands, the cache decodes it into a
 * `wxBitmap` (in memory) and drops the request, so the on-disk footprint
 * lasts only as long as a single download.
 *
 * Lookups are non-blocking: `get()` returns the current state, kicking
 * off a download the first time a URL is seen. The cache notifies a
 * registered listener (on the UI thread) when an entry transitions to
 * Ready or Failed so the chat view can relayout.
 *
 * Only `http://` and `https://` URLs are downloaded; anything else (file,
 * data, mailto, …) is marked Failed immediately. Downloads exceeding 5 MiB
 * or images larger than 4096 px on either axis are rejected.
 *
 * Cleanup: the destructor cancels every in-flight download and drops
 * every cached bitmap; wx removes the per-request temp files as the
 * request handles go away. `clearAll()` does the same without tearing
 * down the cache itself — used when the conversation is cleared.
 *
 * Threading: UI thread only. `wxWebRequest` events are dispatched on the
 * UI thread by wx, so no synchronisation is required.
 */
class ChatImageCache final : public wxEvtHandler {
public:
    enum class State : std::uint8_t { Loading,
        Ready,
        Failed };

    /// One cache entry. `bitmap` is valid only when `state == Ready`.
    struct Entry {
        State state = State::Loading;
        wxBitmap bitmap; ///< Decoded image — valid for Ready entries.
        int width = 0;   ///< Pixel width of the decoded image.
        int height = 0;  ///< Pixel height.
    };

    /// Listener fired (on the UI thread) when an entry settles. Receives
    /// the URL whose state just transitioned to Ready or Failed.
    using Listener = std::function<void(const wxString& url)>;

    ChatImageCache();
    ~ChatImageCache() override;
    NO_COPY_AND_MOVE(ChatImageCache)

    /// Look up `url`. Kicks off an async download on first request and
    /// returns a Loading entry; subsequent calls return the same entry.
    /// Reference is stable until `clearAll()` or destruction.
    [[nodiscard]] auto get(const wxString& url) -> const Entry&;

    /// Cancel in-flight downloads and drop every cached entry. The cache
    /// stays usable afterwards. Used when the conversation is cleared.
    void clearAll();

    void setListener(Listener listener) { m_listener = std::move(listener); }

private:
    void onRequestState(wxWebRequestEvent& event);

    /// Decode the completed download into `entry`, applying the size
    /// guards. Transitions the entry to Ready or Failed and notifies the
    /// listener.
    void finalize(Entry& entry, const wxString& url, const wxString& path);

    /// Mark `entry` failed and notify the listener.
    void fail(Entry& entry, const wxString& url);

    [[nodiscard]] static auto allowedScheme(const wxString& url) -> bool;

    Listener m_listener;
    std::unordered_map<std::string, Entry> m_entries;       ///< URL → entry.
    std::unordered_map<int, std::string> m_requestUrls;     ///< Request id → URL key.
    std::unordered_map<int, wxWebRequest> m_activeRequests; ///< Keep handles alive while
                                                            ///< downloads run; destroying
                                                            ///< them removes wx's temp files.
};

} // namespace fbide
