//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <wx/app.h>
#include "config/ConfigManager.hpp"
#include "document/DocumentType.hpp"
#include "editor/CodeTransformer.hpp"
#include "editor/Editor.hpp"

namespace fbide::tests {

/// Spins up a real `fbide::Editor` plus the minimum dependencies it needs
/// for headless tests (ConfigManager + Theme + CodeTransformer). The
/// optional managers (DocumentManager, UIManager) are passed as null —
/// Editor null-guards every callsite that would touch them.
///
/// `FBIDE_TEST_RESOURCES_DIR` is the build-time absolute path to the
/// test-owned `tests/data/resources/` tree, which ships a minimal
/// `ide/config_test.ini` plus its referenced theme / keywords / locale.
/// Tests run against this isolated tree and do not depend on the
/// production `resources/` directory.
class EditorTestShim final {
public:
    EditorTestShim()
    : m_configManager(FBIDE_TEST_RESOURCES_DIR, /*idePath=*/ {}, /*configPath=*/"ide/config_test.ini")
    , m_transformer(m_configManager)
    , m_editor(new Editor(
          wxTheApp->GetTopWindow(),
          m_configManager,
          m_configManager.getTheme(),
          /*documentManager=*/nullptr,
          /*uiManager=*/nullptr,
          &m_transformer,
          DocumentType::FreeBASIC,
          /*preview=*/false
      )) {}

    ~EditorTestShim() { m_editor->Destroy(); }

    EditorTestShim(const EditorTestShim&) = delete;
    EditorTestShim(EditorTestShim&&) = delete;
    auto operator=(const EditorTestShim&) -> EditorTestShim& = delete;
    auto operator=(EditorTestShim&&) -> EditorTestShim& = delete;

    /// Editor under test.
    [[nodiscard]] auto editor() -> Editor& { return *m_editor; }

    /// Replace the entire buffer.
    void setText(const wxString& s) { m_editor->SetText(s); }

    /// Read the entire buffer back.
    [[nodiscard]] auto getText() const -> wxString { return m_editor->GetText(); }

private:
    ConfigManager m_configManager;
    CodeTransformer m_transformer;
    Editor* m_editor; ///< wx-owned via parent frame; freed via Destroy() in dtor.
};

} // namespace fbide::tests
