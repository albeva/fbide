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
class GeneralPage;
class ThemePage;
class KeywordsPage;
class CompilerPage;
class Panel;

/// Settings dialog with tabs for General, Themes, Keywords, and Compiler.
/// Each tab is managed by its own page class.
/// Applies changes only on OK.
class SettingsDialog : public wxDialog {
public:
    /// Tab order on the dialog notebook. Used by `create()` to pick the
    /// initially-selected page.
    enum class Page : std::uint8_t {
        General = 0,  ///< General preferences tab.
        Theme = 1,    ///< Editor theme tab.
        Keywords = 2, ///< Keyword groups + per-group case rules tab.
        Compiler = 3, ///< Compiler paths + command templates tab.
    };

    NO_COPY_AND_MOVE(SettingsDialog)

    /// Construct without populating panels; `create()` builds them.
    SettingsDialog(wxWindow* parent, Context& ctx);
    /// Out-of-line so the destructor sees the panel definitions.
    ~SettingsDialog() override;
    /// Build the tabs and finalize layout. `target` is a slash-delimited
    /// deep-link: the first segment selects the page
    /// (`general` / `theme` / `keywords` / `compiler`), the remainder is
    /// handed to that page's `focusPath()` to select a sub-location and
    /// move focus. Empty opens the General tab with no forced focus.
    void create(const wxString& target = {});

private:
    /// Run every panel's `apply()` (active page first so its
    /// validation surfaces immediately), save dirty categories,
    /// refresh live UI. Returns `false` when any panel rejects its
    /// input — in that case the offending panel is selected and the
    /// dialog stays open.
    [[nodiscard]] auto applyChanges() -> bool;

    /// Walk every panel's `cancel()` to roll back pending edits.
    void cancelChanges() const;

    /// Resolve a `Page` enum to the underlying panel pointer.
    [[nodiscard]] auto panelAt(Page page) const -> Panel*;

    /// Map the first path segment of a deep-link target to a `Page`.
    /// Unknown / empty names fall back to `Page::General`.
    [[nodiscard]] static auto pageFromName(const wxString& name) -> Page;

    Context& m_ctx;                       ///< Application context.
    Unowned<wxNotebook> m_notebook;       ///< Tab notebook owning the panels.
    Unowned<GeneralPage> m_generalPage;   ///< General tab.
    Unowned<ThemePage> m_themePage;       ///< Theme tab.
    Unowned<KeywordsPage> m_keywordsPage; ///< Keywords tab.
    Unowned<CompilerPage> m_compilerPage; ///< Compiler tab.
};

} // namespace fbide
