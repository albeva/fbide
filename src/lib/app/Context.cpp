//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "app/Context.hpp"
#include "command/CommandManager.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/Config.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "config/Theme.hpp"
#include "editor/DocumentManager.hpp"
#include "help/HelpManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

Context::Context(const wxString& binaryPath)
: m_config(std::make_unique<Config>(binaryPath))
, m_configManager(std::make_unique<ConfigManager>(binaryPath))
, m_fileHistory(std::make_unique<FileHistory>())
, m_theme(std::make_unique<Theme>())
, m_uiManager(std::make_unique<UIManager>(*this))
, m_documentManager(std::make_unique<DocumentManager>(*this))
, m_compilerManager(std::make_unique<CompilerManager>(*this))
, m_helpManager(std::make_unique<HelpManager>(*this))
, m_commandManager(std::make_unique<CommandManager>(*this)) {}

Context::~Context() = default;

auto Context::tr(const wxString& path) -> wxString {
    return m_configManager->locale().get_or(path, "");
}
