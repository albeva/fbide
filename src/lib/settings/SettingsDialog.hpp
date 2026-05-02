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
    /// Build the tabs, select `initial`, and finalize layout.
    void create(Page initial = Page::General);

private:
    /// Run every panel's `apply()`, save dirty categories, refresh live UI.
    void applyChanges() const;

    Context& m_ctx;                            ///< Application context.
    Unowned<GeneralPage> m_generalPage;        ///< General tab.
    Unowned<ThemePage> m_themePage;            ///< Theme tab.
    Unowned<KeywordsPage> m_keywordsPage;      ///< Keywords tab.
    Unowned<CompilerPage> m_compilerPage;      ///< Compiler tab.
};

} // namespace fbide
