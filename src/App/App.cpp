/**
 * FBIde project
 */

#include "Config/Config.hpp"
#include "Config/ConfigManager.hpp"
#include "Document/Document.hpp"
#include "Editor/TextDocument.hpp"
#include "Document/TypeManager.hpp"
#include "Manager.hpp"
#include "UI/CmdManager.hpp"
#include "UI/MainWindow.hpp"
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
