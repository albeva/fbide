//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "IntellisenseService.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
#include "format/transformers/reformat/ReFormatter.hpp"
using namespace fbide;

wxDEFINE_EVENT(fbide::EVT_INTELLISENSE_RESULT, wxThreadEvent);

IntellisenseService::IntellisenseService(Context& ctx, wxEvtHandler* sink)
: m_ctx(ctx)
, m_sink(sink) {
    m_lexer = static_cast<FBSciLexer*>(FBSciLexer::Create());
    lexer::configureFbWordlists(*m_lexer, m_ctx.getConfigManager().keywords().at("groups"));

    if (CreateThread(wxTHREAD_JOINABLE) != wxTHREAD_NO_ERROR) {
        wxLogError("IntellisenseService: failed to create worker thread");
        return;
    }
    if (GetThread()->Run() != wxTHREAD_NO_ERROR) {
        wxLogError("IntellisenseService: failed to start worker thread");
    }
}

IntellisenseService::~IntellisenseService() {
    {
        wxMutexLocker lock(m_mtx);
        m_stopRequested = true;
        m_pending.reset();
        m_cv.Signal();
    }
    if (auto* thread = GetThread(); thread != nullptr && thread->IsRunning()) {
        thread->Wait();
    }
    if (m_lexer != nullptr) {
        m_lexer->Release();
        m_lexer = nullptr;
    }
}

void IntellisenseService::submit(Document* owner, const wxString& content) {
    if (owner == nullptr) {
        return;
    }
    // wxString uses non-atomic copy-on-write refcounting and isn't safe to
    // share across threads. Round-trip through UTF-8 so the wxString placed
    // in the Task shares no buffer with any UI-thread wxString.
    std::string isolated = content.utf8_string();
    wxMutexLocker lock(m_mtx);
    if (m_stopRequested) {
        return;
    }
    m_pending = Task { .owner = owner, .content = std::move(isolated) };
    m_cv.Signal();
}

void IntellisenseService::cancel(const Document* doc) {
    wxMutexLocker lock(m_mtx);
    if (m_pending.has_value() && m_pending->owner == doc) {
        m_pending.reset();
    }
    // Clear the in-flight tag if it matches — worker checks after parse and
    // drops the result if it no longer matches. Use compare_exchange to
    // avoid stomping a different document's flag.
    const Document* expected = doc;
    m_inFlight.compare_exchange_strong(expected, nullptr);
}

void IntellisenseService::prune() {
    wxMutexLocker lock(m_mtx);
    bool keptIdle = false;
    std::erase_if(m_pool, [&keptIdle](const std::shared_ptr<SymbolTable>& slot) {
        if (slot.use_count() > 1) {
            return false; // still referenced — keep
        }
        if (!keptIdle) {
            keptIdle = true;
            return false; // keep one idle slot ready for next parse
        }
        return true;
    });
}

auto IntellisenseService::acquireSymbolTable() -> std::shared_ptr<SymbolTable> {
    wxMutexLocker lock(m_mtx);
    for (auto& slot : m_pool) {
        if (slot.use_count() == 1) {
            slot->reset();
            return slot;
        }
    }
    return m_pool.emplace_back(std::make_shared<SymbolTable>());
}

auto IntellisenseService::Entry() -> wxThread::ExitCode {
    while (true) {
        Task task;
        {
            wxMutexLocker lock(m_mtx);
            while (!m_stopRequested && !m_pending.has_value()) {
                m_cv.Wait();
            }
            if (m_stopRequested) {
                return nullptr;
            }
            task = std::move(*m_pending);
            m_pending.reset();
        }

        m_inFlight.store(task.owner);
        process(task);
    }
}

void IntellisenseService::process(const Task& task) {
    m_memDoc.Set(std::string_view { task.content.data(), task.content.size() });
    m_lexer->Lex(0, m_memDoc.Length(), +ThemeCategory::Default, &m_memDoc);

    lexer::MemoryDocStyledSource src(m_memDoc);
    lexer::StyleLexer adapter(src);
    adapter.tokenise(m_tokens);

    reformat::ReFormatter parser({ .lean = true });
    const auto tree = parser.buildTree(m_tokens);

    // Reuse a pooled SymbolTable when one is idle (no Document holds it),
    // otherwise grow the pool. Repopulate runs in place — vector capacity
    // is preserved across parses, cutting per-keystroke allocations.
    auto symbols = acquireSymbolTable();
    symbols->populate(tree);

    // Atomic check + clear: only deliver if no cancel hit between dispatch
    // and now. Don't post for documents the UI has since dropped.
    const Document* expected = task.owner;
    if (!m_inFlight.compare_exchange_strong(expected, nullptr)) {
        return; // cancelled
    }

    wxThreadEvent* event = make_unowned<wxThreadEvent>(EVT_INTELLISENSE_RESULT);
    event->SetPayload(IntellisenseResult {
        .owner = task.owner,
        .symbols = std::move(symbols),
    });
    wxQueueEvent(m_sink, event);
}
