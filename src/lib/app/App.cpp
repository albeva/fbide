//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "App.hpp"
#include <wx/clipbrd.h>
#include "Context.hpp"
#include "InstanceHandler.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "editor/DocumentManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

auto App::OnExit() -> int {
    // Flush clipboard so text copied from fbide (e.g. Format dialog "Copy"
    // output) remains available in other applications after we close.
    if (wxTheClipboard->Open()) {
        wxTheClipboard->Flush();
        wxTheClipboard->Close();
    }
    return wxApp::OnExit();
}

auto App::OnInit() -> bool {
    const auto fbidePath = getFbidePath();
    wxLog::SetActiveTarget(new wxLogStream(new std::ofstream((fbidePath / "app.log").ToStdString(), std::ios::app)));

    // Create context and parse arguments early (before IPC check)
    m_context = std::make_unique<Context>(fbidePath);

    wxString configFile;
    wxArrayString filesToOpen;
    parseArgs(configFile, filesToOpen);

    // Single instance: if another FBIde is running, forward files and exit
    if (!m_newWindow) {
        m_instanceHandler = std::make_unique<InstanceHandler>(*m_context);
        if (m_instanceHandler->isAnotherRunning()) {
            m_instanceHandler->sendFiles(filesToOpen);
            return false;
        }
    }

    showSplash();

    // If --config was supplied, reload ConfigManager from that file.
    auto& configManager = m_context->getConfigManager();
    if (not configFile.IsEmpty()) {
        configManager.reloadConfig(configFile);
    }

    m_context->getFileHistory().load(configManager.getIdeDir() + "history.ini");

    m_context->getUIManager().createMainFrame();
    openFiles(filesToOpen);
    return true;
}

void App::parseArgs(wxString& configFile, wxArrayString& filesToOpen) {
    const auto& configManager = m_context->getConfigManager();
    auto args = argv.GetArguments();

    for (size_t index = 1; index < args.GetCount(); index++) {
        if (const auto& arg = args[index]; arg == "--config") {
            index += 1;
            if (index >= args.GetCount()) {
                wxLogError("--config requires a path argument");
                continue;
            }
            configFile = configManager.absolute(args[index]);
        } else if (arg == "--new-window") {
            m_newWindow = true;
        } else if (arg == "--verbose") {
            wxLog::SetVerbose(true);
        } else if (!arg.StartsWith("-")) {
            filesToOpen.Add(arg);
        }
    }
}

auto App::getFbidePath() -> wxString {
    const auto& sp = GetTraits()->GetStandardPaths();
    return wxPathOnly(sp.GetExecutablePath());
}

void App::openFiles(const wxArrayString& files) {
    auto& docManager = m_context->getDocumentManager();
    for (const auto& file : files) {
        docManager.openFile(file);
    }
}

void App::showSplash() {
    if (m_context->getConfigManager().config().get_or("general.splashScreen", true)) {
        wxImage::AddHandler(make_unowned<wxPNGHandler>());
        const auto splashPath = m_context->getConfigManager().absolute("splash.png");
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
