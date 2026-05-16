//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "app/Context.hpp"
#include "ui/controls/Panel.hpp"

namespace fbide {

/// Compiler settings tab — paths and command prototypes.
class CompilerPage final : public Panel {
public:
    NO_COPY_AND_MOVE(CompilerPage)

    /// Construct without populating widgets; `create()` builds the UI.
    explicit CompilerPage(Context& ctx, wxWindow* parent);
    /// Build the panel widgets.
    void create() override;
    /// Commit edits back into `ConfigManager`.
    void apply() override;

    /// Move keyboard focus to the compiler path entry. Used when the
    /// dialog is opened from the startup compiler-missing prompt so the
    /// user can start typing the path immediately.
    void focusCompilerPath();

private:
    /// Locale lookup with empty default — sugar over `ConfigManager::locale().get_or`.
    auto tr(const wxString& path) const -> wxString {
        return getContext().getConfigManager().locale().get_or(path, "");
    }

    /// Build the compiler-path file picker row.
    void compilerPath();
    /// Build the compile-command template entry row.
    void compilerCommand();
    /// Build the run-command template entry row.
    void runCommand();
#ifdef __WXMSW__
    /// Build the CHM help-file path picker row (Windows only).
    void helpFile();
#endif
    /// Build the placeholder reference list (click-to-insert).
    void placeholderTable();
    /// Repopulate the placeholder list for the currently-focused command field.
    void refreshPlaceholders();
    /// Insert `placeholder` at the cursor of the most recently focused command field.
    void insertPlaceholder(const wxString& placeholder);
    /// Show or hide the placeholder list + its title in unison.
    void setPlaceholderVisible(bool visible);
    /// Path used as `<$file>` example — active FB document if any, otherwise a fixed sample.
    [[nodiscard]] auto getSampleSourcePath() const -> wxString;

    /// Create a labelled text-entry field bound to `value`.
    auto makeEntryField(wxString& value, const wxString& labelText) -> Unowned<wxTextCtrl>;
    /// Create a labelled text-entry + Browse button bound to `value`.
    auto makeFileEntry(wxString& value, const wxString& labelText) -> std::pair<Unowned<wxTextCtrl>, Unowned<wxButton>>;

    wxString m_compilerPath;   ///< `compiler.path` value.
    wxString m_compileCommand; ///< `compiler.compile` template.
    wxString m_runCommand;     ///< `compiler.run` template.
#ifdef __WXMSW__
    wxString m_helpFile; ///< CHM help-file path (Windows only).
#endif
    Unowned<wxTextCtrl> m_compilerPathField {};   ///< Cached compiler-path entry for `focusCompilerPath`.
    Unowned<wxTextCtrl> m_compileCommandField {}; ///< Cached compile-command entry (target of placeholder inserts).
    Unowned<wxTextCtrl> m_runCommandField {};     ///< Cached run-command entry (target of placeholder inserts).
    Unowned<wxStaticText> m_placeholderTitle {};  ///< Heading shown above `m_placeholderList`; hidden together.
    Unowned<wxListCtrl> m_placeholderList {};     ///< Click-to-insert placeholder reference table.
    wxTextCtrl* m_lastFocused = nullptr;          ///< Most recently focused command field (insert target).
};

} // namespace fbide
