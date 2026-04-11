//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "App.hpp"
#include "Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Lang.hpp"
#include "lib/config/Theme.hpp"
#include "lib/ui/UIManager.hpp"

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

    // Load language
    auto& lang = m_context->getLang();
    lang.load(config.getIdePath() + "lang/" + config.language() + ".fbl");

    // Load theme
    m_context->getTheme().load(config.getIdePath() + config.themeFile());

    // Create UI
    m_context->getUIManager().createMainFrame();
    return true;
}

auto App::getFbidePath() -> wxString {
    const auto& sp = GetTraits()->GetStandardPaths();
    return wxPathOnly(sp.GetExecutablePath());
}

} // namespace fbide
