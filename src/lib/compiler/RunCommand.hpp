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

/// Builds a run command line from a config template and an executable path.
///
/// Replaces meta-tags in the template:
///   <$file>      — full executable path (quoted)
///   <$file_path> — directory containing the executable
///   <$file_name> — executable name without extension
///   <$file_ext>  — executable extension
///   <$param>     — user-supplied parameters
///   <$terminal>  — terminal emulator command
class RunCommand final {
public:
    /// Set the executable path.
    void setExecutable(const wxString& path) { m_executable = path; }

    /// Build the complete command line string using context for config values.
    [[nodiscard]] auto build(Context& ctx) const -> wxString;

    /// Build the complete command line string from explicit values.
    [[nodiscard]] auto build(const wxString& runTemplate, const wxString& terminal = {}, const wxString& parameters = {}) const -> wxString;

    /// Create a run command for the given executable.
    [[nodiscard]] static auto makeDefault(const wxString& executablePath) -> RunCommand;

private:
    wxString m_executable;
};

} // namespace fbide
