//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "Document.hpp"
#include "DocumentPath.hpp"
#include "config/ConfigManager.hpp"
#include "editor/Editor.hpp"
#include "editor/EditorPanel.hpp"
using namespace fbide;

wxDEFINE_EVENT(fbide::EVT_DOCUMENT_TYPE_CHANGED, DocumentTypeChangedEvent);

namespace {

auto defaultEncodingFromConfig(ConfigManager& config) -> TextEncoding {
    const auto& editor = config.config().at("editor");
    const auto key = editor.get_or("encoding", "UTF-8");
    return TextEncoding::parse(key.ToStdString()).value_or(TextEncoding::UTF8);
}

auto defaultEolModeFromConfig(ConfigManager& config) -> EolMode {
    const auto& editor = config.config().at("editor");
    const auto key = editor.get_or("eolMode", "LF");
    return EolMode::parse(key.ToStdString()).value_or(EolMode::LF);
}

} // namespace

Document::Document(ConfigManager& config, const DocumentType type, wxEvtHandler* sink)
: m_config(config)
, m_type(type)
, m_encoding(defaultEncodingFromConfig(config))
, m_eolMode(defaultEolModeFromConfig(config))
, m_sink(sink) {}

void Document::attachView(wxWindow* view, Editor* editor) {
    m_view = view;
    m_editor = editor;
    if (editor != nullptr) {
        editor->SetEOLMode(m_eolMode.toStc());
    }
}

void Document::detachView() {
    m_view = nullptr;
    m_editor = nullptr;
}

void Document::showMinimap(const bool enabled) {
    if (auto* panel = dynamic_cast<EditorPanel*>(m_view.get())) {
        panel->showMinimap(enabled);
    }
}

void Document::updateSettings() {
    if (auto* panel = dynamic_cast<EditorPanel*>(m_view.get())) {
        panel->updateSettings();
    }
}

auto Document::getFilePath() const -> std::filesystem::path {
    // Persistent project: the path lives on the bound file node.
    if (const auto* nodeSlot = std::get_if<Project::Node*>(&m_source)) {
        const auto* node = *nodeSlot;
        return node != nullptr ? node->path : std::filesystem::path {};
    }
    // Standalone (shared ephemeral) or untitled: the document owns its path.
    return std::get<std::filesystem::path>(m_source);
}

void Document::setFilePath(const std::filesystem::path& path) {
    // The path's owner depends on the binding: a persistent project keeps
    // it on the bound file node (and re-keys its path index); an ephemeral
    // project owns it directly; an unbound document holds it in `m_source`.
    if (auto* nodeSlot = std::get_if<Project::Node*>(&m_source)) {
        const auto result = static_cast<Project*>(m_project)->setFilePath(*nodeSlot, path);
        if (!result.has_value()) {
            // Project rejected the path (typically OutOfTree). The caller
            // should have validated upstream — bail without mutating so
            // state stays consistent.
            wxLogError("Document::setFilePath: project rejected new path");
            return;
        }
    } else {
        // Standalone (shared ephemeral) or untitled: the document owns the path.
        m_source = path;
    }
    // Only re-derive type from the new path when the user hasn't
    // explicitly overridden it — Save As shouldn't stomp a manual choice.
    if (!m_typeOverridden) {
        const auto newType = documentTypeFromPath(path);
        if (newType != m_type) {
            m_type = newType;
            if (m_editor != nullptr) {
                m_editor->setDocType(newType);
            }
        }
    }
    updateModTime();
}

void Document::bindToProject(ProjectBase* project, Project::Node* node) {
    assert(project != nullptr && "Project should not be nil");
    m_project = project;
    if (node != nullptr) {
        // Persistent: the path now lives on the project's file node.
        m_source = node;
    }
    // Shared ephemeral (node == nullptr): the document keeps its own path
    // in `m_source`.
}

auto Document::getNode() const -> Project::Node* {
    if (const auto* slot = std::get_if<Project::Node*>(&m_source)) {
        return *slot;
    }
    return nullptr;
}

void Document::setType(const DocumentType type) {
    m_typeOverridden = true;
    if (type == m_type) {
        return;
    }
    const auto previous = m_type;
    m_type = type;
    if (m_editor != nullptr) {
        m_editor->setDocType(type);
    }
    // Cross-cutting side effects (intellisense submit/cancel, sidebar
    // refresh) live on the subscriber side — `DocumentManager` binds
    // to EVT_DOCUMENT_TYPE_CHANGED at construction so this synchronous
    // dispatch routes straight into its handler.
    if (m_sink != nullptr) {
        DocumentTypeChangedEvent evt { this, previous };
        m_sink->ProcessEvent(evt);
    }
}

auto Document::getTitle() const -> wxString {
    wxString title = isNew()
                       ? m_config.locale().get_or("document.untitled", "")
                       : toWxString(getFilePath().filename());
    if (isModified()) {
        title = "[*] " + title;
    }
    return title;
}

auto Document::getFrameTitle() const -> wxString {
    const wxString base = isNew() ? getTitle() : toWxString(getFilePath());
    // Prefix with the owning project's name when it has one — the persistent
    // project; the shared ephemeral returns an empty name, so standalone files
    // show just the path.
    if (m_project != nullptr) {
        if (const auto name = m_project->getName(); !name.empty()) {
            return "[" + name + "] " + base;
        }
    }
    return base;
}

auto Document::isModified() const -> bool {
    return m_metaModified || (m_editor != nullptr && m_editor->GetModify());
}

void Document::markSaved() {
    if (m_editor != nullptr) {
        m_editor->SetSavePoint();
    }
    m_metaModified = false;
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
    if (m_editor != nullptr) {
        m_editor->ConvertEOLs(mode.toStc());
        m_editor->SetEOLMode(mode.toStc());
    }
}

auto Document::checkExternalChange() const -> bool {
    if (isNew()) {
        return false;
    }
    std::error_code ec;
    const auto currentModTime = std::filesystem::last_write_time(getFilePath(), ec);
    if (ec) {
        return false;
    }
    return m_modTime != std::filesystem::file_time_type {} && currentModTime != m_modTime;
}

void Document::updateModTime() {
    if (isNew()) {
        m_modTime = {};
        return;
    }
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(getFilePath(), ec);
    m_modTime = ec ? std::filesystem::file_time_type {} : time;
}

void Document::setSessionAttributes(wxConfigBase& cfg, const SessionScope scope) {
    // Project-meaningful state (encoding / EOL / type) — versioned in the `.fbp`,
    // also part of the standalone `.fbs`. Needs no editor.
    if (scope != SessionScope::Session) {
        // Only persist encoding / EOL when they differ from the config default —
        // a default value is re-derived on open, so storing it is just noise.
        if (m_encoding != defaultEncodingFromConfig(m_config)) {
            cfg.Write("encoding", wxString(m_encoding.toString()));
        }
        if (m_eolMode != defaultEolModeFromConfig(m_config)) {
            cfg.Write("eolMode", wxString(m_eolMode.toString()));
        }
        if (m_typeOverridden) {
            const auto typeKey = documentTypeKey(m_type);
            cfg.Write("type", wxString::FromUTF8(typeKey.data(), typeKey.size()));
        }
    }
    // Pinned compiler configuration — part of a standalone file's session only;
    // project files don't persist a per-document configuration. Empty optional
    // = "follow active", which needs no on-disk representation.
    if (scope == SessionScope::Ephemeral && m_project != nullptr) {
        if (const auto slug = m_project->getConfigurationSlug(); slug.has_value()) {
            cfg.Write("configuration", *slug);
        }
    }
    // Per-user runtime UI state (scroll / caret / folds) — needs the editor.
    if (scope != SessionScope::Project) {
        auto* editor = m_editor.get();
        if (editor == nullptr) {
            return;
        }
        cfg.Write("scroll", editor->GetFirstVisibleLine());
        cfg.Write("cursor", editor->GetCurrentPos());
        if (m_config.config().get_or("editor.folderMargin", false)) {
            editor->Colourise(editor->GetEndStyled(), -1);
            wxString folds;
            const auto lines = editor->GetLineCount();
            for (int line = 0; line < lines; line++) {
                if (!editor->GetFoldExpanded(line)) {
                    if (!folds.empty()) {
                        folds += ",";
                    }
                    folds += std::to_string(line);
                }
            }
            if (!folds.empty()) {
                cfg.Write("folds", folds);
            }
        }
    }
}

void Document::loadSessionAttributes(const wxConfigBase& cfg, const SessionScope scope) {
    if (scope != SessionScope::Session) {
        wxString encKey;
        if (cfg.Read("encoding", &encKey) && !encKey.empty()) {
            if (const auto enc = TextEncoding::parse(encKey.ToStdString())) {
                setEncoding(*enc);
            }
        }
        wxString eolKey;
        if (cfg.Read("eolMode", &eolKey) && !eolKey.empty()) {
            if (const auto eol = EolMode::parse(eolKey.ToStdString())) {
                setEolMode(*eol);
            }
        }
        wxString typeKey;
        if (cfg.Read("type", &typeKey) && !typeKey.empty()) {
            if (const auto type = documentTypeFromKey(typeKey.ToStdString())) {
                setType(*type);
            }
        }
        // setEncoding / setEolMode flip the meta-dirty flag — clear it.
        markSaved();
    }
    // Restore the pinned compiler configuration (standalone sessions only).
    if (scope == SessionScope::Ephemeral) {
        wxString configSlug;
        if (cfg.Read("configuration", &configSlug) && !configSlug.empty()) {
            if (m_project != nullptr) {
                m_project->setConfigurationSlug(configSlug);
            }
        }
    }
    if (scope != SessionScope::Project) {
        auto* editor = m_editor.get();
        if (editor == nullptr) {
            return;
        }
        long scroll = 0;
        long cursor = 0;
        cfg.Read("scroll", &scroll, 0L);
        cfg.Read("cursor", &cursor, 0L);
        editor->ScrollToLine(static_cast<int>(scroll));
        editor->SetCurrentPos(static_cast<int>(cursor));
        editor->SetSelectionStart(static_cast<int>(cursor));
        editor->SetSelectionEnd(static_cast<int>(cursor));

        if (m_config.config().get_or("editor.folderMargin", false)) {
            wxString folds;
            if (cfg.Read("folds", &folds)) {
                editor->Colourise(editor->GetEndStyled(), -1);
                for (const auto& str : wxSplit(folds, ',')) {
                    int line = 0;
                    if (str.ToInt(&line)) {
                        const auto last = editor->GetLastChild(line, -1);
                        if (last > line) {
                            editor->SetFoldExpanded(line, false);
                            editor->HideLines(line + 1, last);
                        }
                    }
                }
            }
        }
    }
}
