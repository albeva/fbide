//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "EditorPanel.hpp"
#include <wx/stc/minimap.h>
#include "Editor.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "document/Document.hpp"
#include "document/DocumentInfoBar.hpp"
#include "document/DocumentManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

wxBEGIN_EVENT_TABLE(EditorPanel, wxPanel)
    EVT_SIZE(EditorPanel::onSize)
wxEND_EVENT_TABLE()

namespace {

/// Default minimap pixel width when no config override is set.
constexpr int kDefaultMinimapWidth = 200;

/// Minimum editor width kept when the minimap is shown. When the page
/// is narrower than `minimapWidth + this`, the minimap auto-hides.
constexpr int kMinEditorWidth = 100;

auto minimapWidthFromConfig(Context& ctx) -> int {
    return ctx.getConfigManager().config().at("editor").get_or("minimapWidth", kDefaultMinimapWidth);
}

auto minimapEnabledFromConfig(Context& ctx) -> bool {
    return ctx.getConfigManager().config().get_or("commands.viewMinimap", true);
}

} // namespace

EditorPanel::EditorPanel(wxWindow* parent, Context& ctx, const DocumentType type, Document& doc)
: wxPanel(parent, wxID_ANY)
, m_doc(doc)
, m_editor(
      make_unowned<Editor>(
          this, ctx.getConfigManager(), ctx.getTheme(),
          &ctx.getDocumentManager(), &ctx.getUIManager(),
          &ctx.getDocumentManager().getCodeTransformer(), type
      )
  )
, m_minimapWidth(minimapWidthFromConfig(ctx))
, m_minimapEnabled(minimapEnabledFromConfig(ctx)) {
    // Page layout: the info bar (hidden until an external change) spans the
    // top; the editor + optional minimap fill the rest. The minimap docks
    // into the inner horizontal sizer to the editor's right.
    m_infoBar = make_unowned<DocumentInfoBar>(this, ctx, doc);

    const auto inner = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    inner->Add(m_editor, 1, wxEXPAND);
    m_editorSizer = inner.get();

    const auto outer = make_unowned<wxBoxSizer>(wxVERTICAL);
    outer->Add(m_infoBar, 0, wxEXPAND);
    outer->Add(inner, 1, wxEXPAND);
    SetSizer(outer);

    if (m_minimapEnabled) {
        createMinimap();
    }

    updateMinimapVisibility();

    // Publish the back-link last — by the time the document looks at
    // its view, the editor is fully wired and ready.
    m_doc.attachView(this, m_editor);
}

EditorPanel::~EditorPanel() {
    m_doc.detachView();
}

void EditorPanel::showMinimap(const bool enabled) {
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

void EditorPanel::updateSettings() {
    m_editor->applySettings();
    if (m_minimap != nullptr) {
        m_minimap->SetEdit(nullptr);
        m_minimap->SetEdit(m_editor);
    }
}

void EditorPanel::showExternalBar(const Document::ExternalChange kind) {
    if (kind == Document::ExternalChange::Conflict) {
        m_infoBar->showConflict();
    } else if (kind == Document::ExternalChange::Deleted) {
        m_infoBar->showDeleted();
    }
}

void EditorPanel::hideExternalBar() {
    m_infoBar->dismiss();
}

void EditorPanel::createMinimap() {
    if (m_minimap != nullptr) {
        return;
    }
    m_minimap = make_unowned<wxStyledTextCtrlMiniMap>(this, m_editor.get());
    // The minimap's Create() hardcodes a border with no style hook, so the
    // wxGTK themed frame can only be cleared after construction.
    m_minimap->SetWindowStyleFlag((m_minimap->GetWindowStyleFlag() & ~wxBORDER_MASK) | wxBORDER_NONE);
    m_minimap->SetMinSize(wxSize(m_minimapWidth, -1));
    if (auto* sizer = m_editorSizer; sizer != nullptr) {
        sizer->Add(m_minimap, 0, wxEXPAND);
        sizer->Layout();
    }
    // Minimap doesn't pick margins up correctly, so force-redefine it
    m_editor->defineChangesMargin();
}

void EditorPanel::destroyMinimap() {
    if (m_minimap == nullptr) {
        return;
    }
    if (auto* sizer = m_editorSizer; sizer != nullptr) {
        sizer->Detach(m_minimap.get());
        sizer->Layout();
    }
    // wx-owned via this panel — Destroy() unparents and frees it.
    m_minimap->Destroy();
    m_minimap = nullptr;
}

void EditorPanel::onSize(wxSizeEvent& event) {
    event.Skip();
    updateMinimapVisibility();
}

void EditorPanel::updateMinimapVisibility() const {
    auto* sizer = m_editorSizer;
    if (sizer == nullptr || m_minimap == nullptr) {
        return;
    }
    const int available = GetClientSize().GetWidth();
    const bool visible = available >= m_minimapWidth + kMinEditorWidth;
    if (sizer->IsShown(m_minimap.get()) == visible) {
        return;
    }
    sizer->Show(m_minimap.get(), visible);
    sizer->Layout();
}
