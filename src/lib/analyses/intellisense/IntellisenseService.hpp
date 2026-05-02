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
#include "analyses/lexer/Token.hpp"

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

    /// Drop excess idle entries from the SymbolTable pool. Keeps at most
    /// one slot whose `use_count` is 1 (held only by the pool); evicts the
    /// rest so capacity stays bounded after document closes. Slots still
    /// referenced by a Document are untouched.
    void prune();

    /// wxThreadHelper entry point. Runs on the worker thread.
    auto Entry() -> wxThread::ExitCode override;

private:
    struct Task {
        Document* owner = nullptr;
        std::string content;
    };

    void process(const Task& task);

    /// Acquire a SymbolTable slot from the pool: scan for an entry whose
    /// `use_count` is 1 (held only by the pool — i.e. no Document or event
    /// payload still references it) and return it. If none, allocate a new
    /// slot and return that. Caller calls `repopulate()` on the returned
    /// table before publishing it.
    auto acquireSymbolTable() -> std::shared_ptr<SymbolTable>;

    Context& m_ctx;
    wxEvtHandler* m_sink;

    /// Lexer owned by the worker. Configured once at ctor with current
    /// keywords from ConfigManager. Worker is the only thread that touches it.
    FBSciLexer* m_lexer = nullptr;
    /// Reused buffer for the worker's text snapshot.
    MemoryDocument m_memDoc;
    std::vector<lexer::Token> m_tokens;

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

    /// Pool of recyclable SymbolTable instances. Guarded by `m_mtx`.
    /// A slot's `use_count` doubles as a liveness flag: 1 means only the
    /// pool holds it (idle, reusable); >1 means a Document or in-flight
    /// event payload still reads it. Pool grows as needed and is trimmed
    /// by `prune()` on document close.
    std::vector<std::shared_ptr<SymbolTable>> m_pool;
};

} // namespace fbide
