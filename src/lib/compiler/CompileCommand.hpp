//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class ConfigManager;
struct ResolvedCompilerConfig;

/// Builds a command line for the FreeBASIC compiler (fbc).
///
/// Replaces meta-tags in the config template:
/// - `<$fbc>`  — compiler executable path
/// - `<$file>` — source file path
class CompileCommand final {
public:
    /// Set the source file to compile.
    void setSourceFile(const wxString& path) { m_sourceFile = path; }

    /// Build the command line for a resolved configuration. The
    /// compiler path inside `cfg` is resolved against the IDE's
    /// app directory before substitution.
    [[nodiscard]] auto build(const ResolvedCompilerConfig& cfg, const ConfigManager& cm) const -> wxString;

    /// Build the command line from explicit template and compiler path.
    [[nodiscard]] auto build(const wxString& compileTemplate, const wxString& compiler) const -> wxString;

    /// Create a command for the given source file.
    [[nodiscard]] static auto makeDefault(const wxString& sourceFile) -> CompileCommand;

    /// Extract the directories passed to fbc via `-i <path>` in a compile
    /// command template, in order. Mirrors fbc's syntax: `-i` is a
    /// standalone argument followed by the path, which may be double-quoted
    /// to embrace spaces. Meta-tags (`<$fbc>` etc.) and other flags are
    /// ignored — only literal `-i` paths come back. Lets `#include`
    /// navigation search the same directories the compiler does.
    [[nodiscard]] static auto extractIncludePaths(const wxString& compileTemplate) -> std::vector<wxString>;

private:
    wxString m_sourceFile; ///< Source file to compile.
};

} // namespace fbide
