//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/geobide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "Document.hpp"
#include "DocumentPath.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "editor/Editor.hpp"
#include "editor/EditorPanel.hpp"
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

Document::Document(Context& ctx, const DocumentType type)
: m_ctx(ctx)
, m_type(type)
, m_encoding(defaultEncodingFromConfig(ctx))
, m_eolMode(defaultEolModeFromConfig(ctx)) {}

void Document::attachView(EditorPanel* panel) {
    m_panel = panel;
    if (panel != nullptr) {
        panel->getEditor()->SetEOLMode(m_eolMode.toStc());
    }
}

void Document::detachView() {
    m_panel = nullptr;
}

auto Document::getEditor() -> Editor* {
    return m_panel != nullptr ? m_panel->getEditor() : nullptr;
}

auto Document::getEditor() const -> const Editor* {
    return m_panel != nullptr ? m_panel->getEditor() : nullptr;
}

auto Document::getPage() -> wxWindow* {
    return m_panel.get();
}

auto Document::getPage() const -> const wxWindow* {
    return m_panel.get();
}

void Document::showMinimap(const bool enabled) {
    if (m_panel != nullptr) {
        m_panel->showMinimap(enabled);
    }
}

void Document::updateSettings() {
    if (m_panel != nullptr) {
        m_panel->updateSettings();
    }
}

void Document::setFilePath(const std::filesystem::path& path) {
    m_filePath = path;
    // Only re-derive type from the new path when the user hasn't
    // explicitly overridden it — Save As shouldn't stomp a manual choice.
    if (!m_typeOverridden) {
        const auto newType = documentTypeFromPath(path);
        if (newType != m_type) {
            m_type = newType;
            if (m_panel != nullptr) {
                m_panel->getEditor()->setDocType(newType);
            }
        }
    }
    updateModTime();
}

void Document::setType(const DocumentType type) {
    m_typeOverridden = true;
    if (type == m_type) {
        return;
    }
    const auto previous = m_type;
    m_type = type;
    if (m_panel != nullptr) {
        m_panel->getEditor()->setDocType(type);
    }
    // Cross-cutting side effects (intellisense submit/cancel, sidebar
    // refresh) live on the subscriber side — `DocumentManager`
    // registers the handler at document creation time.
    if (m_onTypeChanged) {
        m_onTypeChanged(*this, previous);
    }
}

void Document::onTypeChanged(DocumentTypeChangedHandler handler) {
    m_onTypeChanged = std::move(handler);
}

auto Document::getTitle() const -> wxString {
    wxString title = isNew()
                       ? m_ctx.tr("document.untitled")
                       : toWxString(m_filePath.filename());
    if (isModified()) {
        title = "[*] " + title;
    }
    return title;
}

auto Document::getFrameTitle() const -> wxString {
    return isNew() ? getTitle() : toWxString(m_filePath);
}

auto Document::isModified() const -> bool {
    return m_metaModified || (m_panel != nullptr && m_panel->isModified());
}

void Document::setModified(const bool modified) {
    if (!modified) {
        if (m_panel != nullptr) {
            m_panel->markSaved();
        }
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
    if (m_panel != nullptr) {
        auto* editor = m_panel->getEditor();
        editor->ConvertEOLs(mode.toStc());
        editor->SetEOLMode(mode.toStc());
    }
    // View-less: the new mode is applied via `attachView` next time
    // a panel is paired with this document.
}

auto Document::checkExternalChange() const -> bool {
    if (isNew()) {
        return false;
    }
    std::error_code ec;
    const auto currentModTime = std::filesystem::last_write_time(m_filePath, ec);
    if (ec) {
        return false;
    }
    return m_modTime != std::filesystem::file_time_type {} && currentModTime != m_modTime;
}

auto Document::getKeywordAtCursor() const -> wxString {
    // No view → no cursor → nothing under it.
    if (m_panel == nullptr) {
        return {};
    }
    // wxSTC's position / word boundary helpers aren't const; the
    // original implementation reached through the non-const editor
    // accessor — preserved here.
    auto* editor = m_panel->getEditor();
    const auto pos = editor->GetCurrentPos();
    const auto start = editor->WordStartPosition(pos, true);
    const auto end = editor->WordEndPosition(pos, true);
    auto keyword = editor->GetTextRange(start, end).Strip(wxString::both);
    keyword.MakeLower();

    if (keyword.empty()) {
        return {};
    }

    // Include '#' for preprocessor directives like #IFDEF
    if (m_type == DocumentType::FreeBASIC && start > 0 && editor->GetCharAt(start - 1) == '#') {
        keyword = "#" + keyword;
    }

    return keyword;
}

void Document::updateModTime() {
    if (isNew()) {
        m_modTime = {};
        return;
    }
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(m_filePath, ec);
    m_modTime = ec ? std::filesystem::file_time_type {} : time;
}
