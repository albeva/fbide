//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentManager.hpp"
#include "Document.hpp"
#include "DocumentIO.hpp"
#include "DocumentPath.hpp"
#include "DocumentWatcher.hpp"
#include "FileSession.hpp"
#include "analyses/intellisense/IntellisenseService.hpp"
#include "app/Context.hpp"
#include "command/CommandEntry.hpp"
#include "command/CommandId.hpp"
#include "command/CommandManager.hpp"
#include "compiler/CompileCommand.hpp"
#include "compiler/CompilerConfigCatalog.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "editor/CodeTransformer.hpp"
#include "editor/Editor.hpp"
#include "sidebar/SideBarManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {
constexpr auto SESSION_EXT = "fbs";
// Case-insensitive path identity (defined with the other path helpers below);
// declared here for the session-reopen guard in startSession.
auto samePath(const std::filesystem::path& lhs, const std::filesystem::path& rhs) -> bool;
const wxWindowID kTabCloseOthersId = wxNewId();
const wxWindowID kTabShowInBrowserId = wxNewId();
const wxWindowID kTabReloadFromDiskId = wxNewId();
const wxWindowID kGoToDefinitionId = wxNewId();
const wxWindowID kGoToDeclarationId = wxNewId();

/// File dialog → the session path to save to, or empty when cancelled.
auto promptSaveSessionPath(Context& ctx) -> wxString {
    wxFileDialog dlg(
        ctx.getUIManager().getMainFrame(),
        ctx.tr("files.sessionSaveTitle"),
        "", wxString(".") + SESSION_EXT,
        ctx.getConfigManager().filePattern("session"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );
    return dlg.ShowModal() == wxID_OK ? dlg.GetPath() : wxString {};
}
} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(DocumentManager, wxEvtHandler)
    EVT_FIND(wxID_ANY,              DocumentManager::onFindDialog)
    EVT_FIND_NEXT(wxID_ANY,         DocumentManager::onFindDialogNext)
    EVT_FIND_REPLACE(wxID_ANY,      DocumentManager::onReplaceDialog)
    EVT_FIND_REPLACE_ALL(wxID_ANY,  DocumentManager::onReplaceAllDialog)
    EVT_FIND_CLOSE(wxID_ANY,        DocumentManager::onFindDialogClose)
wxEND_EVENT_TABLE()
// clang-format on

DocumentManager::DocumentManager(Context& ctx)
: m_ctx(ctx)
, m_codeTransformer(std::make_unique<CodeTransformer>(ctx.getConfigManager()))
, m_intellisense(std::make_unique<IntellisenseService>(ctx, this))
, m_watcher(std::make_unique<DocumentWatcher>(ctx)) {
    Bind(EVT_INTELLISENSE_RESULT, &DocumentManager::onIntellisenseResult, this);
    Bind(EVT_INTELLISENSE_TRACKED_FILES, &DocumentManager::onIntellisenseTrackedFiles, this);
}

DocumentManager::~DocumentManager() = default;

auto DocumentManager::defaultEncoding() const -> TextEncoding {
    const auto& editor = m_ctx.getConfigManager().config().at("editor");
    const auto key = editor.get_or("encoding", "UTF-8");
    return TextEncoding::parse(key.ToStdString()).value_or(TextEncoding::UTF8);
}

auto DocumentManager::defaultEolMode() const -> EolMode {
    const auto& editor = m_ctx.getConfigManager().config().at("editor");
    const auto key = editor.get_or("eolMode", "LF");
    return EolMode::parse(key.ToStdString()).value_or(EolMode::LF);
}

// ---------------------------------------------------------------------------
// File handling
// ---------------------------------------------------------------------------

auto DocumentManager::newFile(DocumentType type) -> Document& {
    const auto thaw = m_ctx.getUIManager().freeze();
    auto* notebook = getNotebook();
    auto& doc = *m_documents.emplace_back(std::make_unique<Document>(notebook, m_ctx, type));
    notebook->AddPage(doc.getPage(), doc.getTitle(), true);
    return doc;
}

void DocumentManager::openFile() {
    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("files.loadTitle"),
        "",
        ".bas",
        m_ctx.getConfigManager().filePatterns({ "fbide", "properties", "markdown", "batch", "bash", "makefile", "json", "css", "text", "all" }),
        wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE
    );
    dlg.SetFilterIndex(0); // pre-select the FBIde group (first filter) as the default

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    wxArrayString paths;
    dlg.GetPaths(paths);
    for (const auto& path : paths) {
        openFile(path);
    }
}

auto DocumentManager::openInclude(const Document& origin, const wxString& includePath) -> Document* {
    if (includePath.empty()) {
        return nullptr;
    }

    const auto tryOpen = [this](const std::filesystem::path& candidate) -> Document* {
        if (candidate.empty()) {
            return nullptr;
        }
        const auto full = canonicalizePath(candidate);
        std::error_code ec;
        if (!std::filesystem::exists(full, ec)) {
            return nullptr;
        }
        return openFile(full);
    };

    const auto req = toFsPath(includePath);
    if (req.is_absolute()) {
        return tryOpen(req);
    }

    // The origin document's active compiler configuration supplies both
    // the -i search dirs (its compile command) and the stock inc/ folder
    // (its fbc binary) — the same compiler a build of this document uses.
    const auto& cfg = m_ctx.getCompilerManager().catalog().resolveByPinnedSlug(origin.getConfiguration());

    // Search order mirrors fbc's own resolution of `#include "..."`:
    //   1. the source file's folder,
    //   2. the -i directories,
    //   3. the compiler's default inc/,
    // with the current working directory as a final FBIde-only fallback.

    // 1. Relative to the source file's folder.
    if (!origin.isNew()) {
        if (auto* doc = tryOpen(origin.getFilePath().parent_path() / req)) {
            return doc;
        }
    }

    // 2. Directories passed to fbc via -i in the compile command. fbc
    //    resolves relative ones against its working directory (the source
    //    file's folder), so do the same; absolute ones apply even to an
    //    unsaved document.
    for (auto dir : CompileCommand::extractIncludePaths(cfg.compileCommand)) {
        if (dir.is_relative()) {
            if (origin.isNew()) {
                continue; // no source folder to anchor a relative -i path
            }
            dir = origin.getFilePath().parent_path() / dir;
        }
        if (auto* doc = tryOpen(dir / req)) {
            return doc;
        }
    }

    // 3. Compiler `inc/` folder: <dir-of-fbc>/inc/<path>.
    if (!cfg.path.empty()) {
        auto fbc = cfg.path;
        if (fbc.is_relative()) {
            fbc = toFsPath(m_ctx.getConfigManager().getAppDir()) / fbc;
        }
        if (auto* doc = tryOpen(fbc.parent_path() / "inc" / req)) {
            return doc;
        }
    }

    // 4. Current working directory.
    {
        std::error_code ec;
        const auto cwd = std::filesystem::current_path(ec);
        if (!ec) {
            if (auto* doc = tryOpen(cwd / req)) {
                return doc;
            }
        }
    }

    return nullptr;
}

auto DocumentManager::openFile(const wxString& filePath) -> Document* {
    return openFile(toFsPath(filePath));
}

auto DocumentManager::openFile(const std::filesystem::path& filePath) -> Document* {
    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec)) {
        return nullptr;
    }

    const auto thaw = m_ctx.getUIManager().freeze();

    // Canonicalize once at entry — fixes duplicate-tab bug on case-insensitive
    // filesystems (macOS/Windows: `fbgfx.bi` vs `FBGFX.bi`), resolves symlinks,
    // and ensures the stored path is identity-comparable for findByPath.
    const auto canonical = canonicalizePath(filePath);
    const auto canonicalWx = toWxString(canonical);

    // Session files: activate a session for them, however the file was opened
    // (Open dialog, recent files, browser, drop). startSession loads the
    // session's documents; any previously active session is saved + dropped
    // first. Currently open documents stay open — the session merges them.
    if (const auto ext = canonical.extension().string(); ext.size() > 1 && ext.substr(1) == SESSION_EXT) {
        startSession(canonicalWx);
        m_ctx.getFileHistory().addFile(canonicalWx);
        return nullptr;
    }

    // Check if already open
    if (auto* existing = findByPath(canonical)) {
        const auto idx = findPageIndex(*existing);
        if (idx != wxNOT_FOUND) {
            getNotebook()->SetSelection(static_cast<size_t>(idx));
        }
        return existing;
    }

    // Read file first — if it fails, don't create an Editor (dangling
    // child of the notebook). Config-derived defaults seed detection.
    const auto loaded = DocumentIO::load(canonical, defaultEncoding(), defaultEolMode());
    if (!loaded.has_value()) {
        wxLogError(m_ctx.tr("messages.loadFailed"), canonicalWx);
        return nullptr;
    }

    const auto type = documentTypeFromPath(canonical);
    auto& doc = *m_documents.emplace_back(std::make_unique<Document>(getNotebook(), m_ctx, type));

    // don't reformat code on file load
    auto* editor = doc.getEditor();
    editor->disableTransforms(true);
    editor->SetText(loaded->text);
    editor->disableTransforms(false);
    editor->SetEOLMode(loaded->eolMode.toStc());
    editor->ConvertEOLs(loaded->eolMode.toStc());
    editor->EmptyUndoBuffer();
    doc.setEncoding(loaded->encoding);
    doc.setEolMode(loaded->eolMode);
    doc.setFilePath(canonical);
    doc.setModified(false);

    auto* notebook = getNotebook();
    notebook->AddPage(doc.getPage(), doc.getTitle(), true);

    m_ctx.getFileHistory().addFile(canonicalWx);
    m_watcher->addDocument(doc);

    // Initial parse: bypass throttle, submit immediately.
    submitIntellisense(&doc, loaded->text.utf8_string());
    return &doc;
}

namespace {

void reportSaveFailure(Document& doc, const DocumentIO::SaveResult result, Context& ctx,
    const TextEncoding encoding, const wxString& detail) {
    wxString message;
    if (result == DocumentIO::SaveResult::EncodingError) {
        message = wxString::Format(ctx.tr("messages.saveEncodingError"), encoding.toString().data());
    } else { // IOError — append the OS reason (e.g. "Permission denied") when known.
        message = ctx.tr("messages.saveIoError");
        if (!detail.IsEmpty()) {
            message += " " + detail;
        }
    }
    doc.showSaveError(message);
}

} // namespace

void DocumentManager::reloadWithEncoding(Document& doc, const TextEncoding encoding) {
    if (doc.isNew()) {
        return;
    }

    if (doc.isModified()) {
        const auto result = wxMessageBox(
            m_ctx.tr("messages.reloadDiscardChanges"),
            m_ctx.tr("messages.reloadTitle"),
            wxYES_NO | wxICON_QUESTION,
            m_ctx.getUIManager().getMainFrame()
        );
        if (result != wxYES) {
            return;
        }
    }

    const auto loaded = DocumentIO::loadWithEncoding(doc.getFilePath(), encoding, doc.getEolMode());
    if (!loaded.has_value()) {
        wxLogError("%s", m_ctx.tr("messages.reloadFailed"));
        return;
    }

    auto* editor = doc.getEditor();
    editor->disableTransforms(true);
    editor->SetText(loaded->text);
    editor->disableTransforms(false);
    editor->SetEOLMode(loaded->eolMode.toStc());
    editor->ConvertEOLs(loaded->eolMode.toStc());
    editor->EmptyUndoBuffer();
    doc.setEncoding(encoding);
    doc.setEolMode(loaded->eolMode);
    doc.setModified(false);
    doc.updateModTime();
    updateTabTitle(doc);
    editor->updateStatusBar();
}

auto DocumentManager::saveFile(Document& doc) -> bool {
    if (doc.isNew()) {
        return saveFileAs(doc);
    }

    // Detect external modification: file changed on disk since we loaded
    // (or last saved) it. Prompt before clobbering the on-disk version.
    if (doc.checkExternalChange()) {
        const auto result = wxMessageBox(
            m_ctx.tr("messages.externalChangeOverwrite"),
            m_ctx.tr("messages.externalChangeTitle"),
            wxYES_NO | wxICON_EXCLAMATION,
            m_ctx.getUIManager().getMainFrame()
        );
        if (result != wxYES) {
            return false;
        }
    }

    wxString ioDetail;
    const auto result = DocumentIO::save(doc.getFilePath(), doc.getEditor()->GetText(), doc.getEncoding(), doc.getEolMode(), &ioDetail);
    if (result != DocumentIO::SaveResult::Success) {
        reportSaveFailure(doc, result, m_ctx, doc.getEncoding(), ioDetail);
        return false;
    }

    doc.setModified(false);
    doc.updateModTime();
    doc.dismissExternalNotification(); // a save resolves any external-change bar
    doc.dismissSaveError();            // and clears a prior failed-save bar
    updateTabTitle(doc);
    reloadConfigIfMatches(toWxString(doc.getFilePath()));
    return true;
}

auto DocumentManager::saveFileAs(Document& doc) -> bool {
    auto& cfg = m_ctx.getConfigManager();

    // Preselect the filter from the document's type override (the
    // status-bar language menu); default to FreeBASIC otherwise. An
    // override whose type has no configured `[filePatterns]` entry
    // is ignored so the dialog still ships a usable filter.
    std::string_view typeKey = "freebasic";
    if (doc.isTypeOverridden()) {
        const auto overrideKey = documentTypeKey(doc.getType());
        const wxString patternKey = wxString::FromAscii(overrideKey.data(), overrideKey.size());
        if (!cfg.config().at("filePatterns").get_or(patternKey, "").IsEmpty()) {
            typeKey = overrideKey;
        }
    }
    const auto filter = cfg.filePatterns({ typeKey, "all" });

    // Default filename for a new document — derive the extension from
    // the resolved type's first glob entry so an HTML-overridden new
    // doc seeds with ".html", a FreeBASIC one with ".bas", etc.
    const auto defaultName = [&] -> wxString {
        if (!doc.isNew()) {
            return toWxString(doc.getFilePath().filename());
        }
        const wxString key = wxString::FromAscii(typeKey.data(), typeKey.size());
        wxString glob = cfg.config().at("filePatterns").get_or(key, "");
        glob = glob.BeforeFirst(';'); // "*.bas;*.bi" → "*.bas"
        if (glob.StartsWith("*")) {
            glob = glob.Mid(1); // "*.bas" → ".bas"
        }
        return glob;
    }();

    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("files.saveTitle"),
        "",
        defaultName,
        filter,
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );
    dlg.SetFilterIndex(0); // default to the document's type filter (FreeBASIC for new docs), not All files

    if (dlg.ShowModal() != wxID_OK) {
        return false;
    }

    const auto newPath = canonicalizePath(toFsPath(dlg.GetPath()));

    // Guard against overwriting a file that is already loaded in another
    // tab — the on-disk content would diverge from the open buffer.
    auto* clash = findByPath(newPath);
    if (clash != nullptr && clash != &doc) {
        const auto answer = wxMessageBox(
            m_ctx.tr("messages.saveOverwriteOpenMessage"),
            m_ctx.tr("messages.saveOverwriteOpenTitle"),
            wxYES_NO | wxICON_EXCLAMATION,
            m_ctx.getUIManager().getMainFrame()
        );
        if (answer != wxYES) {
            return false;
        }
    }

    wxString ioDetail;
    const auto result = DocumentIO::save(newPath, doc.getEditor()->GetText(), doc.getEncoding(), doc.getEolMode(), &ioDetail);
    if (result != DocumentIO::SaveResult::Success) {
        reportSaveFailure(doc, result, m_ctx, doc.getEncoding(), ioDetail);
        return false;
    }

    // Re-point the watcher: drop the old directory (no-op when the document
    // was untitled) before the path changes, re-register the new one after.
    m_watcher->removeDocument(doc);
    doc.setFilePath(newPath);
    doc.setModified(false);
    doc.updateModTime();
    doc.dismissExternalNotification(); // a save resolves any external-change bar
    doc.dismissSaveError();            // and clears a prior failed-save bar
    m_watcher->addDocument(doc);
    updateTabTitle(doc);
    reloadConfigIfMatches(toWxString(newPath));

    if (clash != nullptr && clash != &doc) {
        // Two tabs showing the same file is redundant. The user already
        // confirmed the overwrite, so close the mirror tab without
        // re-prompting about its now-stale buffer.
        clash->setModified(false);
        closeFile(*clash);
    }
    return true;
}

void DocumentManager::reloadFromDisk(Document& doc) {
    if (doc.isNew()) {
        return;
    }
    std::error_code ec;
    if (!std::filesystem::exists(doc.getFilePath(), ec)) {
        wxLogError("%s", m_ctx.tr("messages.reloadFailed"));
        return;
    }

    if (doc.isModified()) {
        const auto result = wxMessageBox(
            m_ctx.tr("messages.reloadDiscardChanges"),
            m_ctx.tr("messages.reloadTitle"),
            wxYES_NO | wxICON_QUESTION,
            m_ctx.getUIManager().getMainFrame()
        );
        if (result != wxYES) {
            return;
        }
    }

    applyReload(doc);
}

void DocumentManager::applyReload(Document& doc, const bool keepUndo) {
    if (doc.isNew()) {
        return;
    }

    // Reload using the document's currently-active encoding + EOL — the
    // user already chose them (or they were detected at first load), so a
    // re-detection on reload would reset their choice unexpectedly.
    const auto loaded = DocumentIO::loadWithEncoding(doc.getFilePath(), doc.getEncoding(), doc.getEolMode());
    if (!loaded.has_value()) {
        wxLogError("%s", m_ctx.tr("messages.reloadFailed"));
        return;
    }

    // Preserve the caret + scroll position so a silent (clean-buffer)
    // auto-reload doesn't jump the user to the top of the file.
    auto* editor = doc.getEditor();
    const int caret = editor->GetCurrentPos();
    const int firstVisible = editor->GetFirstVisibleLine();

    // Keep the document's existing EOL — convert the loaded text to match
    // it so the editor stays in the user-chosen line-ending mode. When
    // keeping undo, group the text + EOL changes into one action so a single
    // Undo restores the user's discarded version.
    const auto eol = doc.getEolMode().toStc();
    if (keepUndo) {
        editor->BeginUndoAction();
    }
    editor->disableTransforms(true);
    editor->SetText(loaded->text);
    editor->disableTransforms(false);
    editor->SetEOLMode(eol);
    editor->ConvertEOLs(eol);
    if (keepUndo) {
        editor->EndUndoAction();
    } else {
        editor->EmptyUndoBuffer();
    }

    editor->GotoPos(std::min(caret, editor->GetLength()));
    editor->SetFirstVisibleLine(firstVisible);

    doc.setModified(false);
    doc.updateModTime();
    updateTabTitle(doc);
    editor->updateStatusBar();

    submitIntellisense(&doc, loaded->text.utf8_string());
}

void DocumentManager::reloadConfigIfMatches(const wxString& path) const {
    // Reload the config tree only — menu/toolbar/sidebar text is not
    // refreshed in place. Layout / locale tweaks land on the next FBIde
    // launch (matches the language-change restart flow).
    if (m_ctx.getConfigManager().reloadIfKnown(path)) {
        m_ctx.getUIManager().updateSettings();
    }
}

auto DocumentManager::saveAllFiles() -> bool {
    for (auto& doc : m_documents) {
        if (doc->isModified()) {
            if (!saveFile(*doc)) {
                return false;
            }
        }
    }
    return true;
}

auto DocumentManager::closeFile(Document& doc) -> bool {
    closeDocumentIntellisense(&doc);

    if (doc.isModified()) {
        const auto result = wxMessageBox(
            wxString::Format(m_ctx.tr("messages.fileModifiedFormat"), doc.getTitle()),
            m_ctx.tr("messages.fileModifiedTitle"),
            wxYES_NO | wxCANCEL | wxICON_EXCLAMATION,
            m_ctx.getUIManager().getMainFrame()
        );

        if (result == wxCANCEL) {
            return false;
        }
        if (result == wxYES) {
            if (!saveFile(doc)) {
                return false;
            }
        }
    }

    m_watcher->removeDocument(doc);

    if (const auto idx = findPageIndex(doc); idx != wxNOT_FOUND) {
        getNotebook()->DeletePage(static_cast<size_t>(idx));
    }

    std::erase_if(m_documents, [&doc](const auto& ptr) { return ptr.get() == &doc; });

    // Sidebar: reflect post-close state. PAGE_CHANGED isn't always fired
    // by AUI when the active page is removed, so push explicitly here.
    // showSymbolsFor handles nullptr (clears) and dedups by shared_ptr.
    m_ctx.getSideBarManager().showSymbolsFor(getActive());

    // Same reason: tell the compiler manager which document is now
    // active (or none). Otherwise its cached active-document pointer
    // would dangle and the toolbar combobox / status-bar configuration
    // cell would keep showing the closed document's selection.
    m_ctx.getCompilerManager().onActiveDocumentChanged(getActive());

    // Update UI state when no documents remain
    if (m_documents.empty()) {
        m_ctx.getUIManager().setDocumentState(UIState::None);
        m_ctx.getUIManager().updateTitle();
        // Clear every per-document status-bar cell. Going through the
        // handler (rather than hardcoded field indices) means the
        // configuration-in-status-bar layout, which shifts the cells,
        // gets cleared correctly too.
        m_ctx.getUIManager().getStatusBar().clearDocumentFields();
    }

    return true;
}

auto DocumentManager::closeAllFiles() -> bool {
    while (!m_documents.empty()) {
        if (!closeFile(*m_documents.back())) {
            return false;
        }
    }
    return true;
}

auto DocumentManager::closeOtherFiles(const Document& keep) -> bool {
    // Snapshot first — closeFile mutates m_documents.
    std::vector<Document*> targets;
    targets.reserve(m_documents.size());
    for (const auto& doc : m_documents) {
        if (doc.get() != &keep) {
            targets.push_back(doc.get());
        }
    }
    for (auto* doc : targets) {
        if (!closeFile(*doc)) {
            return false;
        }
    }
    return true;
}

void DocumentManager::attachNotebook() {
    auto* notebook = getNotebook();
    notebook->Bind(wxEVT_AUINOTEBOOK_TAB_RIGHT_DOWN, &DocumentManager::onTabRightDown, this);
    // Defer the watcher's first start until the event loop is running:
    // `attachNotebook` runs during OnInit, and wxFileSystemWatcher wants a
    // live loop. `start()` enumerates the open documents, so any restored by
    // the session are registered when it fires.
    CallAfter([this] { m_watcher->applyConfig(); });
}

void DocumentManager::submitIntellisense(Document* doc, std::string content) {
    if (m_intellisense != nullptr) {
        refreshIntellisenseConfig();
        m_intellisense->submit(doc, doc->getFilePath(), std::move(content));
    }
}

void DocumentManager::refreshIntellisenseConfig() {
    if (m_intellisense == nullptr) {
        return;
    }
    auto& compilerMgr = m_ctx.getCompilerManager();
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);

    // This runs on every submit (i.e. every edit-debounce), but the inputs only
    // change on open/close/configuration changes. Skip the re-derivation unless
    // a signature over those inputs changed: each FreeBASIC document's path and
    // its resolved compiler config (command template + fbc path), plus the cwd.
    // Capturing the resolved strings (not just the slug) also catches a Settings
    // edit that mutates a config in place without changing the pinned slug.
    std::string signature;
    for (const auto& doc : m_documents) {
        if (doc->getType() != DocumentType::FreeBASIC) {
            continue;
        }
        const auto& cfg = compilerMgr.catalog().resolveByPinnedSlug(doc->getConfiguration());
        signature += doc->isNew() ? std::string {} : doc->getFilePath().generic_string();
        signature += '\n';
        signature += cfg.compileCommand.utf8_string();
        signature += '\n';
        signature += cfg.path.generic_string();
        signature += '\n';
    }
    signature += ec ? std::string {} : cwd.generic_string();
    if (m_intellisenseConfigSig == signature) {
        return;
    }
    m_intellisenseConfigSig = signature;

    // The worker resolves every include against one global search-dir set, but
    // the -i dirs and stock inc/ are per-document (each document's compiler
    // configuration), and a relative -i anchors to its own document's folder —
    // exactly as openInclude resolves them. Union the resolved dirs of every
    // open FreeBASIC document: over-approximating is harmless for completion (it
    // can only resolve more includes, never fewer), and keeps the worker's
    // search-dir model unchanged.
    std::vector<std::filesystem::path> dirs;
    const auto add = [&dirs](std::filesystem::path dir) {
        dir = dir.lexically_normal();
        if (!dir.empty() && std::ranges::find(dirs, dir) == dirs.end()) {
            dirs.push_back(std::move(dir));
        }
    };
    std::unordered_set<std::string> defines;
    for (const auto& doc : m_documents) {
        if (doc->getType() != DocumentType::FreeBASIC) {
            continue;
        }
        const auto& cfg = compilerMgr.catalog().resolveByPinnedSlug(doc->getConfiguration());
        const auto base = doc->isNew() ? std::filesystem::path {} : doc->getFilePath().parent_path();
        const auto args = CompileCommand::extractIncludesAndDefines(cfg.compileCommand);
        // -i dirs: a relative one anchors to the document's folder (fbc's working
        // dir); an unsaved document has no folder to anchor it to.
        for (auto dir : args.includePaths) {
            if (dir.is_relative()) {
                if (base.empty()) {
                    continue;
                }
                dir = base / dir;
            }
            add(std::move(dir));
        }
        // The compiler's stock inc/ (sibling of its fbc binary).
        if (!cfg.path.empty()) {
            auto fbc = cfg.path;
            if (fbc.is_relative()) {
                fbc = toFsPath(m_ctx.getConfigManager().getAppDir()) / fbc;
            }
            add(fbc.parent_path() / "inc");
        }

        // Preprocessor defines for `#if` branch selection: the compiler's built-in
        // __FB_* presence macros (probed once per compiler, cached) plus the -d
        // command-line defines (already lowercased to match the evaluator).
        for (const auto& builtin : compilerMgr.builtinDefines(cfg.path)) {
            defines.insert(builtin);
        }
        for (auto& def : args.defines) {
            defines.insert(std::move(def));
        }
    }
    if (!ec) {
        add(cwd);
    }
    if (dirs != m_includeSearchDirs) {
        m_includeSearchDirs = dirs;
        m_intellisense->setIncludePaths(dirs);
    }
    if (defines != m_intellisenseDefines) {
        m_intellisenseDefines = defines;
        m_intellisense->setDefines(defines);
    }
}

void DocumentManager::closeDocumentIntellisense(const Document* doc) {
    if (m_intellisense != nullptr) {
        m_intellisense->closeDocument(doc, doc->getFilePath());
    }
}

void DocumentManager::onIntellisenseResult(wxThreadEvent& event) {
    auto result = event.GetPayload<IntellisenseResult>();
    // Validate the document is still alive — race against close.
    if (!contains(result.owner)) {
        return;
    }
    // contains() takes const Document*; cast away to call setter.
    auto* doc = const_cast<Document*>(result.owner); // NOLINT(cppcoreguidelines-pro-type-const-cast)

    // Dim the editor's inactive #if branches from this fresh parse (read the own
    // table before it is moved into the document).
    if (auto* editor = doc->getEditor(); editor != nullptr) {
        editor->applyInactiveRanges(
            result.own ? result.own->getInactiveRanges() : std::vector<std::pair<int, int>> {}
        );
    }

    doc->setSymbols(std::move(result.own), std::move(result.imported));

    // Push to the sidebar only when this document is the active one — the
    // tree always reflects the focused editor.
    if (doc == getActive()) {
        m_ctx.getSideBarManager().showSymbolsFor(doc);
    }
}

void DocumentManager::showEditorContextMenu(Editor& editor, const wxPoint& screenPos) {
    auto* doc = findByEditor(&editor);

    // Word under the click; a keyboard-invoked menu has no position, so the
    // caret position is used instead.
    int pos = editor.GetCurrentPos();
    if (screenPos != wxDefaultPosition) {
        pos = editor.PositionFromPoint(editor.ScreenToClient(screenPos));
    }
    const wxString word = editor.GetTextRange(editor.WordStartPosition(pos, true), editor.WordEndPosition(pos, true));

    std::optional<SymbolTable::Location> def;
    std::optional<SymbolTable::Location> decl;
    if (doc != nullptr && doc->getType() == DocumentType::FreeBASIC && !word.empty()) {
        if (const auto table = doc->getSymbolTable()) {
            def = table->findDefinition(word, doc->getImportedTables());
            decl = table->findDeclaration(word, doc->getImportedTables());
        }
    }

    wxMenu menu;
    menu.Append(+CommandId::Undo, m_ctx.tr("commands.undo.name"))->Enable(editor.CanUndo());
    menu.Append(+CommandId::Redo, m_ctx.tr("commands.redo.name"))->Enable(editor.CanRedo());
    menu.AppendSeparator();
    const bool hasSelection = editor.GetSelectionStart() != editor.GetSelectionEnd();
    menu.Append(+CommandId::Cut, m_ctx.tr("commands.cut.name"))->Enable(hasSelection);
    menu.Append(+CommandId::Copy, m_ctx.tr("commands.copy.name"))->Enable(hasSelection);
    menu.Append(+CommandId::Paste, m_ctx.tr("commands.paste.name"))->Enable(editor.CanPaste());
    menu.AppendSeparator();
    menu.Append(+CommandId::SelectAll, m_ctx.tr("commands.selectAll.name"));

    if (def.has_value() || decl.has_value()) {
        menu.AppendSeparator();
        menu.Append(kGoToDefinitionId, m_ctx.tr("editorContext.goToDefinition"))->Enable(def.has_value());
        menu.Append(kGoToDeclarationId, m_ctx.tr("editorContext.goToDeclaration"))->Enable(decl.has_value());
        if (def.has_value()) {
            menu.Bind(
                wxEVT_MENU,
                [this, loc = *def](const wxCommandEvent&) { goToLocation(loc.path, loc.line); },
                kGoToDefinitionId
            );
        }
        if (decl.has_value()) {
            menu.Bind(
                wxEVT_MENU,
                [this, loc = *decl](const wxCommandEvent&) { goToLocation(loc.path, loc.line); },
                kGoToDeclarationId
            );
        }
    }
    editor.PopupMenu(&menu);
}

void DocumentManager::goToLocation(const std::filesystem::path& path, const int line) {
    Document* target = path.empty() ? getActive() : openFile(path);
    if (target != nullptr) {
        // Symbol/Location lines are 0-based; navigateToLine expects 1-based.
        target->getEditor()->navigateToLine(line + 1);
    }
}

void DocumentManager::onIntellisenseTrackedFiles(wxThreadEvent& event) {
    if (m_watcher == nullptr) {
        return;
    }
    auto paths = event.GetPayload<std::vector<std::filesystem::path>>();
    // An include also open in a tab follows the open-document reload path; drop
    // it here so the closed-include watch never double-handles it.
    std::erase_if(paths, [this](const std::filesystem::path& path) { return findByPath(path) != nullptr; });
    m_watcher->setIncludeWatches(std::move(paths));
}

void DocumentManager::reparseInclude(const std::filesystem::path& path) {
    if (m_intellisense != nullptr) {
        m_intellisense->refreshFile(path);
    }
}

void DocumentManager::syncEditCommands() {
    auto& cmd = m_ctx.getCommandManager();
    const auto setForceDisabled = [&cmd](const CommandId id, const bool state) {
        if (auto* entry = cmd.find(+id)) {
            entry->setForceDisabled(state);
        }
    };

    auto* doc = getActive();
    const auto* editor = (doc != nullptr) ? doc->getEditor() : nullptr;
    if (editor == nullptr) {
        // No editor — leave broad `enabled` to applyState; clear our mask.
        setForceDisabled(CommandId::Undo, false);
        setForceDisabled(CommandId::Redo, false);
        setForceDisabled(CommandId::Cut, false);
        setForceDisabled(CommandId::Copy, false);
        setForceDisabled(CommandId::Paste, false);
        setForceDisabled(CommandId::SelectAll, false);
        return;
    }

    const bool hasSelection = editor->GetSelectionEnd() > editor->GetSelectionStart();
    const bool hasText = editor->GetTextLength() > 0;
    const bool readOnly = editor->GetReadOnly();

    setForceDisabled(CommandId::Undo, !editor->CanUndo());
    setForceDisabled(CommandId::Redo, !editor->CanRedo());
    setForceDisabled(CommandId::Cut, !(hasSelection && !readOnly));
    setForceDisabled(CommandId::Copy, !hasSelection);
    setForceDisabled(CommandId::Paste, !editor->CanPaste());
    setForceDisabled(CommandId::SelectAll, !hasText);
}

void DocumentManager::onTabRightDown(wxAuiNotebookEvent& event) {
    event.Skip();
    auto* notebook = getNotebook();
    const auto pageIdx = event.GetSelection();
    if (pageIdx == wxNOT_FOUND) {
        return;
    }
    const auto* page = notebook->GetPage(static_cast<size_t>(pageIdx));
    auto* doc = findByPage(page);
    if (doc == nullptr) {
        return;
    }

    // Activate the right-clicked tab so commands dispatched via CommandIds
    // act on it (Undo/Cut/etc target the active editor).
    notebook->SetSelection(static_cast<size_t>(pageIdx));
    syncEditCommands();

    auto& cmd = m_ctx.getCommandManager();
    const auto entryEnabled = [&cmd](const CommandId id) -> bool {
        const auto* entry = cmd.find(+id);
        return entry != nullptr && entry->isEnabled();
    };

    const bool hasOthers = m_documents.size() > 1;
    const bool hasPath = !doc->isNew();
    const auto path = doc->getFilePath();
    Document* docPtr = doc;

    std::error_code fsEc;
    const bool fileOnDisk = hasPath && std::filesystem::exists(path, fsEc);

    wxMenu menu;
    menu.Append(+CommandId::Close, m_ctx.tr("commands.close.name"));
    menu.Append(kTabCloseOthersId, m_ctx.tr("tabContext.closeOthers"))
        ->Enable(hasOthers);
    menu.AppendSeparator();
    menu.Append(kTabShowInBrowserId, m_ctx.tr("tabContext.showInBrowser"))
        ->Enable(hasPath);
    menu.Append(kTabReloadFromDiskId, m_ctx.tr("tabContext.reloadFromDisk"))
        ->Enable(fileOnDisk);
    menu.AppendSeparator();
    menu.Append(+CommandId::Undo, m_ctx.tr("commands.undo.name"))
        ->Enable(entryEnabled(CommandId::Undo));
    menu.Append(+CommandId::Redo, m_ctx.tr("commands.redo.name"))
        ->Enable(entryEnabled(CommandId::Redo));
    menu.AppendSeparator();
    menu.Append(+CommandId::Cut, m_ctx.tr("commands.cut.name"))
        ->Enable(entryEnabled(CommandId::Cut));
    menu.Append(+CommandId::Copy, m_ctx.tr("commands.copy.name"))
        ->Enable(entryEnabled(CommandId::Copy));
    menu.Append(+CommandId::Paste, m_ctx.tr("commands.paste.name"))
        ->Enable(entryEnabled(CommandId::Paste));
    menu.Append(+CommandId::SelectAll, m_ctx.tr("commands.selectAll.name"))
        ->Enable(entryEnabled(CommandId::SelectAll));

    menu.Bind(wxEVT_MENU, [this, docPtr](const wxCommandEvent&) { closeOtherFiles(*docPtr); }, kTabCloseOthersId);
    menu.Bind(wxEVT_MENU, [this, path](const wxCommandEvent&) {
        if (auto* entry = m_ctx.getCommandManager().find(+CommandId::Browser)) {
            entry->setChecked(true);
        }
        m_ctx.getSideBarManager().locateFile(toWxString(path)); }, kTabShowInBrowserId);
    menu.Bind(wxEVT_MENU, [this, docPtr](const wxCommandEvent&) {
        if (contains(docPtr)) {
            reloadFromDisk(*docPtr);
        } }, kTabReloadFromDiskId);

    notebook->PopupMenu(&menu);
}

// ---------------------------------------------------------------------------
// Life cycle
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Sessions
// ---------------------------------------------------------------------------

auto DocumentManager::startSession(const wxString& path) -> FileSession* {
    // Reopening the already-active session would drop the live one and re-read
    // it from disk — make it a no-op.
    if (m_session != nullptr && samePath(toFsPath(m_session->getPath()), toFsPath(path))) {
        return m_session.get();
    }
    m_session = std::make_unique<FileSession>(m_ctx, path);
    m_session->load();
    return m_session.get();
}

void DocumentManager::newSession() {
    const wxString path = promptSaveSessionPath(m_ctx);
    if (path.empty()) {
        return;
    }

    // close current session.
    m_session.reset();

    // We assume overwrite, so remove existing one.
    if (wxFileExists(path)) {
        wxRemoveFile(path);
    }

    startSession(path); // active from now; its file is written when it closes
    m_ctx.getFileHistory().addFile(path);
}

void DocumentManager::closeSession() {
    m_session.reset();
}

auto DocumentManager::prepareToQuit() -> bool {
    if (getModifiedCount() == 0) {
        m_session.reset();
        return true;
    }

    const auto result = wxMessageBox(
        m_ctx.tr("messages.saveChanges"),
        m_ctx.tr("messages.saveBeforeExit"),
        wxYES_NO | wxCANCEL | wxICON_EXCLAMATION,
        m_ctx.getUIManager().getMainFrame()
    );

    if (result == wxCANCEL) {
        return false;
    }

    if (result == wxYES) {
        if (!saveAllFiles()) {
            return false;
        }
    }

    // Snapshot + drop the session after any save-on-exit, while all documents
    // are still open, so it records their final paths (e.g. a previously new
    // document just saved under a real name).
    m_session.reset();

    // Discard all — close without prompting (already saved or user said NO)
    while (!m_documents.empty()) {
        auto& doc = *m_documents.back();
        doc.setModified(false);
        if (const auto idx = findPageIndex(doc); idx != wxNOT_FOUND) {
            getNotebook()->DeletePage(static_cast<size_t>(idx));
        }
        m_documents.pop_back();
    }

    m_ctx.getUIManager().setDocumentState(UIState::None);
    return true;
}

auto DocumentManager::getActive() const -> Document* {
    const auto* notebook = getNotebook();
    const auto sel = notebook->GetSelection();
    if (sel == wxNOT_FOUND) {
        return nullptr;
    }
    const auto* page = notebook->GetPage(static_cast<size_t>(sel));
    return findByPage(page);
}

void DocumentManager::setActive(Document* document) {
    if (!contains(document) || document == getActive()) {
        return;
    }
    const auto idx = findPageIndex(*document);
    if (idx != wxNOT_FOUND) {
        getNotebook()->SetSelection(static_cast<size_t>(idx));
    }
}

namespace {
// Lexical path comparison for matching open documents to a path that was just
// renamed or deleted in the IDE. std::filesystem::equivalent can't be used here:
// after the operation the document's current (old) path no longer exists.
auto normForCompare(const std::filesystem::path& path) -> wxString {
    wxString str = toWxString(path);
    while (str.length() > 1 && (str.Last() == '\\' || str.Last() == '/')) {
        str.RemoveLast();
    }
    return wxFileName::IsCaseSensitive() ? str : str.Lower();
}

auto samePath(const std::filesystem::path& lhs, const std::filesystem::path& rhs) -> bool {
    return normForCompare(lhs) == normForCompare(rhs);
}

auto underPath(const std::filesystem::path& child, const std::filesystem::path& parent) -> bool {
    // Strictly nested: child begins with "parent<sep>". normForCompare yields
    // native separators, so a single wxFILE_SEP_PATH check suffices.
    return normForCompare(child).StartsWith(normForCompare(parent) + wxFILE_SEP_PATH);
}
} // namespace

void DocumentManager::handleExternalRename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath) {
    const auto oldLen = normForCompare(oldPath).length();
    for (const auto& docPtr : m_documents) {
        auto& doc = *docPtr;
        if (doc.isNew()) {
            continue;
        }
        std::filesystem::path updated;
        if (samePath(doc.getFilePath(), oldPath)) {
            updated = newPath;
        } else if (underPath(doc.getFilePath(), oldPath)) {
            // Directory rename: keep the document below the new folder, preserving
            // the remainder of its path (with its original case).
            const wxString remainder = toWxString(doc.getFilePath()).Mid(oldLen);
            updated = toFsPath(toWxString(newPath) + remainder);
        } else {
            continue;
        }
        // Re-point the watcher around the path change (mirrors saveFileAs).
        m_watcher->removeDocument(doc);
        doc.setFilePath(updated);
        doc.updateModTime();
        doc.dismissExternalNotification();
        m_watcher->addDocument(doc);
        updateTabTitle(doc);
    }
    updateActiveTabTitle(); // the active document may have moved
}

void DocumentManager::handleExternalDelete(const std::filesystem::path& path) {
    // Collect first — closeFile mutates m_documents.
    std::vector<Document*> victims;
    for (const auto& docPtr : m_documents) {
        auto& doc = *docPtr;
        if (!doc.isNew() && (samePath(doc.getFilePath(), path) || underPath(doc.getFilePath(), path))) {
            victims.push_back(&doc);
        }
    }
    for (auto* doc : victims) {
        doc->setModified(false); // the file is gone; close without prompting to save
        closeFile(*doc);
    }
}

auto DocumentManager::findByPath(const wxString& path) const -> Document* {
    return findByPath(toFsPath(path));
}

auto DocumentManager::findByPath(const std::filesystem::path& path) const -> Document* {
    // File identity is on-disk sameness, NOT string equality: case-insensitive
    // filesystems (macOS/Windows), symlinks, 8.3 short names, and relative paths
    // all alias the same file. std::filesystem::equivalent answers that portably
    // — `==` misses it, and weakly_canonical's case-folding is implementation-
    // defined (works under MSVC, not MinGW/libc++).
    if (path.empty()) {
        return nullptr;
    }
    std::error_code ec;
    for (auto& doc : m_documents) {
        if (doc->isNew()) {
            continue;
        }
        // equivalent needs both paths to exist; a missing query matches nothing
        // (ec is set, the call returns false), which is the correct answer.
        if (std::filesystem::equivalent(doc->getFilePath(), path, ec)) {
            return doc.get();
        }
    }
    return nullptr;
}

auto DocumentManager::getModifiedCount() const -> size_t {
    return static_cast<size_t>(std::ranges::count_if(m_documents, [](const auto& doc) {
        return doc->isModified();
    }));
}

auto DocumentManager::findByEditor(const wxWindow* editor) const -> Document* {
    for (auto& doc : m_documents) {
        if (doc->getEditor() == editor) {
            return doc.get();
        }
    }
    return nullptr;
}

auto DocumentManager::findByPage(const wxWindow* page) const -> Document* {
    for (auto& doc : m_documents) {
        if (doc->getPage() == page) {
            return doc.get();
        }
    }
    return nullptr;
}

void DocumentManager::setMinimapVisible(const bool visible) {
    for (const auto& doc : m_documents) {
        doc->showMinimap(visible);
    }
}

auto DocumentManager::contains(const Document* doc) const -> bool {
    return doc != nullptr && std::ranges::contains(m_documents, doc, &std::unique_ptr<Document>::get);
}

auto DocumentManager::findPageIndex(const Document& doc) const -> int {
    const auto* notebook = m_ctx.getUIManager().getNotebook();
    for (size_t idx = 0; idx < notebook->GetPageCount(); idx++) {
        if (notebook->GetPage(idx) == doc.getPage()) {
            return static_cast<int>(idx);
        }
    }
    return wxNOT_FOUND;
}

auto DocumentManager::getNotebook() const -> wxAuiNotebook* {
    return m_ctx.getUIManager().getNotebook();
}

void DocumentManager::updateActiveTabTitle() const {
    if (const auto* doc = getActive()) {
        updateTabTitle(*doc);
    }
}

void DocumentManager::updateTabTitle(const Document& doc) const {
    const auto idx = findPageIndex(doc);
    if (idx != wxNOT_FOUND) {
        getNotebook()->SetPageText(static_cast<size_t>(idx), doc.getTitle());
    }
}

void DocumentManager::refreshAutoReload() {
    m_watcher->applyConfig();
    // Re-enabling the watcher dropped its include watches; ask the worker to
    // re-post its tracked set so they rebuild without waiting for an edit.
    if (m_watcher->isEnabled() && m_intellisense != nullptr) {
        m_intellisense->resendTrackedFiles();
    }
}

void DocumentManager::shutdownWatcher() {
    if (m_watcher != nullptr) {
        m_watcher->shutdown();
    }
}

void DocumentManager::flushExternalPending(Document& doc) {
    m_watcher->flushPending(doc);
}

void DocumentManager::refreshTabTitle(const Document& doc) const {
    if (const auto idx = findPageIndex(doc); idx != wxNOT_FOUND) {
        getNotebook()->SetPageText(static_cast<size_t>(idx), doc.getTitle());
    }
}

// ---------------------------------------------------------------------------
// Find / Replace
// ---------------------------------------------------------------------------

void DocumentManager::showFind() {
    showFindDialog(false);
}

void DocumentManager::showReplace() {
    showFindDialog(true);
}

void DocumentManager::findNext() {
    auto* doc = getActive();
    if (!doc) {
        return;
    }
    const auto text = m_findData.GetFindString();
    if (text.empty()) {
        showFindDialog(false);
        return;
    }
    const bool forward = (m_findData.GetFlags() & wxFR_DOWN) != 0;
    doc->getEditor()->findNext(text, m_findData.GetFlags(), forward);
}

void DocumentManager::gotoLine() {
    auto* doc = getActive();
    if (!doc) {
        return;
    }
    const auto input = wxGetTextFromUser(
        m_ctx.tr("dialogs.goto.prompt"),
        m_ctx.tr("dialogs.goto.title"),
        "",
        m_ctx.getUIManager().getMainFrame()
    );
    if (!input.empty()) {
        doc->getEditor()->gotoLine(input);
    }
}

void DocumentManager::showFindDialog(const bool replace) {
    auto* doc = getActive();
    if (!doc) {
        return;
    }

    // Single instance: raise the open dialog instead of stacking another.
    // Find and Replace share the one slot — close it to switch modes.
    if (m_findDialog != nullptr) {
        m_findDialog->Raise();
        m_findDialog->SetFocus();
        return;
    }

    // Pre-fill with selection or word under cursor
    if (const auto word = doc->getEditor()->getWordAtCursor(); !word.empty()) {
        m_findData.SetFindString(word);
    }

    auto* frame = m_ctx.getUIManager().getMainFrame();
    const int style = replace ? wxFR_REPLACEDIALOG : 0;
    const auto title = replace ? m_ctx.tr("dialogs.replace.title") : m_ctx.tr("dialogs.find.title");

    m_findDialog = make_unowned<wxFindReplaceDialog>(frame, &m_findData, title, style);
    m_findDialog->PushEventHandler(this);
    m_findDialog->Show();
}

void DocumentManager::onFindDialog(wxFindDialogEvent& event) {
    auto* doc = getActive();
    if (!doc) {
        return;
    }
    const bool forward = (event.GetFlags() & wxFR_DOWN) != 0;
    doc->getEditor()->findNext(event.GetFindString(), event.GetFlags(), forward);
}

void DocumentManager::onFindDialogNext(wxFindDialogEvent& event) {
    onFindDialog(event);
}

void DocumentManager::onReplaceDialog(wxFindDialogEvent& event) {
    auto* doc = getActive();
    if (!doc) {
        return;
    }
    doc->getEditor()->replaceNext(event.GetFindString(), event.GetReplaceString(), event.GetFlags());
}

void DocumentManager::onReplaceAllDialog(wxFindDialogEvent& event) {
    auto* doc = getActive();
    if (!doc) {
        return;
    }
    doc->getEditor()->replaceAll(event.GetFindString(), event.GetReplaceString(), event.GetFlags());
}

void DocumentManager::onFindDialogClose(wxFindDialogEvent& event) {
    if (auto* dlg = event.GetDialog()) {
        dlg->PopEventHandler();
        dlg->Destroy();
    }
    m_findDialog = nullptr;
}
