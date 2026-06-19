//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "analyses/intellisense/SourceGraph.hpp"
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/lexer/Token.hpp"
#include "analyses/symbols/SymbolTable.hpp"

namespace fbide::parser {
class TreeParser;
}

namespace fbide {
class Context;
class Document;
class FBSciLexer;

/// Result delivered to the sink wxEvtHandler when a document's parse finishes.
/// The own table is shared straight from the graph entry (never copied); the
/// closure rides alongside, and the UI combines the two at query time.
struct IntellisenseResult {
    const Document* owner;                                    ///< Tag the worker received (never dereferenced by the worker).
    std::shared_ptr<const SymbolTable> own;                   ///< The document's own symbol table (shared, not copied).
    std::vector<std::shared_ptr<const SymbolTable>> imported; ///< Flattened transitive `#include` closure.
};

wxDECLARE_EVENT(EVT_INTELLISENSE_RESULT, wxThreadEvent);

/// Posted after each parse drain with the current set of pure-include file paths
/// (closed `#include`s) the worker tracks, so the UI can reconcile its filesystem
/// watches. Payload: `std::vector<std::filesystem::path>`.
wxDECLARE_EVENT(EVT_INTELLISENSE_TRACKED_FILES, wxThreadEvent);

/// Worker-generated completion candidates for the editor: already prefix-filtered,
/// priority-ordered, de-duplicated and capped. The editor shows them only if the
/// request is still wanted (its accept-flag) and current (`seq`).
struct CompletionResult {
    const Document* owner;       ///< Identity tag (re-validated by the UI).
    std::size_t seq = 0;         ///< Echoes the request seq; the editor drops out-of-order results.
    std::vector<wxString> items; ///< Candidates, priority then alphabetical, <= the requested cap.
};

wxDECLARE_EVENT(EVT_INTELLISENSE_COMPLETION, wxThreadEvent);

/// Background lex + parse + include-resolution pipeline. One worker thread that
/// owns a `SourceGraph` of open documents and their `#include`s; UI-thread calls
/// post commands which the worker applies, then parses every dirty file, wires
/// the include graph, and re-publishes each affected open document (its own
/// symbols + the flattened closure of its includes). Results post to a sink
/// wxEvtHandler on the UI thread via `wxQueueEvent`.
///
/// Identity: each command carries a `Document*` tag. The worker never
/// dereferences it — it round-trips to the result handler, which must verify the
/// document is still alive before applying.
class IntellisenseService final : public wxThreadHelper {
public:
    NO_COPY_AND_MOVE(IntellisenseService)

    /// Construct and start the worker. `sink` receives EVT_INTELLISENSE_RESULT
    /// events on the UI thread.
    IntellisenseService(Context& ctx, wxEvtHandler* sink);

    /// Stop the worker and join. Pending commands and results are dropped.
    ~IntellisenseService() override;

    /// Unregister a document; its graph entry and any includes it alone kept
    /// alive are collected. Call just before the `Document` is destroyed.
    void closeDocument(const Document* owner, std::filesystem::path path);

    /// Submit a fresh source snapshot for a document. With a path the document
    /// joins the include graph; with an empty path it is parsed standalone.
    void submit(Document* owner, std::filesystem::path path, std::string content);

    /// Re-read a tracked `#include` file from disk and re-parse it (and any open
    /// documents that include it). Called by the UI when the file changed on disk
    /// while not open in a tab. No-op when the path is not already in the graph.
    void refreshFile(std::filesystem::path path);

    /// Set the global `#include` search directories (compiler `inc/`, absolute
    /// `-i` dirs, cwd) used to resolve relative includes. Applied on the worker.
    void setIncludePaths(std::vector<std::filesystem::path> paths);

    /// Set the symbol names treated as defined for preprocessor branch selection
    /// (compiler built-ins + `-d` command-line defines, lowercased). Applied on
    /// the worker; re-parses every tracked file like `setIncludePaths`.
    void setDefines(std::unordered_set<std::string> defines);

    /// Force the next drain to re-post EVT_INTELLISENSE_TRACKED_FILES even when
    /// the tracked set is unchanged. Used when the UI watcher re-enables and must
    /// rebuild its include watches from scratch.
    void resendTrackedFiles();

    /// Request completion candidates for `owner` at caret byte offset `pos`,
    /// filtered by `prefix` (case-insensitive) and capped at `maxItems`. Generated
    /// on the worker from the document's last-parsed table + its include closure +
    /// the keyword set, then delivered via EVT_INTELLISENSE_COMPLETION echoing
    /// `seq`. Only the newest pending request per drain is processed.
    void requestCompletion(Document* owner, int pos, std::string prefix, std::size_t seq, int maxItems);

    /// Set the keyword completion candidates (FB keywords / constants / PP),
    /// included as the lowest-priority bucket. Pushed by the editor when its
    /// keyword list is (re)built; does not trigger a re-parse.
    void setKeywords(std::vector<wxString> keywords);

    /// wxThreadHelper entry point. Runs on the worker thread.
    auto Entry() -> wxThread::ExitCode override;

private:
    enum class CommandType : std::uint8_t { Close,
        Submit,
        IncludePaths,
        Defines,
        Refresh,
        ResendTracked,
        Completion,
        Keywords };
    /// A UI-thread request, applied to the graph on the worker thread.
    struct Command {
        CommandType type;
        Document* owner = nullptr;                      ///< Identity tag (never dereferenced).
        std::filesystem::path path;                     ///< File path; empty means an unsaved document.
        std::string content;                            ///< Source snapshot (Submit only).
        std::vector<std::filesystem::path> includeDirs; ///< Search dirs (IncludePaths only).
        std::unordered_set<std::string> defines;        ///< Defined names (Defines only).
        int pos = 0;                                    ///< Caret byte offset (Completion only).
        std::string prefix;                             ///< Identifier prefix to filter by (Completion only).
        std::size_t seq = 0;                            ///< Completion request sequence (Completion only).
        int maxItems = 0;                               ///< Max candidates to return (Completion only).
        std::vector<wxString> keywords;                 ///< Keyword candidates (Keywords only).
    };

    void applyCommand(Command command);
    /// Generate + post completion candidates for a `Completion` command, drawing
    /// on the owner's stashed completion context (own table + closure) + keywords.
    void generateCompletion(const Command& command);
    void parseStandalone(const Document* owner, const std::string& source);
    void parseEntry(SourceEntry& entry);
    void resolveAndWire(SourceEntry& entry);
    void drainAndDeliver();
    [[nodiscard]] auto parse(const std::string& source) -> std::shared_ptr<SymbolTable>;
    void post(const Document* owner, std::shared_ptr<const SymbolTable> own,
        std::vector<std::shared_ptr<const SymbolTable>> imported);
    /// Snapshot the graph's pure-include paths and post EVT_INTELLISENSE_TRACKED_FILES
    /// when the set changed since the last post (throttled).
    void postTrackedFiles();

    [[maybe_unused]] Context& m_ctx; ///< Application context (for future include-path resolution).
    wxEvtHandler* m_sink;            ///< UI-thread event sink for `EVT_INTELLISENSE_RESULT`.

    // Worker-thread-owned — no synchronisation (exclusive from construction).
    FBSciLexer* m_lexer = nullptr;                   ///< Lexer; only the worker touches it.
    MemoryDocument m_memDoc;                         ///< Reused text buffer for the worker's parse.
    std::unique_ptr<parser::TreeParser> m_parser;    ///< Reused lean tree parser.
    std::vector<lexer::Token> m_tokens;              ///< Reused token buffer.
    SourceGraph m_graph;                             ///< The source/include graph (worker-owned).
    std::vector<std::filesystem::path> m_searchDirs; ///< `#include` search dirs (compiler inc/, -i, cwd).
    /// Shared define set for `#if` branch selection; shared into every table a
    /// drain builds so they don't each copy it. Null until the first `Defines`.
    std::shared_ptr<const std::unordered_set<std::string>> m_defines;
    std::vector<std::filesystem::path> m_lastTrackedSet; ///< Last posted pure-include set (sorted); throttles snapshots.

    /// Per-document completion context (own table + flattened closure), stashed by
    /// `post` and erased on close. Keyed by the opaque `Document*` so completion
    /// works for unsaved documents too (no graph entry). Worker-thread only.
    struct CompletionContext {
        std::shared_ptr<const SymbolTable> own;
        std::vector<std::shared_ptr<const SymbolTable>> imported;
    };
    std::unordered_map<const Document*, CompletionContext> m_completionCtx;
    std::vector<wxString> m_keywords; ///< Keyword candidates (lowest-priority bucket; sorted/deduped).
    // Cached global completion buckets for the last context generated — rebuilt
    // only when the owner or its closure hash changes (so typing reuses them).
    const Document* m_complGlobalsOwner = nullptr;
    std::size_t m_complGlobalsHash = 0;
    std::vector<wxString> m_complGlobalSymbols;
    std::vector<wxString> m_complGlobalVariables;

    // Shared state — guarded by `m_mtx`.
    wxMutex m_mtx;
    wxCondition m_cv { m_mtx };      ///< Signals new commands / shutdown to the worker.
    std::vector<Command> m_commands; ///< Pending UI commands, drained each wake.
    bool m_stopRequested = false;    ///< Set by the destructor; the worker exits its loop.
};

} // namespace fbide
