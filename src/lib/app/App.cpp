//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "App.hpp"
#include "Context.hpp"
#include "lib/config/Config.hpp"

namespace fbide {

auto App::OnInit() -> bool {
    // Create context
    m_context = std::make_unique<Context>(getFbidePath());
    auto& config = m_context->getConfig();

    // Load config
    auto args = argv.GetArguments();
    wxString configFile;
    for (size_t index = 0; index < args.GetCount(); index++) {
        if (const auto& arg = args[index]; arg == "--config") {
            index += 1;
            if (index >= args.GetCount()) {
                // TODO: LOG_ERROR("--config requires path");
                return false;
            }
            configFile = config.resolvePath(args[index]);
        } else if (arg == "--verbose") {
            wxLog::SetVerbose(true);
        }
    }
    if (configFile.IsEmpty()) {
        configFile = config.getIdePath() + config.getDefaultConfigFileName();
    }
    config.load(configFile);

    // create frame
    const auto frame = make_unowned<wxFrame>(nullptr, wxID_ANY, "FBIde");
    frame->Show();
    return true;
}

auto App::getFbidePath() -> wxString {
    const auto& sp = GetTraits()->GetStandardPaths();
    return wxPathOnly(sp.GetExecutablePath());
}

} // namespace fbide
