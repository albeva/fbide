/**
 * FBIde project
 */
#include "app_pch.hpp"
#include "Manager.hpp"
#include "UiManager.hpp"
#include "MainWindow.hpp"
#include "ConfigManager.hpp"


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
            
            // path to the configuration.
			auto path = GetIdePath() + "/ide/fbide.yaml";
                        
            // Load up fbide. Order in which managers are called matters!
            GetCfgMgr().Load(path);
            
            this->Exit();
            
            
            GetUiMgr().Load();
            
            // if we get here. All seems well. So show the window
            GetUiMgr().GetWindow()->Show();
            
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
        auto & sp = this->GetTraits()->GetStandardPaths();
        #ifdef __WXMSW__
            return ::wxPathOnly(sp.GetExecutablePath());
        #else
            return sp.GetResourcesDir();
        #endif
    }
    
    
    /**
     * Application is exiting
     */
    int OnExit() override
    {
        Manager::Release();
        return wxApp::OnExit();
    }
    
};

DECLARE_APP(App);
IMPLEMENT_APP(App);