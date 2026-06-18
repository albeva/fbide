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

auto CompileCommand::tokenizeTemplate(const wxString& compileTemplate) -> std::vector<wxString> {
    // Respect double quotes (stripped from the resulting tokens); whitespace
    // outside quotes separates tokens — the way a shell, and fbc's own argument
    // parsing, see it.
    std::vector<wxString> tokens;
    wxString current;
    bool inQuotes = false;
    bool hasToken = false;
    const auto flush = [&] {
        if (hasToken) {
            tokens.push_back(current);
            current.clear();
            hasToken = false;
        }
    };
    for (const auto ch : compileTemplate) {
        if (ch == '"') {
            inQuotes = !inQuotes;
            hasToken = true; // an opening quote begins a token, even if empty
        } else if (!inQuotes && (ch == ' ' || ch == '\t')) {
            flush();
        } else {
            current += ch;
            hasToken = true;
        }
    }
    flush();
    return tokens;
}

auto CompileCommand::extractIncludePaths(const wxString& compileTemplate) -> std::vector<wxString> {
    const auto tokens = tokenizeTemplate(compileTemplate);

    // fbc takes the include directory as the argument *after* a standalone
    // `-i` (the glued `-i<path>` form is not valid fbc, so it isn't parsed).
    // A trailing `-i` with no following token is ignored.
    std::vector<wxString> paths;
    for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
        if (tokens[i] == "-i" && !tokens[i + 1].empty()) {
            paths.push_back(tokens[i + 1]);
            ++i; // consume the path token
        }
    }
    return paths;
}

auto CompileCommand::extractDefines(const wxString& compileTemplate) -> std::vector<wxString> {
    const auto tokens = tokenizeTemplate(compileTemplate);

    // Like `-i`, fbc takes the definition as the argument *after* a standalone
    // `-d`. The definition is `name` or `name=value`; only the name is relevant
    // to `#ifdef`/`defined()`, so any `=value` suffix is dropped.
    std::vector<wxString> defines;
    for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
        if (tokens[i] == "-d" && !tokens[i + 1].empty()) {
            if (const wxString name = tokens[i + 1].BeforeFirst('='); !name.empty()) {
                defines.push_back(name);
            }
            ++i; // consume the definition token
        }
    }
    return defines;
}
