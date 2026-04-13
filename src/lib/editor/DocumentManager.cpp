//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentManager.hpp"
#include "Document.hpp"
#include "Editor.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/FileHistory.hpp"
#include "lib/config/Lang.hpp"
#include "lib/ui/UIManager.hpp"
using namespace fbide;

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

// ---------------------------------------------------------------------------
// File andling
// ---------------------------------------------------------------------------

auto DocumentManager::newFile(DocumentType type) -> Document& {
    auto* notebook = getNotebook();
    auto& doc = *m_documents.emplace_back(std::make_unique<Document>(notebook, m_ctx, type));
    notebook->AddPage(doc.getEditor(), doc.getTitle(), true);
    return doc;
}

void DocumentManager::openFile() {
    const auto& lang = m_ctx.getLang();
    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        lang[LangId::FileLoadTitle],
        "",
        ".bas",
        lang[LangId::FileLoadFilter],
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
    // Session files are loaded separately
    if (wxFileName(filePath).GetExt() == Config::SESSION_EXT) {
        loadSession(filePath);
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

    // Determine type and create document
    auto type = documentTypeFromPath(filePath);
    auto& doc = *m_documents.emplace_back(std::make_unique<Document>(getNotebook(), m_ctx, type));

    // Load file into editor
    if (!doc.getEditor()->LoadFile(filePath)) {
        m_documents.pop_back();
        return nullptr;
    }

    doc.setFilePath(filePath);
    doc.setModified(false);

    auto* notebook = getNotebook();
    notebook->AddPage(doc.getEditor(), doc.getTitle(), true);

    m_ctx.getFileHistory().addFile(filePath);
    return &doc;
}

auto DocumentManager::saveFile(Document& doc) const -> bool {
    if (doc.isNew()) {
        return saveFileAs(doc);
    }

    if (!doc.getEditor()->SaveFile(doc.getFilePath())) {
        return false;
    }

    doc.setModified(false);
    doc.updateModTime();
    updateTabTitle(doc);
    return true;
}

auto DocumentManager::saveFileAs(Document& doc) const -> bool {
    const auto& lang = m_ctx.getLang();

    wxString filter;
    if (doc.getType() == DocumentType::HTML) {
        filter = lang[LangId::FileHtmlFilter];
    } else {
        filter = lang[LangId::FileSaveFilter];
    }
    filter += lang[LangId::FileSaveFilterAny];

    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        lang[LangId::FileSaveTitle],
        "",
        doc.isNew() ? ".bas"s : wxFileName(doc.getFilePath()).GetFullName(),
        filter,
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );

    if (dlg.ShowModal() != wxID_OK) {
        return false;
    }

    const auto newPath = dlg.GetPath();
    if (!doc.getEditor()->SaveFile(newPath)) {
        return false;
    }

    doc.setFilePath(newPath);
    doc.setModified(false);
    doc.updateModTime();
    updateTabTitle(doc);
    return true;
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
        const auto& lang = m_ctx.getLang();
        const auto result = wxMessageBox(
            wxString::Format(lang[LangId::FileModifiedFormat], doc.getTitle()),
            lang[LangId::FileModifiedTitle],
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
        m_ctx.getUIManager().enableEditorMenus(false);
        m_ctx.getUIManager().getMainFrame()->SetStatusText("", 1);
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

    const auto& lang = m_ctx.getLang();
    const auto result = wxMessageBox(
        lang[LangId::SaveChanges],
        lang[LangId::SaveBeforeExit],
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

    m_ctx.getUIManager().enableEditorMenus(false);
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
    if (auto* doc = getActive()) {
        updateTabTitle(*doc);
    }
}

void DocumentManager::updateTabTitle(const Document& doc) const {
    auto idx = findPageIndex(doc);
    if (idx != wxNOT_FOUND) {
        getNotebook()->SetPageText(static_cast<size_t>(idx), doc.getTitle());
    }
}

// ---------------------------------------------------------------------------
// Sessions
// ---------------------------------------------------------------------------

namespace {
constexpr auto sessionHeader = "<fbide:session:version = \"0.2\"/>";
} // namespace

void DocumentManager::loadSession(const wxString& path) {
    wxTextFile file(path);
    if (!file.Open() || file.GetLineCount() == 0) {
        return;
    }

    // Check version header
    const bool isV2 = wxString(file[0]).Trim().Trim(false).Lower() == sessionHeader;
    const size_t startLine = isV2 ? 2 : 1;

    // Read selected tab index
    unsigned long selectedTab = 0;
    file[isV2 ? 1 : 0].ToULong(&selectedTab);

    // Open files
    for (size_t i = startLine; i < file.GetLineCount(); i++) {
        const auto filePath = file[i];
        if (filePath.empty() || !wxFileExists(filePath)) {
            if (isV2) {
                i += 2; // skip scroll-line and caret-pos
            }
            continue;
        }

        auto* doc = openFile(filePath);
        if (doc && isV2 && i + 2 < file.GetLineCount()) {
            unsigned long scrollLine = 0;
            unsigned long caretPos = 0;
            file[i + 1].ToULong(&scrollLine);
            file[i + 2].ToULong(&caretPos);

            auto* editor = doc->getEditor();
            editor->ScrollToLine(static_cast<int>(scrollLine));
            editor->SetCurrentPos(static_cast<int>(caretPos));
            editor->SetSelectionStart(static_cast<int>(caretPos));
            editor->SetSelectionEnd(static_cast<int>(caretPos));
        }

        if (isV2) {
            i += 2; // skip scroll-line and caret-pos
        }
    }

    // Restore selected tab
    auto* notebook = getNotebook();
    if (selectedTab < notebook->GetPageCount()) {
        notebook->SetSelection(selectedTab);
    }
}

void DocumentManager::loadSession() {
    const auto& lang = m_ctx.getLang();
    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        lang[LangId::FileLoadTitle],
        "", wxString(".") + Config::SESSION_EXT,
        lang[LangId::FileSessionFilter],
        wxFD_FILE_MUST_EXIST
    );
    if (dlg.ShowModal() == wxID_OK) {
        loadSession(dlg.GetPath());
    }
}

void DocumentManager::saveSession() {
    if (m_documents.empty()) {
        return;
    }

    const auto& lang = m_ctx.getLang();
    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        lang[LangId::FileSessionSaveTitle],
        "", wxString(".") + Config::SESSION_EXT,
        lang[LangId::FileSessionFilter],
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    // Prompt to save modified files first
    for (auto& doc : m_documents) {
        if (doc->isModified() && !doc->isNew()) {
            if (!saveFile(*doc)) {
                return;
            }
        }
    }

    wxTextFile file(dlg.GetPath());
    if (file.Exists()) {
        file.Open();
        file.Clear();
    } else {
        file.Create();
    }

    // Header
    file.AddLine(sessionHeader);

    // Selected tab index
    const auto* notebook = getNotebook();
    file.AddLine(wxString::Format("%d", notebook->GetSelection()));

    // File entries (skip untitled)
    for (const auto& doc : m_documents) {
        if (doc->isNew()) {
            continue;
        }
        const auto* editor = doc->getEditor();
        file.AddLine(doc->getFilePath());
        file.AddLine(wxString::Format("%d", editor->GetFirstVisibleLine()));
        file.AddLine(wxString::Format("%d", editor->GetCurrentPos()));
    }

    file.Write();
    file.Close();
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
    const auto& lang = m_ctx.getLang();
    const auto input = wxGetTextFromUser(
        lang[LangId::SearchGotoPrompt],
        lang[LangId::SearchGotoTitle],
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
    const auto& lang = m_ctx.getLang();
    const int style = replace ? wxFR_REPLACEDIALOG : 0;
    const auto& title = replace ? lang[LangId::SearchReplaceTitle] : lang[LangId::SearchFindTitle];

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
        dlg->Destroy();
    }
}
