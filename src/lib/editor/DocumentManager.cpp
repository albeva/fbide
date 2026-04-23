//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentManager.hpp"
#include "Document.hpp"
#include "DocumentIO.hpp"
#include "Editor.hpp"
#include "FileSession.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {
constexpr auto SESSION_EXT = "fbs";
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
: m_ctx(ctx) {}

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
// File andling
// ---------------------------------------------------------------------------

auto DocumentManager::newFile(DocumentType type) -> Document& {
    const auto thaw = m_ctx.getUIManager().freeze();
    auto* notebook = getNotebook();
    auto& doc = *m_documents.emplace_back(std::make_unique<Document>(notebook, m_ctx, type));
    notebook->AddPage(doc.getEditor(), doc.getTitle(), true);
    return doc;
}

void DocumentManager::openFile() {
    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("files.loadTitle"),
        "",
        ".bas",
        m_ctx.getConfigManager().filePatterns({ "freebasic", "properties", "all" }),
        wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE
    );

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    wxArrayString paths;
    dlg.GetPaths(paths);
    for (const auto& path : paths) {
        openFile(path);
    }
}

auto DocumentManager::openFile(const wxString& filePath) -> Document* {
    if (not wxFileExists(filePath)) {
        return nullptr;
    }

    // Session files are loaded separately
    if (wxFileName(filePath).GetExt() == SESSION_EXT) {
        m_ctx.getFileSession().load(filePath);
        return nullptr;
    }

    // Check if already open
    if (auto* existing = findByPath(filePath)) {
        const auto idx = findPageIndex(*existing);
        if (idx != wxNOT_FOUND) {
            getNotebook()->SetSelection(static_cast<size_t>(idx));
        }
        return existing;
    }

    // Read file first — if it fails, don't create an Editor (dangling
    // child of the notebook). Config-derived defaults seed detection.
    const auto loaded = DocumentIO::load(filePath, defaultEncoding(), defaultEolMode());
    if (!loaded.has_value()) {
        wxLogError(m_ctx.tr("messages.loadFailed"), filePath);
        return nullptr;
    }

    const auto thaw = m_ctx.getUIManager().freeze();
    const auto type = documentTypeFromPath(filePath);
    auto& doc = *m_documents.emplace_back(std::make_unique<Document>(getNotebook(), m_ctx, type));

    auto* editor = doc.getEditor();
    editor->SetText(loaded->text);
    editor->SetEOLMode(loaded->eolMode.toStc());
    editor->ConvertEOLs(loaded->eolMode.toStc());
    editor->EmptyUndoBuffer();
    doc.setEncoding(loaded->encoding);
    doc.setEolMode(loaded->eolMode);
    doc.setFilePath(filePath);
    doc.setModified(false);

    auto* notebook = getNotebook();
    notebook->AddPage(doc.getEditor(), doc.getTitle(), true);

    m_ctx.getFileHistory().addFile(filePath);
    return &doc;
}

namespace {

void reportSaveFailure(const DocumentIO::SaveResult result, Context& ctx, const TextEncoding encoding) {
    if (result == DocumentIO::SaveResult::EncodingError) {
        wxLogError(ctx.tr("messages.saveEncodingError"), encoding.toString().data());
    } else if (result == DocumentIO::SaveResult::IOError) {
        wxLogError("%s", ctx.tr("messages.saveIoError"));
    }
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
    editor->SetText(loaded->text);
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

auto DocumentManager::saveFile(Document& doc) const -> bool {
    if (doc.isNew()) {
        return saveFileAs(doc);
    }

    const auto result = DocumentIO::save(doc.getFilePath(), doc.getEditor()->GetText(), doc.getEncoding(), doc.getEolMode());
    if (result != DocumentIO::SaveResult::Success) {
        reportSaveFailure(result, m_ctx, doc.getEncoding());
        return false;
    }

    doc.setModified(false);
    doc.updateModTime();
    updateTabTitle(doc);
    reloadConfigIfMatches(doc.getFilePath());
    return true;
}

auto DocumentManager::saveFileAs(Document& doc) const -> bool {
    const auto typeKey = doc.getType() == DocumentType::HTML ? "html" : "freebasic";
    const auto filter = m_ctx.getConfigManager().filePatterns({ typeKey, "all" });

    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("files.saveTitle"),
        "",
        doc.isNew() ? wxString(".bas") : wxFileName(doc.getFilePath()).GetFullName(),
        filter,
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );

    if (dlg.ShowModal() != wxID_OK) {
        return false;
    }

    const auto newPath = dlg.GetPath();
    const auto result = DocumentIO::save(newPath, doc.getEditor()->GetText(), doc.getEncoding(), doc.getEolMode());
    if (result != DocumentIO::SaveResult::Success) {
        reportSaveFailure(result, m_ctx, doc.getEncoding());
        return false;
    }

    doc.setFilePath(newPath);
    doc.setModified(false);
    doc.updateModTime();
    updateTabTitle(doc);
    reloadConfigIfMatches(newPath);
    return true;
}

void DocumentManager::reloadConfigIfMatches(const wxString& path) const {
    if (m_ctx.getConfigManager().reloadIfKnown(path)) {
        m_ctx.getUIManager().refreshUi();
        m_ctx.getUIManager().updateEditorSettigs();
    }
}

auto DocumentManager::saveAllFiles() const -> bool {
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

    if (const auto idx = findPageIndex(doc); idx != wxNOT_FOUND) {
        getNotebook()->DeletePage(static_cast<size_t>(idx));
    }

    std::erase_if(m_documents, [&doc](const auto& ptr) { return ptr.get() == &doc; });

    // Update UI state when no documents remain
    if (m_documents.empty()) {
        m_ctx.getUIManager().setDocumentState(UIState::None);
        auto* frame = m_ctx.getUIManager().getMainFrame();
        frame->SetStatusText("", 1);
        frame->SetStatusText("", 2);
        frame->SetStatusText("", 3);
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
    return findByEditor(page);
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

auto DocumentManager::findByPath(const wxString& path) const -> Document* {
    auto normalized = wxFileName(path);
    normalized.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    const auto fullPath = normalized.GetFullPath();

    for (auto& doc : m_documents) {
        if (!doc->isNew()) {
            wxFileName docPath(doc->getFilePath());
            docPath.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
            if (docPath.GetFullPath() == fullPath) {
                return doc.get();
            }
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

auto DocumentManager::contains(const Document* doc) const -> bool {
    return doc != nullptr && std::ranges::contains(m_documents, doc, &std::unique_ptr<Document>::get);
}

auto DocumentManager::findPageIndex(const Document& doc) const -> int {
    const auto* notebook = m_ctx.getUIManager().getNotebook();
    for (size_t idx = 0; idx < notebook->GetPageCount(); idx++) {
        if (notebook->GetPage(idx) == doc.getEditor()) {
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

    // Pre-fill with selection or word under cursor
    if (const auto word = doc->getEditor()->getWordAtCursor(); !word.empty()) {
        m_findData.SetFindString(word);
    }

    auto* frame = m_ctx.getUIManager().getMainFrame();
    const int style = replace ? wxFR_REPLACEDIALOG : 0;
    const auto title = replace ? m_ctx.tr("dialogs.replace.title") : m_ctx.tr("dialogs.find.title");

    const auto dlg = make_unowned<wxFindReplaceDialog>(frame, &m_findData, title, style);
    dlg->PushEventHandler(this);
    dlg->Show();
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
}
