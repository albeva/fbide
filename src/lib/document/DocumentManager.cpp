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
#include "FileSession.hpp"
#include "analyses/intellisense/IntellisenseService.hpp"
#include "app/App.hpp"
#include "app/Context.hpp"
#include "command/CommandManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "editor/CodeTransformer.hpp"
#include "editor/Editor.hpp"
#include "editor/EditorPanel.hpp"
#include "sidebar/SideBarManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {
constexpr auto SESSION_EXT = "fbs";
} // namespace

DocumentManager::DocumentManager(Context& ctx)
: m_ctx(ctx)
, m_codeTransformer(std::make_unique<CodeTransformer>(ctx.getConfigManager()))
, m_intellisense(std::make_unique<IntellisenseService>(ctx, this)) {
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
    const auto thaw = m_ctx.getUIManager().freeze();
    auto& doc = *m_documents.emplace_back(std::make_unique<Document>(m_ctx, type, this));
    make_unowned<EditorPanel>(m_notebook.get(), m_ctx, type, doc);
    m_notebook->addPage(doc);
    return doc;
}

void DocumentManager::openFile() {
    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("files.loadTitle"),
        "",
        ".bas",
        m_ctx.getConfigManager().filePatterns({ "freebasic", "properties", "markdown", "batch", "bash", "makefile", "json", "css", "all" }),
        wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE
    );

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    wxArrayString paths;
    dlg.GetPaths(paths);
    for (const auto& path : paths) {
        openFile(toFsPath(path));
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

    // 1. Relative to source file
    if (!origin.isNew()) {
        if (auto* doc = tryOpen(origin.getFilePath().parent_path() / req)) {
            return doc;
        }
    }

    // 2. Compiler `inc/` folder: <dir-of-fbc>/inc/<path>
    const auto compilerPathStr = m_ctx.getConfigManager().config().get_or("compiler.path", "");
    if (!compilerPathStr.empty()) {
        auto fbc = toFsPath(compilerPathStr);
        if (fbc.is_relative()) {
            fbc = m_ctx.getConfigManager().getAppDir() / fbc;
        }
        if (auto* doc = tryOpen(fbc.parent_path() / "inc" / req)) {
            return doc;
        }
    }

    // 3. Current working directory
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

auto DocumentManager::openFile(const std::filesystem::path& filePath) -> Document* {
    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec)) {
        return nullptr;
    }

    // Canonicalize once at entry — fixes duplicate-tab bug on case-insensitive
    // filesystems (macOS/Windows: `fbgfx.bi` vs `FBGFX.bi`), resolves symlinks,
    // and ensures the stored path is identity-comparable for findByPath.
    const auto canonical = canonicalizePath(filePath);

    // Session files are loaded separately
    if (const auto ext = canonical.extension().string(); ext.size() > 1 && ext.substr(1) == SESSION_EXT) {
        m_ctx.getFileSession().load(canonical);
        return nullptr;
    }

    // Check if already open
    if (auto* existing = findByPath(canonical)) {
        m_notebook->selectDocument(*existing);
        return existing;
    }

    // Read file first — if it fails, don't create an Editor (dangling
    // child of the notebook). Config-derived defaults seed detection.
    const auto loaded = DocumentIO::load(canonical, defaultEncoding(), defaultEolMode());
    if (!loaded.has_value()) {
        wxLogError(m_ctx.tr("messages.loadFailed"), toWxString(canonical));
        return nullptr;
    }

    const auto thaw = m_ctx.getUIManager().freeze();
    const auto type = documentTypeFromPath(canonical);

    auto& doc = *m_documents.emplace_back(std::make_unique<Document>(m_ctx, type, this));
    make_unowned<EditorPanel>(m_notebook.get(), m_ctx, type, doc);

    loadFile(doc, loaded->text, loaded->eolMode);
    doc.setEncoding(loaded->encoding);
    doc.setEolMode(loaded->eolMode);
    doc.setFilePath(canonical);
    doc.markSaved();

    m_notebook->addPage(doc);

    m_ctx.getFileHistory().addFile(canonical);

    // Initial parse: bypass throttle, submit immediately.
    submitIntellisense(&doc, loaded->text);
    return &doc;
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

    doc.setFilePath(newPath);
    doc.markSaved();
    doc.updateModTime();
    refreshTitleFor(doc);
    promptRestartIfConfig(newPath);

    if (clash != nullptr && clash != &doc) {
        // Two tabs showing the same file is redundant. The user already
        // confirmed the overwrite, so close the mirror tab without
        // re-prompting about its now-stale buffer.
        clash->markSaved();
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

    m_notebook->removePage(doc);
    std::erase_if(m_documents, [&doc](const auto& ptr) { return ptr.get() == &doc; });

    // Sidebar: reflect post-close state. PAGE_CHANGED isn't always fired
    // by AUI when the active page is removed, so push explicitly here.
    // showSymbolsFor handles nullptr (clears) and dedups by shared_ptr.
    m_ctx.getSideBarManager().showSymbolsFor(getActive());

    // Trim the IntellisenseService SymbolTable pool: the closed doc's
    // shared_ptr just released, so any pool slot it held is now idle.
    m_intellisense->prune();

    // Update UI state when no documents remain
    if (m_documents.empty()) {
        auto& ui = m_ctx.getUIManager();
        ui.setDocumentState(UIState::None);
        ui.setTitle(wxEmptyString);
        ui.clearDocumentStatus();
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

void DocumentManager::onDocumentTypeChanged(DocumentTypeChangedEvent& event) {
    auto& doc = *event.getDocument();
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
    m_intellisense->submit(doc, content);
}

void DocumentManager::cancelIntellisense(const Document* doc) {
    m_intellisense->cancel(doc);
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

    // Discard all — close without prompting (already saved or user said NO)
    while (!m_documents.empty()) {
        auto& doc = *m_documents.back();
        doc.markSaved();
        m_notebook->removePage(doc);
        m_documents.pop_back();
    }

    m_ctx.getUIManager().setDocumentState(UIState::None);
    return true;
}

auto DocumentManager::getActive() const -> Document* {
    return m_notebook->activeDocument();
}

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
    // Stored doc paths are canonical (set via openFile / saveFileAs).
    // Canonicalize the query once so case-insensitive filesystems, symlinks,
    // and relative paths all collapse to the same identity.
    const auto canonical = canonicalizePath(path);
    for (auto& doc : m_documents) {
        if (!doc->isNew() && doc->getFilePath() == canonical) {
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

void DocumentManager::setMinimapVisible(const bool visible) {
    for (const auto& doc : m_documents) {
        doc->showMinimap(visible);
    }
}

auto DocumentManager::contains(const Document* doc) const -> bool {
    return doc != nullptr && std::ranges::contains(m_documents, doc, &std::unique_ptr<Document>::get);
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
