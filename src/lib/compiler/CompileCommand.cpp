//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompileCommand.hpp"
#include "QuoteUtils.hpp"
#include "app/Context.hpp"
#include "config/Config.hpp"
#include "config/ConfigManager.hpp"
using namespace fbide;

auto CompileCommand::build(Context& ctx) const -> wxString {
    const auto& compiler = ctx.getConfigManager().config().at("compiler");
    const wxString compileTemplate = compiler.get_or("compileCommand", R"("<$fbc>" "<$file>")");
    const wxString compilerPath = compiler.get_or("path", "");
    wxFileName path(compilerPath);
    path.MakeAbsolute(ctx.getConfig().getAppPath());
    return build(compileTemplate, path.GetFullPath());
}

auto CompileCommand::build(const wxString& compileTemplate, const wxString& compiler) const -> wxString {
    auto cmd = compileTemplate;

    cmd.Replace("<$fbc>", escapeQuotes(compiler));
    cmd.Replace("<$file>", escapeQuotes(m_sourceFile));

    return cmd.Strip(wxString::both);
}

auto CompileCommand::makeDefault(const wxString& sourceFile) -> CompileCommand {
    CompileCommand cmd;
    cmd.setSourceFile(sourceFile);
    return cmd;
}
