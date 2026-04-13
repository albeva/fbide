//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "RunCommand.hpp"
#include "CompilerManager.hpp"
#include "QuoteUtils.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
using namespace fbide;

auto RunCommand::build(const Context& ctx) const -> wxString {
    return build(
        ctx.getConfig().getRunCommand(),
        Config::getTerminal(),
        ctx.getCompilerManager().getParameters()
    );
}

auto RunCommand::build(const wxString& runTemplate, const wxString& terminal, const wxString& parameters) const -> wxString {
    const wxFileName file(m_executable);
    auto cmd = runTemplate;

    cmd.Replace("<$file>", escapeQuotes(file.GetFullPath()));
    cmd.Replace("<$file_path>", escapeQuotes(file.GetPath()));
    cmd.Replace("<$file_name>", escapeQuotes(file.GetName()));
    cmd.Replace("<$file_ext>", escapeQuotes(file.GetExt()));
    cmd.Replace("<$param>", parameters);
    cmd.Replace("<$terminal>", terminal);

    return cmd.Strip(wxString::both);
}

auto RunCommand::makeDefault(const wxString& executablePath) -> RunCommand {
    RunCommand cmd;
    cmd.setExecutable(executablePath);
    return cmd;
}
