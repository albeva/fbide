//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ChatImageCache.hpp"
using namespace fbide;

namespace {

/// Cap on the downloaded payload. Chat images are screenshots / icons /
/// diagrams — not photos — so a 2 MiB cap is generous in practice and
/// keeps the synchronous decode (`wxImage::LoadFile`) under ~50 ms even
/// on slower machines. The check runs at completion time because the
/// HTTP Content-Length isn't always present mid-stream.
constexpr unsigned long long kMaxBytes = 2ULL * 1024 * 1024;

/// Hard cap on decoded dimensions. Anything larger almost certainly means
/// something went wrong upstream and would blow memory if we drew it.
constexpr int kMaxPixels = 4096;

constexpr int kHttpErrorStatus = 400;

} // namespace

ChatImageCache::ChatImageCache(const std::size_t maxReady)
: m_maxReady(std::max<std::size_t>(1, maxReady)) {
    Bind(wxEVT_WEBREQUEST_STATE, &ChatImageCache::onRequestState, this);
}

ChatImageCache::~ChatImageCache() {
    clearAll();
}

auto ChatImageCache::get(const wxString& url) -> const Entry& {
    const std::string key = url.utf8_string();
    if (auto it = m_entries.find(key); it != m_entries.end()) {
        // Lookup counts as use — keep Ready entries fresh in the LRU.
        if (it->second.state == State::Ready) {
            touchReady(key);
        }
        return it->second;
    }

    auto [it, inserted] = m_entries.emplace(key, Entry {});
    (void)inserted;
    Entry& entry = it->second;

    if (!allowedScheme(url)) {
        entry.state = State::Failed;
        return entry;
    }

    wxWebRequest request = wxWebSession::GetDefault().CreateRequest(this, url);
    if (!request.IsOk()) {
        entry.state = State::Failed;
        return entry;
    }
    request.SetStorage(wxWebRequest::Storage_File);
    const int id = request.GetId();
    m_requestUrls[id] = key;
    m_activeRequests.emplace(id, request);
    request.Start();
    return entry;
}

void ChatImageCache::clearAll() {
    // Cancel in-flight downloads first; dropping the handles afterwards
    // lets wx remove its per-request temp files.
    for (auto& [id, request] : m_activeRequests) {
        if (request.GetState() == wxWebRequest::State_Active) {
            request.Cancel();
        }
    }
    m_activeRequests.clear();
    m_requestUrls.clear();
    m_entries.clear();
    m_lru.clear();
    m_lruIter.clear();
}

void ChatImageCache::insertReady(const wxString& url, wxBitmap bitmap, const int width, const int height) {
    const std::string key = url.utf8_string();
    auto [it, inserted] = m_entries.try_emplace(key, Entry {});
    (void)inserted;
    Entry& entry = it->second;
    entry.state = State::Ready;
    entry.bitmap = std::move(bitmap);
    entry.width = width;
    entry.height = height;
    markReady(key);
}

auto ChatImageCache::contains(const wxString& url) const -> bool {
    return m_entries.contains(url.utf8_string());
}

void ChatImageCache::markReady(const std::string& key) {
    // Already in the LRU? Move to MRU position. Otherwise append.
    if (const auto it = m_lruIter.find(key); it != m_lruIter.end()) {
        m_lru.splice(m_lru.end(), m_lru, it->second);
    } else {
        m_lru.push_back(key);
        m_lruIter[key] = std::prev(m_lru.end());
    }
    while (m_lru.size() > m_maxReady) {
        evictOldestReady();
    }
}

void ChatImageCache::touchReady(const std::string& key) {
    if (const auto it = m_lruIter.find(key); it != m_lruIter.end()) {
        m_lru.splice(m_lru.end(), m_lru, it->second);
    }
}

void ChatImageCache::evictOldestReady() {
    if (m_lru.empty()) {
        return;
    }
    const std::string oldest = m_lru.front();
    m_lru.pop_front();
    m_lruIter.erase(oldest);
    m_entries.erase(oldest);
}

void ChatImageCache::forgetLru(const std::string& key) {
    if (const auto it = m_lruIter.find(key); it != m_lruIter.end()) {
        m_lru.erase(it->second);
        m_lruIter.erase(it);
    }
}

void ChatImageCache::onRequestState(wxWebRequestEvent& event) {
    const auto reqIt = m_requestUrls.find(event.GetId());
    if (reqIt == m_requestUrls.end()) {
        return; // Cancelled / cleared — drop the late event.
    }

    const std::string key = reqIt->second;
    const auto entryIt = m_entries.find(key);
    if (entryIt == m_entries.end()) {
        m_requestUrls.erase(reqIt);
        m_activeRequests.erase(event.GetId());
        return;
    }
    Entry& entry = entryIt->second;
    const wxString url = wxString::FromUTF8(key);

    switch (event.GetState()) {
    case wxWebRequest::State_Completed: {
        const int status = event.GetResponse().GetStatus();
        const wxString path = event.GetResponse().GetDataFile();
        // `wxWebRequest` follows HTTP redirects through the platform
        // backend (NSURLSession / WinHTTP / libcurl), and the
        // cross-scheme policy varies. Re-check the *final* URL's scheme
        // against the allow-list so an https → file:// (or data:)
        // redirect can't sneak past `allowedScheme` — which only sees
        // the originally requested URL.
        const wxString finalUrl = event.GetResponse().GetURL();
        // Decode BEFORE dropping the request handle — wx removes the temp
        // file when the last handle is destroyed.
        if (status >= kHttpErrorStatus || path.empty() || !allowedScheme(finalUrl)) {
            fail(entry, url);
        } else {
            finalize(entry, url, path);
        }
        m_requestUrls.erase(reqIt);
        m_activeRequests.erase(event.GetId());
        break;
    }
    case wxWebRequest::State_Failed:
    case wxWebRequest::State_Unauthorized:
    case wxWebRequest::State_Cancelled:
        m_requestUrls.erase(reqIt);
        m_activeRequests.erase(event.GetId());
        fail(entry, url);
        break;
    case wxWebRequest::State_Idle:
    case wxWebRequest::State_Active:
        break;
    }
}

void ChatImageCache::finalize(Entry& entry, const wxString& url, const wxString& path) {
    if (wxFileName::GetSize(path) > wxULongLong(kMaxBytes)) {
        fail(entry, url);
        return;
    }
    wxImage image;
    {
        // Silence wxImage's own error log on a bad payload — we report
        // via the cache state instead.
        const wxLogNull noLog;
        if (!image.LoadFile(path, wxBITMAP_TYPE_ANY)) {
            fail(entry, url);
            return;
        }
    }
    if (image.GetWidth() <= 0 || image.GetHeight() <= 0
        || image.GetWidth() > kMaxPixels || image.GetHeight() > kMaxPixels) {
        fail(entry, url);
        return;
    }
    entry.bitmap = wxBitmap(image);
    entry.width = image.GetWidth();
    entry.height = image.GetHeight();
    entry.state = State::Ready;
    markReady(url.utf8_string());
    if (m_listener) {
        m_listener(url);
    }
}

void ChatImageCache::fail(Entry& entry, const wxString& url) {
    entry.state = State::Failed;
    entry.bitmap = wxBitmap {};
    entry.width = 0;
    entry.height = 0;
    if (m_listener) {
        m_listener(url);
    }
}

auto ChatImageCache::allowedScheme(const wxString& url) -> bool {
    return url.StartsWith("http://") || url.StartsWith("https://");
}
