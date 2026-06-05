//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentManager.hpp"
#include "Document.hpp"
#include "DocumentIO.hpp"
#include "DocumentNotebook.hpp"
#include "DocumentPath.hpp"
#include "analyses/intellisense/IntellisenseService.hpp"
#include "app/App.hpp"
#include "app/Context.hpp"
#include "command/CommandManager.hpp"
#include "compiler/CompileCommand.hpp"
#include "compiler/CompilerConfigCatalog.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "editor/CodeTransformer.hpp"
#include "editor/Editor.hpp"
#include "editor/EditorPanel.hpp"
#include "sidebar/SideBarManager.hpp"
#include "ui/UIManager.hpp"
#include "workspace/WorkspaceManager.hpp"
using namespace fbide;

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
DocumentManager::DocumentManager(Context& ctx)
: m_ctx(ctx)
, m_codeTransformer(std::make_unique<CodeTransformer>(ctx.getConfigManager())) {
    Bind(EVT_INTELLISENSE_RESULT, &DocumentManager::onIntellisenseResult, this);
    Bind(EVT_DOCUMENT_TYPE_CHANGED, &DocumentManager::onDocumentTypeChanged, this);
}

DocumentManager::~DocumentManager() = default;

auto DocumentManager::createNotebook(wxWindow* parent) -> DocumentNotebook& {
    m_notebook = make_unowned<DocumentNotebook>(parent, m_ctx);
    return *m_notebook;
}

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
    // The shared ephemeral project owns the document; we create its editor.
    auto doc = std::make_unique<Document>(m_ctx.getConfigManager(), type, nullptr);
    auto* ptr = m_ctx.getWorkspaceManager().adoptStandalone(std::move(doc));
    openEditorFor(*ptr);
    return *ptr;
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
        return openDocument(full);
    };

    const auto req = toFsPath(includePath);
    if (req.is_absolute()) {
        return tryOpen(req);
    }

    // The origin document's active compiler configuration supplies both
    // the -i search dirs (its compile command) and the stock inc/ folder
    // (its fbc binary) — the same compiler a build of this document uses.
    const auto* originProject = origin.getProject();
    const auto& cfg = originProject != nullptr
                        ? originProject->getCompilerConfig()
                        : m_ctx.getCompilerManager().catalog().resolveByPinnedSlug(std::nullopt);

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
    for (const auto& entry : CompileCommand::extractIncludePaths(cfg.compileCommand)) {
        auto dir = toFsPath(entry);
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
            fbc = m_ctx.getConfigManager().getAppDir() / fbc;
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

auto DocumentManager::openDocument(const std::filesystem::path& filePath) -> Document* {
    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec)) {
        return nullptr;
    }

    // Canonicalize once at entry — fixes duplicate-tab bug on case-insensitive
    // filesystems (macOS/Windows: `fbgfx.bi` vs `FBGFX.bi`), resolves symlinks,
    // and ensures the stored path is identity-comparable for findByPath.
    const auto canonical = canonicalizePath(filePath);

    // Already known (open tab, or a member of the open persistent project) —
    // focus it, creating its editor if needed.
    if (auto* existing = findByPath(canonical)) {
        openEditorFor(*existing);
        return existing;
    }

    // Verify it reads before creating anything — no empty tab on failure.
    if (!DocumentIO::load(canonical, defaultEncoding(), defaultEolMode()).has_value()) {
        wxLogError(m_ctx.tr("messages.loadFailed"), toWxString(canonical));
        return nullptr;
    }

    // Standalone document, owned by the shared ephemeral project.
    auto doc = std::make_unique<Document>(m_ctx.getConfigManager(), documentTypeFromPath(canonical), nullptr);
    doc->setFilePath(canonical);
    auto* ptr = m_ctx.getWorkspaceManager().adoptStandalone(std::move(doc));
    m_ctx.getFileHistory().addFile(canonical);
    openEditorFor(*ptr);
    return ptr;
}

void DocumentManager::openEditorFor(Document& doc) {
    // Already open — just focus its tab.
    if (doc.hasView()) {
        m_notebook->selectDocument(doc);
        return;
    }

    const auto thaw = m_ctx.getUIManager().freeze();
    make_unowned<EditorPanel>(m_notebook.get(), m_ctx, doc.getType(), doc); // attaches the editor view
    doc.setSink(this);

    // Populate from disk for a saved document; untitled documents start empty.
    wxString content;
    if (!doc.isNew()) {
        if (const auto loaded = DocumentIO::load(doc.getFilePath(), defaultEncoding(), defaultEolMode())) {
            loadFile(doc, loaded->text, loaded->eolMode);
            doc.setEncoding(loaded->encoding);
            doc.setEolMode(loaded->eolMode);
            doc.markSaved();
            doc.updateModTime();
            content = loaded->text;
        } else {
            wxLogError(m_ctx.tr("messages.loadFailed"), toWxString(doc.getFilePath()));
        }
    }

    m_notebook->addPage(doc, true);
    // Restore any saved per-document session state (project members only;
    // a no-op for standalone documents).
    m_ctx.getWorkspaceManager().applyDocumentSession(doc);
    if (doc.getType() == DocumentType::FreeBASIC) {
        submitIntellisense(&doc, content);
    }
}

void DocumentManager::closeEditor(Document& doc) {
    m_notebook->removePage(doc); // destroys the EditorPanel → doc.detachView()
    doc.setSink(nullptr);        // an editor-less document fires no type-change events
}

void DocumentManager::reportSaveFailure(const DocumentIO::SaveResult result, const TextEncoding encoding) const {
    if (result == DocumentIO::SaveResult::EncodingError) {
        wxLogError(m_ctx.tr("messages.saveEncodingError"), encoding.toString().data());
    } else if (result == DocumentIO::SaveResult::IOError) {
        wxLogError("%s", m_ctx.tr("messages.saveIoError"));
    }
}

void DocumentManager::loadFile(Document& doc, const wxString& text, const EolMode eol) const {
    auto* editor = doc.getEditor();
    const auto stcEol = eol.toStc();
    editor->disableTransforms(true);
    editor->SetText(text);
    editor->disableTransforms(false);
    editor->SetEOLMode(stcEol);
    editor->ConvertEOLs(stcEol);
    editor->EmptyUndoBuffer();
}

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

    loadFile(doc, loaded->text, loaded->eolMode);
    doc.setEncoding(encoding);
    doc.setEolMode(loaded->eolMode);
    doc.markSaved();
    doc.updateModTime();
    refreshTitleFor(doc);
    doc.getEditor()->updateStatusBar();
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

    const auto result = DocumentIO::save(doc.getFilePath(), doc.getEditor()->GetText(), doc.getEncoding(), doc.getEolMode());
    if (result != DocumentIO::SaveResult::Success) {
        reportSaveFailure(result, doc.getEncoding());
        return false;
    }

    doc.markSaved();
    doc.updateModTime();
    refreshTitleFor(doc);
    promptRestartIfConfig(doc.getFilePath());
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

    if (dlg.ShowModal() != wxID_OK) {
        return false;
    }

    const auto newPath = canonicalizePath(toFsPath(dlg.GetPath()));

    // Persistent projects keep all members under their root — Save As
    // to outside the project folder is a user error. Ephemeral projects
    // accept any path (their root follows the file).
    if (const auto* project = doc.getProject(); project != nullptr && !project->isUnderRoot(newPath)) {
        wxMessageBox(
            m_ctx.tr("messages.saveOutOfTreeMessage"),
            m_ctx.tr("messages.saveOutOfTreeTitle"),
            wxOK | wxICON_EXCLAMATION,
            m_ctx.getUIManager().getMainFrame()
        );
        return false;
    }

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

    const auto result = DocumentIO::save(newPath, doc.getEditor()->GetText(), doc.getEncoding(), doc.getEolMode());
    if (result != DocumentIO::SaveResult::Success) {
        reportSaveFailure(result, doc.getEncoding());
        return false;
    }

    // Close any clashing tab BEFORE retargeting `doc`. For Persistent
    // projects, that doc's project node still owns `newPath` in the
    // project's path index; if we set our doc's path first,
    // ProjectBase::setFilePath would collide. Closing it first releases
    // the index entry (or, for Ephemeral, tears the whole project
    // down) so the retarget is unambiguous.
    if (clash != nullptr && clash != &doc) {
        clash->markSaved();
        closeFile(*clash);
    }

    doc.setFilePath(newPath);
    doc.markSaved();
    doc.updateModTime();
    refreshTitleFor(doc);
    promptRestartIfConfig(newPath);
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

    // Reload using the document's currently-active encoding + EOL — the
    // user already chose them (or they were detected at first load), so a
    // re-detection on reload would reset their choice unexpectedly.
    const auto loaded = DocumentIO::loadWithEncoding(doc.getFilePath(), doc.getEncoding(), doc.getEolMode());
    if (!loaded.has_value()) {
        wxLogError("%s", m_ctx.tr("messages.reloadFailed"));
        return;
    }

    // Keep the document's existing EOL — convert the loaded text to match
    // it so the editor stays in the user-chosen line-ending mode (loaded
    // EOL is the detected one, which may differ).
    loadFile(doc, loaded->text, doc.getEolMode());
    doc.markSaved();
    doc.updateModTime();
    refreshTitleFor(doc);
    doc.getEditor()->updateStatusBar();

    submitIntellisense(&doc, loaded->text);
}

void DocumentManager::promptRestartIfConfig(const std::filesystem::path& path) const {
    // Saving an in-place edit of one of FBIde's own config files (theme,
    // shortcuts, keywords, etc.) only takes effect on the next launch —
    // hot-reloading was unreliable. Offer a restart, mirroring the
    // language-change flow in GeneralPage.
    if (!m_ctx.getConfigManager().isKnownConfig(path)) {
        return;
    }
    const auto answer = wxMessageBox(
        m_ctx.tr("messages.configRestartMessage"),
        m_ctx.tr("messages.configRestartTitle"),
        wxYES_NO | wxICON_QUESTION,
        m_ctx.getUIManager().getMainFrame()
    );
    if (answer == wxYES) {
        m_ctx.getApp().scheduleRestart();
    }
}

auto DocumentManager::saveAllFiles() -> bool {
    for (auto* doc : m_ctx.getWorkspaceManager().documents()) {
        if (doc->isModified()) {
            if (!saveFile(*doc)) {
                return false;
            }
        }
    }
    return true;
}

auto DocumentManager::closeFile(Document& doc) -> bool {
    cancelIntellisense(&doc);

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

    // Capture the document's session state before its editor is torn down —
    // project members only, so their scroll/cursor/folds survive to next open.
    m_ctx.getWorkspaceManager().captureDocumentSession(doc);

    // Tear down the editor/tab. A standalone document dies with its tab; a
    // persistent project member survives (editor-less) under its node.
    auto* project = doc.getProject();
    closeEditor(doc);
    if (project != nullptr && project->isEphemeral()) {
        m_ctx.getWorkspaceManager().closeStandalone(&doc); // destroys the document
    }

    // Reflect the post-close active document — PAGE_CHANGED isn't always fired
    // by AUI when the active page is removed, so push explicitly. Refresh the
    // workspace's ephemeral build context first (it drives the capabilities /
    // config the consumers below read); showSymbolsFor handles nullptr (clears);
    // onActiveDocumentChanged avoids a dangling cached pointer in the compiler
    // manager.
    m_ctx.getWorkspaceManager().onActiveDocumentChanged(getActive());
    m_ctx.getSideBarManager().showSymbolsFor(getActive());
    m_ctx.getCompilerManager().onActiveDocumentChanged(getActive());

    // Trim the IntellisenseService SymbolTable pool — the closed doc's slot is
    // now idle.
    m_ctx.getWorkspaceManager().getIntellisense().prune();

    // Update UI state when no tabs remain.
    if (getCount() == 0) {
        auto& ui = m_ctx.getUIManager();
        ui.syncDocCommands();
        ui.syncBuildCommands();
        ui.setTitle(wxEmptyString);
        ui.clearDocumentStatus();
    }

    return true;
}

auto DocumentManager::closeAllFiles() -> bool {
    // Snapshot the open tabs first — closeFile mutates ownership + notebook.
    for (auto* doc : openDocuments()) {
        if (!closeFile(*doc)) {
            return false;
        }
    }
    return true;
}

auto DocumentManager::closeOtherFiles(const Document& keep) -> bool {
    // Snapshot first — closeFile mutates ownership + notebook.
    for (auto* doc : openDocuments()) {
        if (doc != &keep && !closeFile(*doc)) {
            return false;
        }
    }
    return true;
}

void DocumentManager::onDocumentTypeChanged(DocumentTypeChangedEvent& event) {
    auto& doc = *event.getDocument();

    // The shared ephemeral project's capabilities follow the active document's
    // type, so a flip just needs a build-command + dropdown refresh — there is
    // no per-document project to create or tear down. Re-point the ephemeral's
    // build context first so syncBuildCommands sees the new capabilities.
    m_ctx.getWorkspaceManager().onActiveDocumentChanged(getActive());
    m_ctx.getUIManager().syncBuildCommands();
    // The configuration dropdown tracks that same project, and a type
    // flip fires no page-change event, so re-sync it explicitly.
    m_ctx.getCompilerManager().onActiveDocumentChanged(getActive());

    // The type override is project data — persist it to the `.fbp` (project
    // members only; standalone files store it in their `.fbs` session).
    m_ctx.getWorkspaceManager().persistProjectFile(doc);

    if (doc.getType() == DocumentType::FreeBASIC) {
        // Re-enter the FreeBASIC pipeline — submit the current buffer
        // for intellisense so the symbol browser populates. View-less
        // documents have no buffer; skip until a view attaches.
        if (const auto* editor = doc.getEditor(); editor != nullptr) {
            submitIntellisense(&doc, editor->GetText());
        }
        return;
    }
    // Leaving FreeBASIC: drop any in-flight intellisense work, release
    // the symbol table (frees the shared_ptr — workers may still hold
    // a reference until they finish, which is fine), and clear the
    // sub/function browser if this is the active document.
    cancelIntellisense(&doc);
    if (auto* editor = doc.getEditor()) {
        editor->setSymbolTable(nullptr);
    }
    if (getActive() == &doc) {
        m_ctx.getSideBarManager().showSymbolsFor(nullptr);
    }
}

void DocumentManager::submitIntellisense(Document* doc, const wxString& content) {
    m_ctx.getWorkspaceManager().getIntellisense().submit(doc, content);
}

void DocumentManager::cancelIntellisense(const Document* doc) {
    m_ctx.getWorkspaceManager().getIntellisense().cancel(doc);
}

void DocumentManager::onIntellisenseResult(wxThreadEvent& event) {
    const auto result = event.GetPayload<IntellisenseResult>();
    // Validate the document is still alive — race against close.
    if (!contains(result.owner)) {
        return;
    }
    if (auto* editor = result.owner->getEditor()) {
        editor->setSymbolTable(result.symbols);
    }

    // Push to the sidebar only when this document is the active one — the
    // tree always reflects the focused editor.
    if (result.owner == getActive()) {
        m_ctx.getSideBarManager().showSymbolsFor(result.owner);
    }
}

void DocumentManager::syncEditCommands() {
    // Thin forwarder — `CommandManager::syncEditCommands(doc)` owns
    // the actual mask logic and the list of edit `CommandId`s. This
    // entry point exists so callers that already hold a
    // DocumentManager reference (Editor, UIManager, DocumentNotebook)
    // don't have to chain through Context for the common
    // "sync against the active doc" case.
    m_ctx.getCommandManager().syncEditCommands(getActive());
}

// ---------------------------------------------------------------------------
// Life cycle
// ---------------------------------------------------------------------------

auto DocumentManager::prepareToQuit() -> bool {
    if (getModifiedCount() == 0) {
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

    // Discard all — close every open tab without prompting (already saved, or
    // the user chose not to). Documents are owned by their projects and are
    // released at Context teardown.
    for (auto* doc : openDocuments()) {
        doc->markSaved();
        closeEditor(*doc);
    }

    auto& ui = m_ctx.getUIManager();
    ui.syncDocCommands();
    ui.syncBuildCommands();
    return true;
}

auto DocumentManager::getActive() const -> Document* {
    return m_notebook->activeDocument();
}

auto DocumentManager::getCount() const -> size_t {
    return m_notebook != nullptr ? static_cast<size_t>(m_notebook->GetPageCount()) : 0;
}

auto DocumentManager::documentsInTabOrder() const -> std::vector<Document*> {
    std::vector<Document*> result;
    if (m_notebook == nullptr) {
        return result;
    }
    const auto count = m_notebook->GetPageCount();
    result.reserve(count);
    for (size_t idx = 0; idx < count; idx++) {
        if (auto* doc = m_notebook->documentForPage(m_notebook->GetPage(idx))) {
            result.push_back(doc);
        }
    }
    return result;
}

auto DocumentManager::openDocuments() const -> std::vector<Document*> {
    std::vector<Document*> result;
    for (auto* doc : m_ctx.getWorkspaceManager().documents()) {
        if (doc->hasView()) {
            result.push_back(doc);
        }
    }
    return result;
}

// Semantically mutating (changes the active tab) even though
// notebook access goes through Unowned<>'s pointer operators.
// NOLINTNEXTLINE(readability-make-member-function-const)
void DocumentManager::setActive(Document* document) {
    if (!contains(document) || document == getActive()) {
        return;
    }
    m_notebook->selectDocument(*document);
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
    for (auto* doc : m_ctx.getWorkspaceManager().documents()) {
        if (doc->isNew()) {
            continue;
        }
        // equivalent needs both paths to exist; a missing query matches nothing
        // (ec is set, the call returns false), which is the correct answer.
        if (std::filesystem::equivalent(doc->getFilePath(), path, ec)) {
            return doc;
        }
    }
    return nullptr;
}

auto DocumentManager::getModifiedCount() const -> size_t {
    const auto docs = m_ctx.getWorkspaceManager().documents();
    return static_cast<size_t>(std::ranges::count_if(docs, [](const Document* doc) {
        return doc->isModified();
    }));
}

auto DocumentManager::findByEditor(const wxWindow* editor) const -> Document* {
    for (auto* doc : openDocuments()) {
        if (doc->getEditor() == editor) {
            return doc;
        }
    }
    return nullptr;
}

void DocumentManager::setMinimapVisible(const bool visible) {
    for (auto* doc : openDocuments()) {
        doc->showMinimap(visible);
    }
}

auto DocumentManager::contains(const Document* doc) const -> bool {
    if (doc == nullptr) {
        return false;
    }
    const auto docs = m_ctx.getWorkspaceManager().documents();
    return std::ranges::find(docs, doc) != docs.end();
}

void DocumentManager::updateActiveTabTitle() const {
    if (const auto* doc = getActive()) {
        refreshTitleFor(*doc);
    }
}

void DocumentManager::refreshTitleFor(const Document& doc) const {
    // Tab text always reflects the doc; frame title only when the doc
    // is the focused tab. Without the active-only guard, saveAllFiles
    // walks the whole modified set and leaves the frame title pointing
    // at whichever doc was saved last rather than the visible one.
    m_notebook->updateTitle(doc);
    if (&doc == getActive()) {
        m_ctx.getUIManager().setTitle(doc.getFrameTitle());
    }
}
