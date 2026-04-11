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
#include "lib/config/Lang.hpp"
#include "lib/ui/UIManager.hpp"
using namespace fbide;

DocumentManager::DocumentManager(Context& ctx)
: m_ctx(ctx) {}

auto DocumentManager::createNew(DocumentType type) -> Document& {
    auto* notebook = getNotebook();
    auto& doc = *m_documents.emplace_back(std::make_unique<Document>(notebook, m_ctx, type));
    notebook->AddPage(doc.getEditor(), doc.getTitle(), true);
    return doc;
}

auto DocumentManager::open(const wxString& filePath) -> Document* {
    // Check if already open
    if (auto* existing = findByPath(filePath)) {
        auto idx = findPageIndex(*existing);
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
    return &doc;
}

auto DocumentManager::close(Document& doc) -> bool {
    if (doc.isModified()) {
        const auto& lang = m_ctx.getLang();
        auto result = wxMessageBox(
            wxString::Format(lang[LangId::FileModifiedFormat], doc.getTitle()),
            lang[LangId::FileModifiedTitle],
            wxYES_NO | wxCANCEL | wxICON_EXCLAMATION,
            m_ctx.getUIManager().getMainFrame());

        if (result == wxCANCEL) {
            return false;
        }
        // TODO: if YES, save the file
    }

    auto idx = findPageIndex(doc);
    if (idx != wxNOT_FOUND) {
        getNotebook()->DeletePage(static_cast<size_t>(idx));
    }

    std::erase_if(m_documents, [&doc](const auto& ptr) { return ptr.get() == &doc; });
    return true;
}

auto DocumentManager::closeAll() -> bool {
    while (!m_documents.empty()) {
        if (!close(*m_documents.back())) {
            return false;
        }
    }
    return true;
}

auto DocumentManager::getActive() -> Document* {
    auto* notebook = getNotebook();
    auto sel = notebook->GetSelection();
    if (sel == wxNOT_FOUND) {
        return nullptr;
    }
    auto* page = notebook->GetPage(static_cast<size_t>(sel));
    return findByEditor(page);
}

auto DocumentManager::findByPath(const wxString& path) -> Document* {
    auto normalized = wxFileName(path);
    normalized.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    auto fullPath = normalized.GetFullPath();

    for (auto& doc : m_documents) {
        if (!doc->isUntitled()) {
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

auto DocumentManager::findByEditor(const wxWindow* editor) -> Document* {
    for (auto& doc : m_documents) {
        if (doc->getEditor() == editor) {
            return doc.get();
        }
    }
    return nullptr;
}

auto DocumentManager::findPageIndex(const Document& doc) const -> int {
    auto* notebook = m_ctx.getUIManager().getNotebook();
    for (size_t idx = 0; idx < notebook->GetPageCount(); idx++) {
        if (notebook->GetPage(idx) == doc.getEditor()) {
            return static_cast<int>(idx);
        }
    }
    return wxNOT_FOUND;
}

auto DocumentManager::getNotebook() -> wxAuiNotebook* {
    return m_ctx.getUIManager().getNotebook();
}
