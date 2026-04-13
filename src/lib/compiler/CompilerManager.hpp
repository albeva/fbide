//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "lib/config/LangId.hpp"

namespace fbide {
class BuildTask;
class Document;
class Context;

/// Manages compiler interaction: validates documents, resolves
/// the compiler, and delegates to BuildTask for execution.
class CompilerManager final {
public:
    explicit CompilerManager(Context& ctx);
    ~CompilerManager();

    /// Compile the active document.
    void compile();

    /// Compile and run the active document.
    void compileAndRun();

    /// Run the previously compiled executable (or offer to compile first).
    void run();

    /// Compile active document as temp file, run, then clean up.
    void quickRun();

    /// Show compiler log dialog with full output.
    void showCompilerLog();

    /// Navigate to an error by line number and file name.
    void goToError(int line, const wxString& fileName);

private:
    /// Get active FreeBASIC document, or nullptr if unavailable.
    [[nodiscard]] auto getActiveDocument() -> Document*;

    /// Ensure document is saved. Returns false if user cancelled.
    auto ensureSaved(Document& doc) -> bool;

    /// Set status bar text.
    void setStatus(LangId id) const;

    Context& m_ctx;
    std::unique_ptr<BuildTask> m_task;
};

} // namespace fbide
