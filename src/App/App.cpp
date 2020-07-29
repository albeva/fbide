/**
 * FBIde project
 */

#include "Config/Config.hpp"
#include "Config/ConfigManager.hpp"
#include "Editor/Document.hpp"
#include "Editor/TextDocument.hpp"
#include "Editor/TypeManager.hpp"
#include "Manager.hpp"
#include "UI/CmdManager.hpp"
#include "UI/MainWindow.hpp"
#include "UI/UiManager.hpp"
#include "Log/LogManager.hpp"
#include "FB/FBEditor.hpp"
using namespace fbide;

/**
 * App is the basic entry point into FBIde
 */
class App final : public wxApp {
public:

    bool OnInit() final
    try {
        // Load the managers
        GetMgr().Load();

        // Load up fbide. Order in which managers are called matters!
        auto path = GetIdePath() / "ide" / "fbide.yaml";
        GetCfgMgr().Load(path);

        // Load UI
        auto& ui = GetUiMgr();
        ui.Load();

        // Load scintilla lexer for fb
        LoadScintillaFBLexer();

        // if we get here. All seems well. So show the window
        ui.Bind(wxEVT_CLOSE_WINDOW, &App::OnClose, this);
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

    int OnExit() final {
        return EXIT_SUCCESS;
    }

    void LoadScintillaFBLexer() {
        // we need to create instance of wxStyledTextCtrl in order to
        // tell scintilla to load the dynamic library.

        auto wnd = GetUiMgr().GetWindow();
        wxWindowUpdateLocker lock(wnd);
        auto stc = new wxStyledTextCtrl(wnd);

        #if defined(__DARWIN__)
            auto path = GetConfig(Key::BasePath).AsString() / "libfblexer.dylib";
        #elif defined(__WXMSW__)
            auto path = GetConfig(Key::IdePath).AsString() / "fblexer.dll";
        #endif

        stc->LoadLexerLibrary(path);
        delete stc;
    }

    wxString GetIdePath() {
        auto& sp = GetTraits()->GetStandardPaths();
        #ifdef __WXMSW__
            return ::wxPathOnly(sp.GetExecutablePath());
        #else
            return sp.GetResourcesDir();
        #endif
    }

    void CloseApp() {
        Manager::Release();
    }

    void OnExit(wxCommandEvent& event) {
        CloseApp();
    }

    void OnClose(wxCloseEvent& close) {
        close.Veto();
        CloseApp();
    }

    DECLARE_EVENT_TABLE()
};

// App wide events

wxBEGIN_EVENT_TABLE(App, wxApp)
    EVT_MENU(wxID_EXIT, App::OnExit)
    EVT_CLOSE(App::OnClose)
wxEND_EVENT_TABLE()

DECLARE_APP(App)
IMPLEMENT_APP(App)
