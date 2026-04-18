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
    auto& cfg = ctx.getConfigManager();
    const auto compileTemplate = cfg.read_or("compiler.compileCommand", std::string { R"("<$fbc>" "<$file>")" });
    const auto compilerPath = cfg.read_or("compiler.path", std::string {});
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
