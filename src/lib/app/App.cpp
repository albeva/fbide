//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "App.hpp"
#include "Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/FileHistory.hpp"
#include "lib/config/Keywords.hpp"
#include "lib/config/Lang.hpp"
#include "lib/config/Theme.hpp"
#include "lib/ui/UIManager.hpp"
using namespace fbide;

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
        configFile = config.getAppSettingsPath() + config.getPlatformConfigFileName();
    }
    config.load(configFile);

    // Load language
    auto& lang = m_context->getLang();
    lang.load(config.getAppSettingsPath() + "lang/" + config.getLanguage() + ".fbl");

    // Load keywords, theme, and file history
    m_context->getKeywords().load(config.resolvePath(config.getSyntaxFile()));
    m_context->getTheme().load(config.getThemePath());
    m_context->getFileHistory().load(config.getAppSettingsPath() + "history.ini");

    showSplash();

    m_context->getUIManager().createMainFrame();
    return true;
}

auto App::getFbidePath() -> wxString {
    const auto& sp = GetTraits()->GetStandardPaths();
    return wxPathOnly(sp.GetExecutablePath());
}

void App::showSplash() {
    if (m_context->getConfig().getSplashScreen()) {
        wxInitAllImageHandlers();
        const auto splashPath = m_context->getConfig().resolvePath("splash.png");
        if (const wxBitmap bmp(splashPath, wxBITMAP_TYPE_PNG); bmp.IsOk()) {
            make_unowned<wxSplashScreen>(
                bmp,
                wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_TIMEOUT,
                1000, nullptr, wxID_ANY
            );
            wxYield();
        }
    }
}
