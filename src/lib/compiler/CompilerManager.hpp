//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class BuildTask;
class CompilerConfigCatalog;
class Document;
class Context;

/**
 * Owns FBIde's relationship with `fbc`: the compile / run / quickrun
 * commands, the compiler probe, the runtime parameters, and the
 * single in-flight `BuildTask`.
 *
 * **Owns:** the current `m_task` (`unique_ptr<BuildTask>`), cached
 * `m_fbcVersion`, and runtime `m_parameters`.
 * **Owned by:** `Context`.
 * **Threading:** UI thread only. `BuildTask` spawns `AsyncProcess`,
 * which is async with a UI-thread callback — there is no
 * compiler-side worker.
 * **Single in-flight:** replacing `m_task` deletes the previous task
 * and aborts any process it had spawned. Two builds cannot race.
 *
 * See @ref compiler.
 */
class CompilerManager final {
public:
    NO_COPY_AND_MOVE(CompilerManager)

    /// Construct without probing the compiler.
    explicit CompilerManager(Context& ctx);
    /// Out-of-line so the destructor sees the full `BuildTask` definition.
    ~CompilerManager();

    /// Compile the active document.
    void compile();

    /// Compile and run the active document.
    void compileAndRun();

    /// Run the previously compiled executable (or offer to compile first).
    void run();

    /// Compile active document as temp file, run, then clean up.
    void quickRun();

    /// Kill active compile or run task
    void killProcess();

    /// Show compiler log dialog with full output.
    void showCompilerLog();

    /// Refresh the compiler log dialog if it exists.
    void refreshCompilerLog();

    /// Navigate to an error by line number and file name.
    void goToError(int line, const wxString& fileName);

    /// Get the fbc version string. Validates the compiler path, runs `fbc --version`,
    /// and caches the result. Returns empty string if compiler is not accessible.
    [[nodiscard]] auto getFbcVersion() -> const wxString&;

    /// Resolve `compiler.path` against the IDE's appDir and verify the
    /// binary exists/is executable. Returns the resolved absolute path,
    /// or empty when the path is unset or missing.
    [[nodiscard]] auto resolveCompilerBinary() const -> wxString;

    /// Startup probe: when the configured compiler binary is missing,
    /// surface a wxRichMessageDialog with a "Don't show again" checkbox.
    /// "Yes" opens the Settings dialog focused on the Compiler tab; the
    /// checkbox toggles `alerts.ignore.missingCompilerBinary` so future
    /// launches stay silent. No-op when the binary is reachable or the
    /// ignore flag is set. Call once after the main frame is created.
    void checkCompilerOnStartup();

    /// Show the "compiler not found" prompt without the silence checkbox
    /// (used by the build flow when the user explicitly invokes compile/
    /// run): the alert is always relevant because the user just asked for
    /// the compiler. "Yes" opens the Settings dialog on the Compiler tab.
    void promptMissingCompiler();

    /// Reset the cached fbc version. Call when compiler path may have changed.
    void resetFbcVersion() { m_fbcVersion.clear(); }

    /// Get runtime parameters for the executable.
    [[nodiscard]] auto getParameters() const -> const wxString& { return m_parameters; }

    /// Set runtime parameters (from the parameters dialog).
    void setParameters(const wxString& params) { m_parameters = params; }

    /// Catalog of available compiler configurations (canonical Default
    /// plus any user-defined `[compiler/*]` sections).
    [[nodiscard]] auto catalog() -> CompilerConfigCatalog& { return *m_catalog; }
    [[nodiscard]] auto catalog() const -> const CompilerConfigCatalog& { return *m_catalog; }

    /// Apply the "matches active → empty" normalisation and store the
    /// resulting optional on the document. The combobox event handler
    /// is the only caller.
    void setDocumentConfiguration(Document& doc, const wxString& pickedSlug);

    /// Create the toolbar configuration combobox. Called once by
    /// `UIManager::configureToolBar` when it sees the reserved
    /// `CommandId::Configuration` entry. Toolbar takes ownership.
    [[nodiscard]] auto createConfigurationCombo(wxAuiToolBar* parent) -> wxComboBox*;

    /// Re-populate the combobox from the catalog. Call after a settings
    /// dialog OK that mutated the catalog.
    void refreshConfigurationCombo();

    /// Notify the manager that the active document changed (or there
    /// is no active document — pass `nullptr`). Updates the combobox
    /// selection and enabled state.
    void onActiveDocumentChanged(Document* doc);

    /// Show or hide the toolbar combobox — used when the user toggles
    /// the "configuration in status bar" preference at runtime.
    void setConfigurationComboVisible(bool visible);

    /// Resolve the display label for the active document's
    /// configuration (empty when the active document isn't a
    /// FreeBASIC source — the status-bar selector hides in that case).
    [[nodiscard]] auto configurationStatusLabel() const -> wxString;

    /// Build a popup menu listing every catalog entry as a radio item
    /// (current one checked). Used by the status-bar selector.
    [[nodiscard]] auto buildConfigurationMenu() const -> std::unique_ptr<wxMenu>;

    /// Apply a status-bar menu selection — same normalisation as the
    /// toolbar combobox path.
    void applyConfigurationMenuSelection(int menuId);

    /// First menu-item ID reserved for the status-bar configuration
    /// popup. Each catalog entry's index is added to the base — used
    /// by `buildConfigurationMenu` / `applyConfigurationMenuSelection`
    /// and by `UIManager::onStatusBarClick`.
    static constexpr int kStatusMenuIdBase = wxID_HIGHEST + 10500;

private:
    /// Get active FreeBASIC document, or nullptr if unavailable.
    [[nodiscard]] auto getActiveDocument() -> Document*;

    /// Ensure document is saved. Returns false if user cancelled.
    auto ensureSaved(Document& doc) -> bool;

    /// Rebuild the combobox display names from the current catalog
    /// state. Item order mirrors `catalog().all()`, so a selection index
    /// maps straight back through `catalog().at()`.
    void populateConfigurationCombo();

    /// React to the user picking an entry in the toolbar combobox —
    /// apply the normalisation and store on the active document.
    void onConfigurationComboSelected();

    /// Mirror the resolved configuration label into the status-bar
    /// field, when the configuration status-bar layout is active.
    void pushStatusBarLabel();

    Context& m_ctx;                                   ///< Application context.
    std::unique_ptr<CompilerConfigCatalog> m_catalog; ///< Resolved view of `[compiler]` + `[compiler/*]`.
    std::unique_ptr<BuildTask> m_task;                ///< In-flight task (`nullptr` when idle).
    wxString m_parameters;                            ///< Runtime parameters set via the Parameters dialog.
    wxString m_fbcVersion;                            ///< Cached `fbc --version` output (empty until probed).
    wxComboBox* m_configCombo = nullptr;              ///< Toolbar-owned widget; non-null after configureToolBar.
    Document* m_lastActiveDoc = nullptr;              ///< Last document the combobox was synced to.
};

} // namespace fbide
