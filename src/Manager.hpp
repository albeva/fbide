/**
 * FBIde project
 */
#pragma once

namespace fbide {
    
    class ManagerBase;
    class Manager;
    class UiManager;
    class ConfigManager;
    
    /**
     * Get manager instance.
     *
     * This is shorthand for Manager::GetInstance()
     */
    Manager & GetMgr();
    
    /**
     * Get UI manager
     *
     * This is shorthand for Manager::GetInstance().GetUiManager()
     */
    UiManager & GetUiMgr();
    
    /**
     * Get configuration manager
     *
     * This is shorthand for Manager::GetInstance().GetConfigManager()
     */
    ConfigManager & GetCfgMgr();
    

    /**
     * Main manager class. This is aproxy class that holds
     * the instances and other bookkeeping of the SDK
     * and should be used to access the SDK API
     *
     * This class is a singleton
     */
    class Manager : private NonCopyable
    {
    public:
        
        // Get manager instance
        static Manager & GetInstance();
        
        // clean up
        static void Release();
        
        // Get UI manager
        UiManager & GetUiManager();
        
        // Get config manager
        ConfigManager & GetConfigManager();
        
    private:
        
        std::unique_ptr<UiManager> m_ui;
        std::unique_ptr<ConfigManager> m_cfg;
        
        Manager();
        ~Manager();
    };
    
    
    // macro to declare a manager class in the header
    #define DECLARE_MANAGER() \
        private : \
            friend class ::fbide::Manager;
    
}
