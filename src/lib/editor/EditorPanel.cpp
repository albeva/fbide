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
#include "document/DocumentManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

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

EditorPanel::EditorPanel(wxWindow* parent, Context& ctx, const DocumentType type)
: wxPanel(parent, wxID_ANY)
, m_editor(
      make_unowned<Editor>(
          this, ctx.getConfigManager(), ctx.getTheme(),
          &ctx.getDocumentManager(), &ctx.getUIManager(),
          &ctx.getDocumentManager().getCodeTransformer(), type
      )
  )
, m_minimapWidth(minimapWidthFromConfig(ctx))
, m_minimapEnabled(minimapEnabledFromConfig(ctx)) {
    // Editor fills the page; the minimap (when enabled) docks to its right.
    const auto sizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    sizer->Add(m_editor, 1, wxEXPAND);
    SetSizer(sizer);

    if (m_minimapEnabled) {
        createMinimap();
    }

    // Auto-hide the minimap when the page becomes too narrow.
    Bind(wxEVT_SIZE, &EditorPanel::onSize, this);
    updateMinimapVisibility();
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

auto EditorPanel::isModified() const -> bool {
    return m_editor->GetModify();
}

void EditorPanel::markSaved() {
    m_editor->SetSavePoint();
}

void EditorPanel::updateSettings() {
    m_editor->applySettings();
    if (m_minimap != nullptr) {
        m_minimap->SetEdit(nullptr);
        m_minimap->SetEdit(m_editor);
    }
}

void EditorPanel::createMinimap() {
    if (m_minimap != nullptr) {
        return;
    }
    m_minimap = make_unowned<wxStyledTextCtrlMiniMap>(this, m_editor.get());
    m_minimap->SetMinSize(wxSize(m_minimapWidth, -1));
    if (auto* sizer = GetSizer(); sizer != nullptr) {
        sizer->Add(m_minimap, 0, wxEXPAND);
        sizer->Layout();
    }
}

void EditorPanel::destroyMinimap() {
    if (m_minimap == nullptr) {
        return;
    }
    if (auto* sizer = GetSizer(); sizer != nullptr) {
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
    auto* sizer = GetSizer();
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
