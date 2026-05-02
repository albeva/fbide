//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "app/Context.hpp"
#include "command/CommandManager.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "document/DocumentManager.hpp"
#include "document/FileSession.hpp"
#include "help/HelpManager.hpp"
#include "sidebar/SideBarManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

Context::Context(App& app, const wxString& binaryPath, const wxString& idePath, const wxString& configPath)
: m_app(app)
, m_configManager(std::make_unique<ConfigManager>(binaryPath, idePath, configPath))
, m_fileHistory(std::make_unique<FileHistory>())
, m_uiManager(std::make_unique<UIManager>(*this))
, m_sideBarManager(std::make_unique<SideBarManager>(*this))
, m_documentManager(std::make_unique<DocumentManager>(*this))
, m_fileSession(std::make_unique<FileSession>(*this))
, m_compilerManager(std::make_unique<CompilerManager>(*this))
, m_helpManager(std::make_unique<HelpManager>(*this))
, m_commandManager(std::make_unique<CommandManager>(*this)) {}

Context::~Context() = default;

auto Context::tr(const wxString& path) -> wxString {
    return m_configManager->locale().get_or(path, "");
}
