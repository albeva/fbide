//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "Document.hpp"
#include "DocumentManager.hpp"
#include "DocumentPath.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "editor/Editor.hpp"
#include "sidebar/SideBarManager.hpp"
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

constexpr int kDefaultMinimapWidth = 200;

auto minimapWidthFromConfig(Context& ctx) -> int {
    return ctx.getConfigManager().config().at("editor").get_or("minimapWidth", kDefaultMinimapWidth);
}

auto minimapEnabledFromConfig(Context& ctx) -> bool {
    return ctx.getConfigManager().config().get_or("commands.viewMinimap", true);
}

/// Minimum editor width kept when the minimap is shown. When the page is
/// narrower than minimapWidth + this, the minimap auto-hides.
constexpr int kMinEditorWidth = 100;

} // namespace

Document::Document(wxWindow* parent, Context& ctx, const DocumentType type)
: m_ctx(ctx)
, m_type(type)
, m_container(make_unowned<wxPanel>(parent))
, m_editor(
      make_unowned<Editor>(
          m_container.get(), ctx.getConfigManager(), ctx.getTheme(),
          &ctx.getDocumentManager(), &ctx.getUIManager(),
          &ctx.getDocumentManager().getCodeTransformer(), type
      )
  )
, m_minimapWidth(minimapWidthFromConfig(ctx))
, m_minimapEnabled(minimapEnabledFromConfig(ctx))
, m_encoding(defaultEncodingFromConfig(ctx))
, m_eolMode(defaultEolModeFromConfig(ctx)) {
    // Editor fills the page; the minimap (when enabled) docks to its right.
    const auto sizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    sizer->Add(m_editor, 1, wxEXPAND);
    m_container->SetSizer(sizer);

    // Auto-hide the minimap when the page becomes too narrow.
    m_container->Bind(wxEVT_SIZE, &Document::onContainerSize, this);
    m_editor->SetEOLMode(m_eolMode.toStc());

    if (m_minimapEnabled) {
        createMinimap();
        updateMinimapVisibility();
    }
}

void Document::showMinimap(const bool enabled) {
    if (m_minimapEnabled == enabled) {
        return;
    }
    m_minimapEnabled = enabled;
    if (enabled) {
        createMinimap();
    } else {
        destroyMinimap();
    }
    updateMinimapVisibility();
}

void Document::createMinimap() {
    if (m_minimap != nullptr) {
        return;
    }
    m_minimap = make_unowned<wxStyledTextCtrlMiniMap>(m_container, getEditor());
    // The minimap's Create() hardcodes a border with no style hook, so the
    // wxGTK themed frame can only be cleared after construction.
    m_minimap->SetWindowStyleFlag((m_minimap->GetWindowStyleFlag() & ~wxBORDER_MASK) | wxBORDER_NONE);
    m_minimap->SetMinSize(wxSize(m_minimapWidth, -1));
    if (auto* sizer = m_container->GetSizer(); sizer != nullptr) {
        sizer->Add(m_minimap, 0, wxEXPAND);
        sizer->Layout();
    }
    // Minimap doesn't pick margins up correctly, so force-redefine it
    getEditor()->defineChangesMargin();
}

void Document::destroyMinimap() {
    if (m_minimap == nullptr) {
        return;
    }
    if (auto* sizer = m_container->GetSizer(); sizer != nullptr) {
        sizer->Detach(m_minimap.get());
        sizer->Layout();
    }
    // wx-owned via m_container — Destroy() unparents and frees it.
    m_minimap->Destroy();
    m_minimap = nullptr;
}

void Document::updateSettings() {
    getEditor()->applySettings();
    if (m_minimap != nullptr) {
        m_minimap->SetEdit(nullptr);
        m_minimap->SetEdit(getEditor());
    }
}

void Document::onContainerSize(wxSizeEvent& event) {
    event.Skip();
    updateMinimapVisibility();
}

void Document::updateMinimapVisibility() const {
    auto* sizer = m_container->GetSizer();
    if (sizer == nullptr || m_minimap == nullptr) {
        return;
    }
    const int available = m_container->GetClientSize().GetWidth();
    const bool visible = available >= m_minimapWidth + kMinEditorWidth;
    if (sizer->IsShown(m_minimap.get()) == visible) {
        return;
    }
    sizer->Show(m_minimap.get(), visible);
    sizer->Layout();
}

void Document::setFilePath(const std::filesystem::path& path) {
    m_filePath = path;
    // Only re-derive type from the new path when the user hasn't
    // explicitly overridden it — Save As shouldn't stomp a manual choice.
    if (!m_typeOverridden) {
        const auto newType = documentTypeFromPath(path);
        if (newType != m_type) {
            m_type = newType;
            m_editor->setDocType(newType);
        }
    }
    updateModTime();
}

void Document::setType(const DocumentType type) {
    m_typeOverridden = true;
    if (type == m_type) {
        return;
    }
    m_type = type;
    m_editor->setDocType(type);

    auto& dm = m_ctx.getDocumentManager();
    if (type == DocumentType::FreeBASIC) {
        // Re-enter the FreeBASIC pipeline — submit the current buffer for
        // intellisense so the symbol browser populates.
        dm.submitIntellisense(this, m_editor->GetText());
    } else {
        // Leaving FreeBASIC: drop any in-flight intellisense work, release
        // the symbol table (frees the shared_ptr — workers may still hold
        // a reference until they finish, which is fine), and clear the
        // sub/function browser if this is the active document.
        dm.cancelIntellisense(this);
        m_symbolTable = nullptr;
        if (dm.getActive() == this) {
            m_ctx.getSideBarManager().showSymbolsFor(nullptr);
        }
    }
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

auto Document::isModified() const -> bool {
    return m_metaModified || m_editor->GetModify();
}

void Document::setModified(const bool modified) {
    if (!modified) {
        m_editor->SetSavePoint();
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
    m_editor->ConvertEOLs(mode.toStc());
    m_editor->SetEOLMode(mode.toStc());
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
    if (isNew()) {
        m_modTime = {};
        return;
    }
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(m_filePath, ec);
    m_modTime = ec ? std::filesystem::file_time_type {} : time;
}
