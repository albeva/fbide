//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompileCommand.hpp"
using namespace fbide;

namespace {

/// Escape unescaped double quotes in a string.
auto escapeQuotes(const wxString& str) -> wxString {
    wxString escaped;
    escaped.reserve(str.length());
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '"' && (i == 0 || str[i - 1] != '\\')) {
            escaped += '\\';
        }
        escaped += str[i];
    }
    return escaped;
}

void quote(wxString& cmd, const wxString& val) {
    const auto trimmed = val.Strip(wxString::both);
    if (trimmed.StartsWith('"') && trimmed.EndsWith('"')) {
        const auto inner = trimmed.Mid(1, trimmed.length() - 2);
        cmd += "\"" + escapeQuotes(inner) + "\"";
    } else {
        cmd += "\"" + escapeQuotes(trimmed) + "\"";
    }
}
} // namespace

auto CompileCommand::build() const -> wxString {
    wxString cmd;

    // Compiler path
    quote(cmd, m_compiler);

    // Extra flags
    for (const auto& [flag, value] : m_extras) {
        cmd += " -" + flag;
        if (!value.empty()) {
            cmd += " " + value;
        }
    }

    // Source files
    for (const auto& file : m_files) {
        cmd += " ";
        quote(cmd, file);
    }

    return cmd;
}

auto CompileCommand::makeDefault(const wxString& compiler, const wxString& sourceFile) -> CompileCommand {
    CompileCommand cmd;
    cmd.setCompiler(compiler);
    cmd.addFile(sourceFile);
    return cmd;
}
