/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */

#include "Config/Config.hpp"
#include "Config/ConfigManager.hpp"
#include "Document/Document.hpp"
#include "Editor/TextDocument.hpp"
#include "Document/TypeManager.hpp"
#include "Manager.hpp"
#include "UI/CmdManager.hpp"
#include "UI/UiManager.hpp"
#include "Log/LogManager.hpp"
#include "FB/FBEditor.hpp"
#include "App.hpp"
using namespace fbide;

bool App::OnInit()
try {
    // Load the managers
    GetMgr().Load();

    // Load up fbide. Order in which managers are called matters!
    auto path = GetIdePath() / "ide" / "fbide.yaml";
    GetCfgMgr().Load(path);

    // Load UI
    auto& ui = GetUiMgr();
    ui.Load();

    // if we get here. All seems well. So show the window
    // ui.Bind(wxEVT_CLOSE_WINDOW, &App::OnClose, this);
    ui.GetWindow()->Show();

    // plain text
    auto& type = GetTypeMgr();
    type.Register<TextDocument>();

    // freebasic
    type.Register<FBEditor>();

    // default editor type
    type.BindAlias("default", FBEditor::TypeId, true);

    // done
    return true;
} catch (std::exception& e) {
    ::wxMessageBox(std::string("Failed to start fbide. Error: ") + e.what(), "Failed to start IDE");
    Manager::Release();
    return false;
}

int App::OnExit() {
    return EXIT_SUCCESS;
}

wxString App::GetIdePath() {
    auto& sp = GetTraits()->GetStandardPaths();
    return ::wxPathOnly(sp.GetExecutablePath());
}

void App::ExitFBIde() {
    Manager::Release();
}

IMPLEMENT_APP(fbide::App) // NOLINT
