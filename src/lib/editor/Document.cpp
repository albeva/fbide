//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Document.hpp"
#include "Editor.hpp"
#include "app/Context.hpp"
#include "config/Lang.hpp"
#include "config/LangId.hpp"
using namespace fbide;

Document::Document(wxWindow* parent, Context& ctx, const DocumentType type)
: m_ctx(ctx)
, m_type(type)
, m_editor(make_unowned<Editor>(parent, ctx, type)) {}

void Document::setFilePath(const wxString& path) {
    m_filePath = path;
    const auto newType = documentTypeFromPath(path);
    if (newType != m_type) {
        m_type = newType;
        m_editor->setDocType(newType);
    }
    updateModTime();
}

auto Document::getTitle() const -> wxString {
    wxString title = isNew()
                       ? m_ctx.getLang()[LangId::Untitled]
                       : wxFileName(m_filePath).GetFullName();
    if (isModified()) {
        title = "[*] " + title;
    }
    return title;
}

auto Document::isModified() const -> bool {
    return m_editor && m_editor->GetModify();
}

void Document::setModified(const bool modified) const {
    if (m_editor) {
        if (modified) {
            // Can't force STC to modified state directly, but this
            // is typically called with false to clear the state
        } else {
            m_editor->SetSavePoint();
        }
    }
}

auto Document::checkExternalChange() const -> bool {
    if (isNew() || !wxFileExists(m_filePath)) {
        return false;
    }
    const wxDateTime currentModTime = wxFileName(m_filePath).GetModificationTime();
    return m_modTime.IsValid() && currentModTime.IsValid() && currentModTime != m_modTime;
}

auto Document::getKeywordAtCursor() const -> wxString {
    if (m_editor == nullptr) {
        return {};
    }

    const auto pos = m_editor->GetCurrentPos();
    const auto start = m_editor->WordStartPosition(pos, true);
    const auto end = m_editor->WordEndPosition(pos, true);
    auto keyword = m_editor->GetTextRange(start, end).Strip(wxString::both);
    keyword.MakeLower();

    if (keyword.empty()) {
        return {};
    }

    // Include '#' for preprocessor directives like #IFDEF
    if (m_type == DocumentType::FreeBASIC && start > 0 && m_editor->GetCharAt(start - 1) == '#') {
        keyword = "#" + keyword;
    }

    return keyword;
}

void Document::updateModTime() {
    if (!isNew() && wxFileExists(m_filePath)) {
        m_modTime = wxFileName(m_filePath).GetModificationTime();
    } else {
        m_modTime = wxDateTime();
    }
}
