/**
 * FBIde project
 */

#include "Manager.hpp"
#include "UiManager.hpp"
#include "MainWindow.hpp"
#include "ConfigManager.hpp"
#include "CmdManager.hpp"
#include "Document.hpp"
#include "TypeManager.hpp"
#include "StyledEditor.hpp"
#include "EditorDocument.hpp"
#include "FBLexer.hpp"

using namespace fbide;


/**
 * App is the basic entry point into FBIde
 */
class App : public wxApp
{
public:

    /**
     * Application started
     */
	bool OnInit() override
	{
        try {
            // Load the managers
            GetMgr().Load();
                        
            // Load up fbide. Order in which managers are called matters!
            auto path = GetIdePath() + "/ide/fbide.yaml";
            GetCfgMgr().Load(path);
            
            // Load UI
            auto & ui = GetUiMgr();
            ui.Load();
            
            // if we get here. All seems well. So show the window
            ui.Bind(wxEVT_CLOSE_WINDOW, &App::OnClose, this);
            ui.GetWindow()->Show();
            
            // plain text document
            auto & type = GetTypeMgr();
            type.Register<EditorDocument>("text/plain", {"txt", "", "md"});
            
            auto v = wxStyledTextCtrl::GetLibraryVersionInfo();
            std::cout <<v.GetDescription() << '\n';
            
            //setup with scintilla
            LexerFreeBasic::SetupLexer();
            
            // done
            return true;
        } catch (std::exception & e) {
            ::wxMessageBox(std::string("Failed to start fbide. Error: ") + e.what(), "Failed to start IDE");
            Manager::Release();
            return false;
        }
	}
    
    
    /**
     * find the path where fbide settings are stored
     */
    wxString GetIdePath()
    {
        auto & sp = GetTraits()->GetStandardPaths();
        #ifdef __WXMSW__
            return ::wxPathOnly(sp.GetExecutablePath());
        #else
            return sp.GetResourcesDir();
        #endif
    }
    
    
    /**
     * Handle application shutdown.
     */
    void CloseApp()
    {
        Manager::Release();
    }
    
    
    /**
     * Attempting to quit application (via menu)
     */
    void OnExit(wxCommandEvent & event)
    {
        CloseApp();
    }
    
    
    /**
     * Attempting to quit application (close window, etc.)
     */
    void OnClose(wxCloseEvent & close)
    {
        CloseApp();
    }
    
    DECLARE_EVENT_TABLE()
};

// App wide events

wxBEGIN_EVENT_TABLE(App, wxApp)
    EVT_MENU(wxID_EXIT, App::OnExit)
    EVT_CLOSE(App::OnClose)
wxEND_EVENT_TABLE()

// wxWidgets machinery

DECLARE_APP(App);
IMPLEMENT_APP(App);
