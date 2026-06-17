//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentWatcher.hpp"
#include "Document.hpp"
#include "DocumentManager.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "utils/PathConversions.hpp"
using namespace fbide;

namespace {
// Coalesce window for the event burst an atomic save (write-temp + rename,
// or truncate + rewrite) produces — process once it settles, then re-stat.
constexpr int kDebounceMs = 300;

constexpr int kFsEvents = wxFSW_EVENT_CREATE | wxFSW_EVENT_DELETE | wxFSW_EVENT_RENAME | wxFSW_EVENT_MODIFY;
} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(DocumentWatcher, wxEvtHandler)
    EVT_FSWATCHER(wxID_ANY, DocumentWatcher::onFsEvent)
    EVT_TIMER(wxID_ANY,     DocumentWatcher::onDebounce)
wxEND_EVENT_TABLE()
// clang-format on

DocumentWatcher::DocumentWatcher(Context& ctx)
: m_ctx(ctx) {
    m_debounce.SetOwner(this);
}

DocumentWatcher::~DocumentWatcher() {
    stop();
}

void DocumentWatcher::applyConfig() {
    if (m_ctx.getConfigManager().config().get_or("editor.autoReload", true)) {
        start();
    } else {
        stop();
    }
}

void DocumentWatcher::start() {
    if (m_watcher != nullptr) {
        return;
    }
    m_watcher = std::make_unique<wxFileSystemWatcher>();
    m_watcher->SetOwner(this);
    // App-activation re-stat is the safety net for events the OS watcher
    // misses (network drives, NFS). Delivered to the app, so bound there.
    wxTheApp->Bind(wxEVT_ACTIVATE_APP, &DocumentWatcher::onActivateApp, this);
    for (const auto& doc : m_ctx.getDocumentManager().getDocuments()) {
        addDocument(*doc);
    }
}

void DocumentWatcher::stop() {
    if (m_watcher == nullptr) {
        return;
    }
    wxTheApp->Unbind(wxEVT_ACTIVATE_APP, &DocumentWatcher::onActivateApp, this);
    m_debounce.Stop();
    m_watcher.reset();
    m_watchedDirs.clear();
    m_includeStamps.clear();
    // Drop any pending UI the feature put up while it was on.
    for (const auto& doc : m_ctx.getDocumentManager().getDocuments()) {
        if (doc->getPendingExternal() != Document::ExternalChange::None) {
            doc->setPendingExternal(Document::ExternalChange::None);
            doc->hideExternalBar();
        }
    }
}

void DocumentWatcher::addDocument(Document& doc) {
    if (!isEnabled() || doc.isNew()) {
        return;
    }
    watchDir(doc.getFilePath().parent_path());
}

void DocumentWatcher::removeDocument(const Document& doc) {
    if (!isEnabled() || doc.isNew()) {
        return;
    }
    unwatchDir(doc.getFilePath().parent_path());
}

void DocumentWatcher::watchDir(const std::filesystem::path& dir) {
    if (dir.empty()) {
        return;
    }
    const auto key = dir.lexically_normal();
    const auto [it, inserted] = m_watchedDirs.try_emplace(key, 0);
    if (inserted) {
        std::error_code ec;
        if (!std::filesystem::is_directory(key, ec)) {
            m_watchedDirs.erase(it);
            return;
        }
        m_watcher->Add(wxFileName::DirName(toWxString(key)), kFsEvents);
    }
    it->second++;
}

void DocumentWatcher::unwatchDir(const std::filesystem::path& dir) {
    if (dir.empty()) {
        return;
    }
    const auto key = dir.lexically_normal();
    const auto it = m_watchedDirs.find(key);
    if (it == m_watchedDirs.end()) {
        return;
    }
    if (--it->second <= 0) {
        m_watcher->Remove(wxFileName::DirName(toWxString(key)));
        m_watchedDirs.erase(it);
    }
}

void DocumentWatcher::onFsEvent(wxFileSystemWatcherEvent& event) {
    event.Skip();
    // Restart the debounce — the directory is small and re-stat is cheap, so
    // we don't bother mapping the event back to a specific document here.
    m_debounce.StartOnce(kDebounceMs);
}

void DocumentWatcher::onDebounce(wxTimerEvent& /*event*/) {
    syncAll();
}

void DocumentWatcher::onActivateApp(wxActivateEvent& event) {
    event.Skip();
    if (event.GetActive()) {
        syncAll();
    }
}

void DocumentWatcher::syncAll() {
    if (!isEnabled()) {
        return;
    }
    // handleChange never mutates the document list, so a span walk is safe.
    for (const auto& doc : m_ctx.getDocumentManager().getDocuments()) {
        handleChange(*doc);
    }
    syncIncludes();
}

auto DocumentWatcher::stampOf(const std::filesystem::path& path) -> IncludeStamp {
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return {}; // missing/unreadable — distinct from any real (mtime, size)
    }
    std::error_code sizeEc;
    const auto size = std::filesystem::file_size(path, sizeEc);
    return { mtime, sizeEc ? 0 : size };
}

void DocumentWatcher::setIncludeWatches(std::vector<std::filesystem::path> includes) {
    if (!isEnabled()) {
        return;
    }
    std::set<std::filesystem::path> desired;
    for (auto& path : includes) {
        desired.insert(path.lexically_normal());
    }
    // Unwatch includes no longer tracked.
    for (auto it = m_includeStamps.begin(); it != m_includeStamps.end();) {
        if (desired.contains(it->first)) {
            ++it;
        } else {
            unwatchDir(it->first.parent_path());
            it = m_includeStamps.erase(it);
        }
    }
    // Watch newly tracked includes (their parent dir, refcounted alongside open
    // documents), seeding each one's change baseline.
    for (const auto& path : desired) {
        if (m_includeStamps.try_emplace(path, stampOf(path)).second) {
            watchDir(path.parent_path());
        }
    }
}

void DocumentWatcher::syncIncludes() {
    for (auto& [path, stamp] : m_includeStamps) {
        if (const auto current = stampOf(path); current != stamp) {
            stamp = current;
            m_ctx.getDocumentManager().reparseInclude(path);
        }
    }
}

void DocumentWatcher::handleChange(Document& doc) {
    if (doc.isNew()) {
        return;
    }
    std::error_code ec;
    if (!std::filesystem::exists(doc.getFilePath(), ec)) {
        if (doc.getPendingExternal() != Document::ExternalChange::Deleted) {
            // The on-disk file is gone, so the buffer is now unsaved — mark it
            // dirty (refreshing the tab's [*]) before surfacing the bar.
            doc.setModified(true);
            m_ctx.getDocumentManager().refreshTabTitle(doc);
            presentChange(doc, /*deleted*/ true);
        }
        return;
    }
    if (!doc.checkExternalChange()) {
        return; // unchanged, or our own save (mod-time already re-baselined)
    }
    if (!doc.isModified()) {
        m_ctx.getDocumentManager().applyReload(doc); // clean buffer → silent reload
        return;
    }
    presentChange(doc, /*deleted*/ false);
}

void DocumentWatcher::presentChange(Document& doc, const bool deleted) {
    const auto kind = deleted ? Document::ExternalChange::Deleted : Document::ExternalChange::Conflict;
    doc.setPendingExternal(kind);
    // Surface the bar only on the active tab; a background tab shows it when
    // it next gains focus (via flushPending). The notification bar is the
    // whole signal — the tab title is left untouched.
    if (m_ctx.getDocumentManager().getActive() == &doc) {
        doc.showExternalBar(kind);
    }
}

void DocumentWatcher::flushPending(Document& doc) const {
    if (!isEnabled()) {
        return;
    }
    const auto kind = doc.getPendingExternal();
    if (kind != Document::ExternalChange::None) {
        doc.showExternalBar(kind);
    }
}
