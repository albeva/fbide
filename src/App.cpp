/**
 * FBIde project
 */
#include "app_pch.h"
#include "Manager.h"

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
        GetMgr().Load();
		return true;
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