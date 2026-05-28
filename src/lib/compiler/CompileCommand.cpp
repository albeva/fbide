//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompileCommand.hpp"
#include "CompilerConfigCatalog.hpp"
#include "QuoteUtils.hpp"
#include "config/ConfigManager.hpp"
#include "utils/PathConversions.hpp"
using namespace fbide;

auto CompileCommand::build(const ResolvedCompilerConfig& cfg, const ConfigManager& cm) const -> wxString {
    wxFileName path { toWxString(cfg.path) };
    path.MakeAbsolute(cm.getAppDir());
    return build(cfg.compileCommand, path.GetFullPath());
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
