//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Document.hpp"
#include "DocumentManager.hpp"
#include "Editor.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
using namespace fbide;

namespace {

auto defaultEncodingFromConfig(Context& ctx) -> TextEncoding {
    const auto& editor = ctx.getConfigManager().config().at("editor");
    const auto key = editor.get_or("encoding", "UTF-8");
    return TextEncoding::parse(key.ToStdString()).value_or(TextEncoding::UTF8);
}

auto defaultEolModeFromConfig(Context& ctx) -> EolMode {
    const auto& editor = ctx.getConfigManager().config().at("editor");
    const auto key = editor.get_or("eolMode", "LF");
    return EolMode::parse(key.ToStdString()).value_or(EolMode::LF);
}

} // namespace

Document::Document(wxWindow* parent, Context& ctx, const DocumentType type)
: m_ctx(ctx)
, m_type(type)
, m_editor(make_unowned<Editor>(parent, ctx, &ctx.getDocumentManager().getCodeTransformer(), type))
, m_encoding(defaultEncodingFromConfig(ctx))
, m_eolMode(defaultEolModeFromConfig(ctx)) {
    if (m_editor) {
        m_editor->SetEOLMode(m_eolMode.toStc());
    }
}

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
                       ? m_ctx.tr("document.untitled")
                       : wxFileName(m_filePath).GetFullName();
    if (isModified()) {
        title = "[*] " + title;
    }
    return title;
}

auto Document::isModified() const -> bool {
    return m_metaModified || (m_editor && m_editor->GetModify());
}

void Document::setModified(const bool modified) {
    if (m_editor) {
        if (modified) {
            // Can't force STC to modified state directly, but this
            // is typically called with false to clear the state
        } else {
            m_editor->SetSavePoint();
        }
    }
    if (!modified) {
        m_metaModified = false;
    }
}

void Document::setEncoding(const TextEncoding encoding) {
    if (m_encoding == encoding) {
        return;
    }
    m_encoding = encoding;
    m_metaModified = true;
}

void Document::setEolMode(const EolMode mode) {
    if (m_eolMode == mode) {
        return;
    }
    m_eolMode = mode;
    if (m_editor) {
        m_editor->ConvertEOLs(mode.toStc());
        m_editor->SetEOLMode(mode.toStc());
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
