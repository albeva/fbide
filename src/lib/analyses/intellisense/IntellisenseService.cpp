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

namespace {

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

/// Read a file as raw UTF-8 bytes (stripping a leading BOM). Returns false when
/// the file cannot be opened.
auto readFile(const std::filesystem::path& path, std::string& out) -> bool {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char> {});
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
    if (path.empty()) {
        return; // unsaved document — never entered the graph
    }
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

void IntellisenseService::resendTrackedFiles() {
    wxMutexLocker lock(m_mtx);
    if (m_stopRequested) {
        return;
    }
    m_commands.push_back(Command { .type = CommandType::ResendTracked });
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
        for (auto& command : commands) {
            applyCommand(std::move(command));
        }
        drainAndDeliver();
    }
}

void IntellisenseService::applyCommand(Command command) {
    switch (command.type) {
    case CommandType::IncludePaths:
        m_searchDirs = std::move(command.includeDirs);
        break;
    case CommandType::Close:
        m_graph.closeDocument(command.path, command.owner);
        break;
    case CommandType::Submit:
        if (command.path.empty()) {
            parseStandalone(command.owner, command.content); // unsaved — no graph, no includes
        } else {
            m_graph.openDocument(command.path, command.owner); // ensure ownership
            m_graph.submit(command.path, std::move(command.content));
        }
        break;
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
    }
}

auto IntellisenseService::parse(const std::string& source) -> std::shared_ptr<SymbolTable> {
    m_memDoc.Set(std::string_view { source }); // copy — the caller retains the source
    m_lexer->Lex(0, m_memDoc.Length(), +ThemeCategory::Default, &m_memDoc);

    lexer::MemoryDocStyledSource styled(m_memDoc);
    lexer::StyleLexer adapter(styled);
    adapter.tokenise(m_tokens);

    auto table = std::make_shared<SymbolTable>();
    table->populate(m_parser->parse(m_tokens, {}));
    return table;
}

void IntellisenseService::parseStandalone(const Document* owner, const std::string& source) {
    post(owner, parse(source));
}

void IntellisenseService::parseEntry(SourceEntry& entry) {
    entry.symbolTable = parse(entry.source);
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
    std::unordered_set<SourceEntry*> parsed;  // entries (re)parsed this pass
    std::unordered_set<SourceEntry*> changed; // ... whose own symbols actually changed

    while (auto* const entry = m_graph.takeNext()) {
        const std::size_t before = entry->symbolTable ? entry->symbolTable->getHash() : 0;
        const bool hadTable = entry->symbolTable != nullptr;
        parseEntry(*entry);
        parsed.insert(entry);
        if (!hadTable || entry->symbolTable->getHash() != before) {
            changed.insert(entry);
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
        if (!parsed.contains(root)) {
            parseEntry(*root); // own text unchanged, but an include did — refresh for a fresh table
        }
        root->symbolTable->setImported(flatClosure(*root));
        post(root->owner, root->symbolTable);
    }

    // Sweep includes orphaned by an edit (a parent dropped its #include) so they
    // leave the tracked-files snapshot, then publish the set to the UI watcher.
    m_graph.collectOrphans();
    postTrackedFiles();
}

void IntellisenseService::post(const Document* owner, std::shared_ptr<const SymbolTable> symbols) {
    wxThreadEvent* const event = make_unowned<wxThreadEvent>(EVT_INTELLISENSE_RESULT);
    event->SetPayload(IntellisenseResult { .owner = owner, .symbols = std::move(symbols) });
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
