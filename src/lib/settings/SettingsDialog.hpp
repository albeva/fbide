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
    SettingsDialog(wxWindow* parent, Context& ctx);
    ~SettingsDialog() override;
    void create();

private:
    void applyChanges() const;

    Context& m_ctx;
    Unowned<GeneralPage> m_generalPage;
    Unowned<ThemePage> m_themePage;
    Unowned<KeywordsPage> m_keywordsPage;
    Unowned<CompilerPage> m_compilerPage;
};

} // namespace fbide
