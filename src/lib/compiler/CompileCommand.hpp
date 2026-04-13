//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <map>

namespace fbide {

/// Builds a command line for the FreeBASIC compiler (fbc).
///
/// Common flags are exposed as typed attributes.
/// Less common flags can be set via the extras map.
class CompileCommand final {
public:
    /// Set the path to the fbc executable.
    void setCompiler(const wxString& path) { m_compiler = path; }
    [[nodiscard]] auto getCompiler() const -> const wxString& { return m_compiler; }

    /// Add a source file to compile.
    void addFile(const wxString& path) { m_files.push_back(path); }
    [[nodiscard]] auto getFiles() const -> const std::vector<wxString>& { return m_files; }

    /// Set an extra flag. Key is the flag name without leading dash.
    /// Value is the argument, or empty for boolean flags (e.g. -v).
    void setExtra(const wxString& flag, const wxString& value = {}) { m_extras[flag] = value; }
    [[nodiscard]] auto getExtras() const -> const std::map<wxString, wxString>& { return m_extras; }

    /// Build the complete command line string.
    [[nodiscard]] auto build() const -> wxString;

    /// Create a default command suitable for quick runs.
    [[nodiscard]] static auto makeDefault(const wxString& compiler, const wxString& sourceFile) -> CompileCommand;

private:

    wxString m_compiler;
    std::vector<wxString> m_files;
    std::map<wxString, wxString> m_extras;
};

} // namespace fbide
