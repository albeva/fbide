//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompileCommand.hpp"
using namespace fbide;

void CompileCommand::quote(wxString& cmd, const wxString& val) {
    cmd += " \"" + val + "\"";
}

auto CompileCommand::build() const -> wxString {
    wxString cmd;

    // Compiler path
    cmd += "\"" + m_compiler + "\"";

    // Extra flags
    for (const auto& [flag, value] : m_extras) {
        cmd += " -" + flag;
        if (!value.empty()) {
            cmd += " " + value;
        }
    }

    // Source files
    for (const auto& file : m_files) {
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
