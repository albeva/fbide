//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/symbols/SymbolTable.hpp"

namespace fbide {
class Context;
class Document;
class FBSciLexer;

/// Result delivered to the sink wxEvtHandler when a parse finishes.
struct IntellisenseResult {
    const Document* owner;
    std::shared_ptr<const SymbolTable> symbols;
};

wxDECLARE_EVENT(EVT_INTELLISENSE_RESULT, wxThreadEvent);

/// Background lex + parse pipeline. One worker thread, single-task slot
/// (latest wins). Owned by `DocumentManager`. Posts results to a sink
/// wxEvtHandler on the UI thread via `wxQueueEvent`.
///
/// Identity: each task carries a `Document*` tag. The worker never
/// dereferences it — it's only round-tripped to the result handler.
/// The handler must verify the Document is still alive before applying.
class IntellisenseService final : public wxThreadHelper {
public:
    NO_COPY_AND_MOVE(IntellisenseService)

    /// Construct and start the worker. `sink` receives EVT_INTELLISENSE_RESULT
    /// events on the UI thread.
    IntellisenseService(Context& ctx, wxEvtHandler* sink);

    /// Stop the worker and join. Pending and in-flight results are dropped.
    ~IntellisenseService() override;

    /// Submit a snapshot for parsing. Replaces any pending task.
    void submit(Document* owner, const wxString& content);

    /// Cancel any pending or in-flight task tagged with `doc`. Safe to call
    /// from the UI thread. After return, no result for `doc` will be posted
    /// (a result race may still arrive but will carry a stale tag — handler
    /// must validate via DocumentManager::contains).
    void cancel(const Document* doc);

    /// wxThreadHelper entry point. Runs on the worker thread.
    auto Entry() -> wxThread::ExitCode override;

private:
    struct Task {
        Document* owner = nullptr;
        wxString content;
    };

    void process(Task task);

    Context& m_ctx;
    wxEvtHandler* m_sink;

    /// Lexer owned by the worker. Configured once at ctor with current
    /// keywords from ConfigManager. Worker is the only thread that touches it.
    FBSciLexer* m_lexer = nullptr;
    /// Reused buffer for the worker's text snapshot.
    MemoryDocument m_memDoc;

    /// Slot + signalling for the latest-wins single-task queue.
    /// `m_mtx` guards `m_pending` and `m_stopRequested`. The worker waits
    /// on `m_cv` for either a new task or shutdown.
    wxMutex m_mtx;
    wxCondition m_cv { m_mtx };
    std::optional<Task> m_pending;
    bool m_stopRequested = false;

    /// Set by the worker right before processing; cleared on completion or
    /// by `cancel`. When cleared mid-parse the worker drops the result.
    std::atomic<const Document*> m_inFlight { nullptr };
};

} // namespace fbide
