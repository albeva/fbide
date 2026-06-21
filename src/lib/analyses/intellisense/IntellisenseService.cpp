//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "IntellisenseService.hpp"
#include <fstream>
#include <iterator>
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "analyses/parser/TreeParser.hpp"
#include "app/Context.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
#include "utils/PathConversions.hpp"
using namespace fbide;

wxDEFINE_EVENT(fbide::EVT_INTELLISENSE_RESULT, wxThreadEvent);
wxDEFINE_EVENT(fbide::EVT_INTELLISENSE_TRACKED_FILES, wxThreadEvent);
wxDEFINE_EVENT(fbide::EVT_INTELLISENSE_COMPLETION, wxThreadEvent);

namespace {

/// Lowercase ASCII in place (FreeBASIC identifiers/keywords are ASCII-cased).
void toLowerAscii(std::string& text) {
    std::ranges::transform(text, text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
}

/// Sort case-insensitively (FreeBASIC is case-insensitive) and drop duplicates.
void sortUniqueCI(std::vector<wxString>& names) {
    std::ranges::sort(names, [](const wxString& lhs, const wxString& rhs) { return lhs.CmpNoCase(rhs) < 0; });
    names.erase(std::unique(names.begin(), names.end(),
                    [](const wxString& lhs, const wxString& rhs) { return lhs.CmpNoCase(rhs) == 0; }),
        names.end());
}

/// Resolve a raw `#include` target (quotes already stripped): first relative to
/// the including file's directory, then against each configured search directory
/// (compiler `inc/`, absolute `-i` dirs, cwd). Returns empty when no file is
/// found, so unresolved includes are simply skipped.
auto resolveInclude(const wxString& raw, const std::filesystem::path& dir,
    const std::vector<std::filesystem::path>& searchDirs) -> std::filesystem::path {
    const std::filesystem::path target = toFsPath(raw);
    const auto exists = [](const std::filesystem::path& candidate) {
        std::error_code ec;
        return std::filesystem::is_regular_file(candidate, ec);
    };
    if (target.is_absolute()) {
        auto candidate = target.lexically_normal();
        return exists(candidate) ? candidate : std::filesystem::path {};
    }
    if (auto candidate = (dir / target).lexically_normal(); exists(candidate)) {
        return candidate;
    }
    for (const auto& base : searchDirs) {
        if (auto candidate = (base / target).lexically_normal(); exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

/// Graph key for an unsaved/untitled buffer, which has no on-disk path. Derived
/// from the opaque owner identity so each unsaved document gets a unique, stable
/// node. The key is relative, so it never collides with a resolved include (those
/// are always absolute), and the entry is never read from disk — it takes its
/// source from the editor buffer.
auto standaloneKey(const Document* owner) -> std::filesystem::path {
    return toFsPath(wxString::Format("untitled-%p", static_cast<const void*>(owner)));
}

/// Read a file as raw UTF-8 bytes (stripping a leading BOM). Returns false when
/// the file cannot be opened.
auto readFile(const std::filesystem::path& path, std::string& out) -> bool {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return false;
    }
    const std::streamsize size = stream.tellg();
    if (size < 0) {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (size > 0) {
        stream.read(out.data(), size);
    }
    if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF
        && static_cast<unsigned char>(out[1]) == 0xBB && static_cast<unsigned char>(out[2]) == 0xBF) {
        out.erase(0, 3); // UTF-8 BOM
    }
    return true;
}

/// Collect every open document reachable from `start` by walking parent edges
/// (cycle-safe) — the documents whose include closure contains `start`.
void collectOwnedAncestors(SourceEntry& start, std::unordered_set<SourceEntry*>& out) {
    std::unordered_set<SourceEntry*> seen;
    std::vector<SourceEntry*> stack { &start };
    while (!stack.empty()) {
        auto* const entry = stack.back();
        stack.pop_back();
        if (!seen.insert(entry).second) {
            continue;
        }
        if (entry->isOwned()) {
            out.insert(entry);
        }
        for (auto* const parent : entry->parents) {
            stack.push_back(parent);
        }
    }
}

/// Flatten the transitive include closure of `root` into a de-duplicated list of
/// the includes' own symbol tables (cycle-safe; skips not-yet-parsed entries).
auto flatClosure(SourceEntry& root) -> std::vector<std::shared_ptr<const SymbolTable>> {
    std::vector<std::shared_ptr<const SymbolTable>> result;
    std::unordered_set<SourceEntry*> seen;
    std::vector<SourceEntry*> stack(root.includes.begin(), root.includes.end());
    while (!stack.empty()) {
        auto* const entry = stack.back();
        stack.pop_back();
        if (!seen.insert(entry).second) {
            continue;
        }
        if (entry->symbolTable) {
            result.push_back(entry->symbolTable);
        }
        for (auto* const child : entry->includes) {
            stack.push_back(child);
        }
    }
    return result;
}

} // namespace

IntellisenseService::IntellisenseService(Context& ctx, wxEvtHandler* sink)
: m_ctx(ctx)
, m_sink(sink) {
    m_lexer = static_cast<FBSciLexer*>(FBSciLexer::Create());
    m_parser = std::make_unique<parser::TreeParser>(parser::ParseOptions { .lean = true });
    // Keywords come from the shared table (built at startup / on settings change
    // via FBSciLexer::setKeywords) — no per-instance configuration.

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
        m_commands.clear();
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

void IntellisenseService::closeDocument(const Document* owner, std::filesystem::path path) {
    // An unsaved document (empty path) never entered the graph, but it may still
    // have a completion context cached by `post`, so queue the Close regardless:
    // the worker erases that context and the graph-side close is a no-op.
    wxMutexLocker lock(m_mtx);
    if (m_stopRequested) {
        return;
    }
    m_commands.push_back(
        Command { .type = CommandType::Close, .owner = const_cast<Document*>(owner), .path = std::move(path) }
    );
    m_cv.Signal();
}

void IntellisenseService::submit(Document* owner, std::filesystem::path path, std::string content) {
    if (owner == nullptr) {
        return;
    }
    wxMutexLocker lock(m_mtx);
    if (m_stopRequested) {
        return;
    }
    m_commands.push_back(Command {
        .type = CommandType::Submit, .owner = owner, .path = std::move(path), .content = std::move(content) });
    m_cv.Signal();
}

void IntellisenseService::refreshFile(std::filesystem::path path) {
    if (path.empty()) {
        return;
    }
    wxMutexLocker lock(m_mtx);
    if (m_stopRequested) {
        return;
    }
    m_commands.push_back(Command { .type = CommandType::Refresh, .path = std::move(path) });
    m_cv.Signal();
}

void IntellisenseService::setIncludePaths(std::vector<std::filesystem::path> paths) {
    wxMutexLocker lock(m_mtx);
    if (m_stopRequested) {
        return;
    }
    m_commands.push_back(Command { .type = CommandType::IncludePaths, .includeDirs = std::move(paths) });
    m_cv.Signal();
}

void IntellisenseService::setDefines(std::unordered_set<std::string> defines) {
    wxMutexLocker lock(m_mtx);
    if (m_stopRequested) {
        return;
    }
    m_commands.push_back(Command { .type = CommandType::Defines, .defines = std::move(defines) });
    m_cv.Signal();
}

void IntellisenseService::resendTrackedFiles() {
    wxMutexLocker lock(m_mtx);
    if (m_stopRequested) {
        return;
    }
    m_commands.push_back(Command { .type = CommandType::ResendTracked });
    m_cv.Signal();
}

void IntellisenseService::requestCompletion(Document* owner, int pos, std::string prefix, std::size_t seq, int maxItems) {
    if (owner == nullptr) {
        return;
    }
    wxMutexLocker lock(m_mtx);
    if (m_stopRequested) {
        return;
    }
    m_commands.push_back(Command { .type = CommandType::Completion, .owner = owner, .pos = pos, .prefix = std::move(prefix), .seq = seq, .maxItems = maxItems });
    m_cv.Signal();
}

void IntellisenseService::setKeywords(std::vector<wxString> keywords) {
    wxMutexLocker lock(m_mtx);
    if (m_stopRequested) {
        return;
    }
    m_commands.push_back(Command { .type = CommandType::Keywords, .keywords = std::move(keywords) });
    m_cv.Signal();
}

auto IntellisenseService::Entry() -> wxThread::ExitCode {
    while (true) {
        std::vector<Command> commands;
        {
            wxMutexLocker lock(m_mtx);
            while (!m_stopRequested && m_commands.empty()) {
                m_cv.Wait();
            }
            if (m_stopRequested) {
                return nullptr;
            }
            commands.swap(m_commands);
        }
        // Coalesce completion requests — only the newest in the batch matters
        // (the editor also drops out-of-order results via seq).
        std::size_t lastCompletion = commands.size();
        for (std::size_t i = 0; i < commands.size(); ++i) {
            if (commands[i].type == CommandType::Completion) {
                lastCompletion = i;
            }
        }
        for (std::size_t i = 0; i < commands.size(); ++i) {
            if (commands[i].type == CommandType::Completion && i != lastCompletion) {
                continue; // superseded by a newer completion request
            }
            applyCommand(std::move(commands[i]));
        }
        drainAndDeliver();
    }
}

void IntellisenseService::applyCommand(Command command) {
    switch (command.type) {
    case CommandType::IncludePaths:
        // Include resolution depends on the search dirs, so re-resolve every
        // tracked file's includes against the new set. The UI only sends this
        // when the dirs actually changed (so it never fires on a plain edit).
        m_searchDirs = std::move(command.includeDirs);
        m_graph.reparseAll();
        break;
    case CommandType::Defines:
        // Branch selection depends on the define set, so re-parse every tracked
        // file. The UI only sends this when the set actually changed. Wrapped in
        // a shared_ptr so every table built this drain points at the one set.
        m_defines = std::make_shared<const std::unordered_set<std::string>>(std::move(command.defines));
        m_graph.reparseAll();
        break;
    case CommandType::Close: {
        // Close under the key the entry is actually enrolled with: a Save As can
        // change the document's path after its last submit, so command.path may
        // no longer match the graph entry.
        const auto it = m_ownerKey.find(command.owner);
        const auto key = it != m_ownerKey.end()
                           ? std::move(it->second)
                           : (command.path.empty() ? standaloneKey(command.owner) : std::move(command.path));
        m_graph.closeDocument(key, command.owner);
        if (it != m_ownerKey.end()) {
            m_ownerKey.erase(it);
        }
        m_completionCtx.erase(command.owner);
        if (command.owner == m_complGlobalsOwner) {
            m_complGlobalsOwner = nullptr; // invalidate the global cache for a reused address
        }
        break;
    }
    case CommandType::Submit: {
        // Unsaved buffers have no on-disk path; key them on the owner so they
        // still join the include graph and resolve their #includes.
        auto key = command.path.empty() ? standaloneKey(command.owner) : std::move(command.path);
        // A changed path (untitled buffer saved, or a Save As rename) leaves the
        // previous entry owned, so the orphan sweep never reclaims it — close it
        // under the old key first.
        if (const auto it = m_ownerKey.find(command.owner); it != m_ownerKey.end() && it->second != key) {
            m_graph.closeDocument(it->second, command.owner);
        }
        m_graph.openDocument(key, command.owner); // ensure ownership
        m_graph.submit(key, std::move(command.content));
        m_ownerKey[command.owner] = std::move(key);
        break;
    }
    case CommandType::Refresh:
        // Only refresh a file already in the graph — never resurrect one a sweep
        // dropped. A vanished file re-parses as empty so its symbols drop.
        if (auto* const entry = m_graph.find(command.path); entry != nullptr) {
            std::string content;
            readFile(command.path, content); // false -> content stays empty
            m_graph.submit(command.path, std::move(content));
        }
        break;
    case CommandType::ResendTracked:
        m_lastTrackedSet.clear(); // force the end-of-drain snapshot to re-post
        break;
    case CommandType::Completion:
        generateCompletion(command);
        break;
    case CommandType::Keywords:
        m_keywords = std::move(command.keywords);
        sortUniqueCI(m_keywords);
        break;
    }
}

auto IntellisenseService::parse(const std::string& source) -> std::shared_ptr<SymbolTable> {
    m_memDoc.Set(std::string_view { source }); // copy — the caller retains the source
    m_lexer->Lex(0, m_memDoc.Length(), +ThemeCategory::Default, &m_memDoc);

    lexer::MemoryDocStyledSource styled(m_memDoc);
    lexer::StyleLexer adapter(styled);
    adapter.tokenise(m_tokens);

    auto table = std::make_shared<SymbolTable>();
    table->populate(m_parser->parse(m_tokens, {}), m_defines);
    return table;
}

void IntellisenseService::parseEntry(SourceEntry& entry) {
    entry.symbolTable = parse(entry.source);
    entry.symbolTable->setSourcePath(entry.path); // so symbols are locatable across files
    entry.parsedStamp = entry.contentStamp;
    resolveAndWire(entry);
}

void IntellisenseService::resolveAndWire(SourceEntry& entry) {
    std::vector<std::filesystem::path> resolved;
    const auto dir = entry.path.parent_path();
    for (const auto& include : entry.symbolTable->getIncludes()) {
        if (auto path = resolveInclude(include.path, dir, m_searchDirs); !path.empty()) {
            resolved.push_back(std::move(path));
        }
    }
    // Newly referenced includes have no content yet — read them from disk and
    // submit so the worker parses them in this same drain.
    for (auto* const created : m_graph.setIncludes(&entry, resolved)) {
        if (std::string content; readFile(created->path, content)) {
            m_graph.submit(created->path, std::move(content));
        }
    }
}

void IntellisenseService::drainAndDeliver() {
    // Each file is dequeued at most once per drain (the `queued` flag dedups the
    // queue), so these stay unique without set semantics.
    std::vector<SourceEntry*> parsed;  // entries (re)parsed this pass
    std::vector<SourceEntry*> changed; // ... whose own symbols actually changed

    while (auto* const entry = m_graph.takeNext()) {
        const std::size_t before = entry->symbolTable ? entry->symbolTable->getHash() : 0;
        const bool hadTable = entry->symbolTable != nullptr;
        parseEntry(*entry);
        parsed.push_back(entry);
        if (!hadTable || entry->symbolTable->getHash() != before) {
            changed.push_back(entry);
        }
    }

    // Deliver every open document that was parsed directly, plus every open
    // document whose include closure changed (an ancestor of a changed file).
    std::unordered_set<SourceEntry*> toDeliver;
    for (auto* const entry : parsed) {
        if (entry->isOwned()) {
            toDeliver.insert(entry);
        }
    }
    for (auto* const entry : changed) {
        collectOwnedAncestors(*entry, toDeliver);
    }

    for (auto* const root : toDeliver) {
        if (root->symbolTable == nullptr) {
            parseEntry(*root); // never parsed yet (e.g. a just-opened ancestor)
        }
        // Publish the entry's own table shared (no copy) alongside its include
        // closure; the UI combines them. The own table stays import-free, so it
        // is freely shareable into other documents' closures and reusable here.
        post(root->owner, root->symbolTable, flatClosure(*root));
    }

    // Sweep includes orphaned by an edit (a parent dropped its #include) so they
    // leave the tracked-files snapshot, then publish the set to the UI watcher.
    m_graph.collectOrphans();
    postTrackedFiles();
}

void IntellisenseService::generateCompletion(const Command& cmd) {
    std::vector<wxString> items;
    const int cap = cmd.maxItems > 0 ? cmd.maxItems : 100;

    // Assemble in priority order (a local shadows a global shadows a keyword),
    // filtering by the typed prefix (case-insensitive) and capping at `cap`.
    std::string prefixLower = cmd.prefix;
    toLowerAscii(prefixLower);
    std::unordered_set<std::string> seen;
    const auto append = [&](const std::vector<wxString>& bucket) {
        for (const auto& name : bucket) {
            if (static_cast<int>(items.size()) >= cap) {
                return;
            }
            std::string key = name.utf8_string();
            toLowerAscii(key);
            if (!key.starts_with(prefixLower)) {
                continue;
            }
            if (seen.insert(std::move(key)).second) {
                items.push_back(name);
            }
        }
    };

    // Symbol buckets need the document's parsed context; keywords don't, so they
    // are appended unconditionally below — a just-created or not-yet-parsed
    // document still offers keyword completion.
    if (const auto it = m_completionCtx.find(cmd.owner);
        it != m_completionCtx.end() && it->second.own != nullptr) {
        const CompletionContext& ctx = it->second;
        const SymbolTable& own = *ctx.own;

        // Global buckets (own + closure) — cached and rebuilt only when the owner
        // or its closure hash changes, so typing within one file reuses them.
        std::size_t hash = own.getHash();
        for (const auto& imp : ctx.imported) {
            if (imp != nullptr) {
                hash ^= imp->getHash() + 0x9e3779b9U + (hash << 6) + (hash >> 2);
            }
        }
        if (cmd.owner != m_complGlobalsOwner || hash != m_complGlobalsHash) {
            m_complGlobalSymbols.clear();
            m_complGlobalVariables.clear();
            own.globalSymbolCompletions(m_complGlobalSymbols);
            own.moduleVariableCompletions(m_complGlobalVariables);
            for (const auto& imp : ctx.imported) {
                if (imp != nullptr) {
                    imp->globalSymbolCompletions(m_complGlobalSymbols);
                    imp->moduleVariableCompletions(m_complGlobalVariables);
                }
            }
            sortUniqueCI(m_complGlobalSymbols);
            sortUniqueCI(m_complGlobalVariables);
            m_complGlobalsOwner = cmd.owner;
            m_complGlobalsHash = hash;
        }

        // Per-caret local buckets (scope-dependent, not cached).
        std::vector<wxString> localVariables;
        std::vector<wxString> localSymbols;
        own.localCompletionsAt(cmd.pos, localVariables);
        own.memberCompletionsAt(cmd.pos, localSymbols, ctx.imported);
        sortUniqueCI(localVariables);
        sortUniqueCI(localSymbols);

        append(localVariables);
        append(localSymbols);
        append(m_complGlobalVariables);
        append(m_complGlobalSymbols);
    }
    append(m_keywords);

    wxThreadEvent* const event = make_unowned<wxThreadEvent>(EVT_INTELLISENSE_COMPLETION);
    event->SetPayload(CompletionResult { .owner = cmd.owner, .seq = cmd.seq, .items = std::move(items) });
    wxQueueEvent(m_sink, event);
}

void IntellisenseService::post(const Document* owner, std::shared_ptr<const SymbolTable> own,
    std::vector<std::shared_ptr<const SymbolTable>> imported) {
    // Stash the context so a completion request can be answered from the owner
    // alone, without re-walking the graph.
    m_completionCtx[owner] = CompletionContext { .own = own, .imported = imported };
    wxThreadEvent* const event = make_unowned<wxThreadEvent>(EVT_INTELLISENSE_RESULT);
    event->SetPayload(
        IntellisenseResult { .owner = owner, .own = std::move(own), .imported = std::move(imported) }
    );
    wxQueueEvent(m_sink, event);
}

void IntellisenseService::postTrackedFiles() {
    auto paths = m_graph.pureIncludePaths();
    std::ranges::sort(paths);
    if (paths == m_lastTrackedSet) {
        return; // unchanged since last post — nothing for the UI to reconcile
    }
    m_lastTrackedSet = paths;
    wxThreadEvent* const event = make_unowned<wxThreadEvent>(EVT_INTELLISENSE_TRACKED_FILES);
    event->SetPayload(std::move(paths));
    wxQueueEvent(m_sink, event);
}
