//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "lib/app/Context.hpp"
#include "lib/app/CommandManager.hpp"
#include "lib/app/HelpManager.hpp"
#include "lib/compiler/CompilerManager.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/FileHistory.hpp"
#include "lib/config/Keywords.hpp"
#include "lib/config/Lang.hpp"
#include "lib/config/Theme.hpp"
#include "lib/editor/DocumentManager.hpp"
#include "lib/ui/UIManager.hpp"
using namespace fbide;

Context::Context(const wxString& binaryPath)
: m_config(std::make_unique<Config>(binaryPath))
, m_fileHistory(std::make_unique<FileHistory>())
, m_keywords(std::make_unique<Keywords>())
, m_lang(std::make_unique<Lang>())
, m_theme(std::make_unique<Theme>())
, m_uiManager(std::make_unique<UIManager>(*this))
, m_documentManager(std::make_unique<DocumentManager>(*this))
, m_compilerManager(std::make_unique<CompilerManager>(*this))
, m_helpManager(std::make_unique<HelpManager>(*this))
, m_commandManager(std::make_unique<CommandManager>(*this)) {}

Context::~Context() = default;
