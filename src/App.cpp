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
            // path to the configuration
            auto & sp = this->GetTraits()->GetStandardPaths();
            auto path = sp.GetResourcesDir() + "/fbide.yaml";
            
            // bootstrap things
            GetMgr();
            GetCfgMgr().Load(path);
            GetUiMgr().GetWindow()->Show();
            
            // done
            return true;
        } catch (std::exception & e) {
            std::cout << "Failed to start fbide. Error: " << e.what() << std::endl;
            Manager::Release();
            return false;
        }
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