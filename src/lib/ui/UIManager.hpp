//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;

/// Manages the main application UI: frame, menus, toolbar, statusbar, layout.
/// Does not handle command logic — that is CommandManager's responsibility.
class UIManager final : public wxEvtHandler {
public:
    explicit UIManager(Context& ctx);

    /// Build the main application frame with all UI elements.
    void createMainFrame();

    /// Get the main frame.
    [[nodiscard]] auto getMainFrame() -> wxFrame* { return m_frame; }

    /// Enable/disable editor-dependent menus and toolbar items.
    void enableEditorMenus(bool state) const;

private:
    void createMenuBar();
    void createToolBar();
    void createStatusBar() const;
    void createLayout();

    Context& m_ctx;
    Unowned<wxFrame> m_frame;
    Unowned<wxMenu> m_fileMenu;
    Unowned<wxMenu> m_editMenu;
    Unowned<wxMenu> m_searchMenu;
    Unowned<wxMenu> m_viewMenu;
    Unowned<wxMenu> m_runMenu;
    Unowned<wxMenu> m_helpMenu;
    Unowned<wxToolBar> m_toolbar;
    Unowned<wxSplitterWindow> m_splitter;
    Unowned<wxPanel> m_codePanel;
    Unowned<wxListCtrl> m_console;
};

} // namespace fbide
