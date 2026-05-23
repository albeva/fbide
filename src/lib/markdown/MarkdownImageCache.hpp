//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide::markdown {

/**
 * Async cache of inline images for markdown rendering.
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
 * data, mailto, …) is marked Failed immediately. Downloads exceeding 2 MiB
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
class MarkdownImageCache final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(MarkdownImageCache)

    enum class State : std::uint8_t {
        Loading,
        Ready,
        Failed
    };

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

    /// Default cap on Ready entries. Failed entries are not counted (they
    /// carry no bitmap, so they're cheap to keep around).
    static constexpr std::size_t kDefaultMaxReady = 32;

    explicit MarkdownImageCache(std::size_t maxReady = kDefaultMaxReady);
    ~MarkdownImageCache() override;

    /// Look up `url`. Kicks off an async download on first request and
    /// returns a Loading entry; subsequent calls return the same entry.
    /// A `get` on an existing Ready entry counts as use and refreshes its
    /// LRU position. Reference is stable until `clearAll()`, eviction or
    /// destruction.
    [[nodiscard]] auto get(const wxString& url) -> const Entry&;

    /// Insert a Ready entry directly from an already-decoded bitmap —
    /// for code paths that have the image data on hand without going
    /// through the HTTP downloader, and for test setup. Eviction kicks
    /// in if this would exceed the Ready cap.
    void insertReady(const wxString& url, wxBitmap bitmap, int width, int height);

    /// True when `url` has any entry in the cache (any state). Side-effect
    /// free — does not touch the LRU and does not start a download.
    [[nodiscard]] auto contains(const wxString& url) const -> bool;

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
    void fail(Entry& entry, const wxString& url) const;

    /// LRU bookkeeping. `markReady` promotes a URL to most-recently-used
    /// (inserting into the order if absent), then evicts the oldest
    /// Ready entries until the cap is respected. `touchReady` is the
    /// no-eviction variant for use during lookup.
    void markReady(const std::string& key);
    void touchReady(const std::string& key);
    void evictOldestReady();
    void forgetLru(const std::string& key);

    [[nodiscard]] static auto allowedScheme(const wxString& url) -> bool;

    Listener m_listener;
    std::size_t m_maxReady;
    std::unordered_map<std::string, Entry> m_entries;       ///< URL → entry.
    std::unordered_map<int, std::string> m_requestUrls;     ///< Request id → URL key.
    std::unordered_map<int, wxWebRequest> m_activeRequests; ///< Keep handles alive while
                                                            ///< downloads run; destroying
                                                            ///< them removes wx's temp files.

    /// LRU order over Ready entries. Front = least-recently-used. The
    /// iterator map gives O(1) splice on touch.
    std::list<std::string> m_lru;
    std::unordered_map<std::string, std::list<std::string>::iterator> m_lruIter;
};

} // namespace fbide::markdown
