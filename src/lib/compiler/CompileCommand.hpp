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

/// Builds a command line for the FreeBASIC compiler (fbc).
///
/// Replaces meta-tags in the config template:
/// - `<$fbc>`  — compiler executable path
/// - `<$file>` — source file path
class CompileCommand final {
public:
    /// Set the source file to compile.
    void setSourceFile(const wxString& path) { m_sourceFile = path; }

    /// Build the command line using context for the template and compiler path.
    [[nodiscard]] auto build(Context& ctx) const -> wxString;

    /// Build the command line from explicit template and compiler path.
    [[nodiscard]] auto build(const wxString& compileTemplate, const wxString& compiler) const -> wxString;

    /// Create a command for the given source file.
    [[nodiscard]] static auto makeDefault(const wxString& sourceFile) -> CompileCommand;

private:
    wxString m_sourceFile; ///< Source file to compile.
};

} // namespace fbide
