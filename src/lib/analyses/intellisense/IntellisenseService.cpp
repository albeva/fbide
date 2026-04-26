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
    wxString isolated = wxString::FromUTF8(content.utf8_string());
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

auto IntellisenseService::Entry() -> wxThread::ExitCode {
    auto* thread = GetThread();
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

        if (thread != nullptr && thread->TestDestroy()) {
            return nullptr;
        }

        m_inFlight.store(task.owner);
        process(std::move(task));
    }
}

void IntellisenseService::process(Task task) {
    auto* thread = GetThread();

    const auto utf8 = task.content.utf8_string();
    m_memDoc.Set(std::string_view { utf8.data(), utf8.size() });
    m_lexer->Lex(0, m_memDoc.Length(), +ThemeCategory::Default, &m_memDoc);

    if (thread != nullptr && thread->TestDestroy()) {
        m_inFlight.store(nullptr);
        return;
    }

    lexer::MemoryDocStyledSource src(m_memDoc);
    lexer::StyleLexer adapter(src);
    auto tokens = adapter.tokenise();

    if (thread != nullptr && thread->TestDestroy()) {
        m_inFlight.store(nullptr);
        return;
    }

    reformat::ReFormatter parser({ .lean = true });
    const auto tree = parser.buildTree(tokens);

    if (thread != nullptr && thread->TestDestroy()) {
        m_inFlight.store(nullptr);
        return;
    }

    auto symbols = std::make_shared<SymbolTable>(tree);

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
