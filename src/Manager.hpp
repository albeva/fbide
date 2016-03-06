/**
 * FBIde project
 */
#pragma once

namespace fbide {
    
    class ManagerBase;
    class Manager;
    class UiManager;
    class ConfigManager;
    class CmdManager;
    class Config;
    
    /**
     * Get manager instance.
     */
    Manager & GetMgr();
    
    /**
     * Get UI manager
     */
    UiManager & GetUiMgr();
    
    /**
     * Get main configuration
     */
    Config & GetConfg();
    
    /**
     * Get Language
     */
    Config & GetLang();
    
    /**
     * Get CmdManager
     */
    CmdManager & GetCmdMgr();
    
    /**
     * Get translatedstring
     *
     * This is convinience method. Equivelant to:
     * Manager::GetConfigManager.GetLang().Get(path, def);
     */
    const wxString & GetLang(const wxString & path, const wxString def = ""_wx);
    
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
        
        // Load
        void Load();
        
        // clean up
        static void Release();
        
        // Get UI manager
        UiManager & GetUiManager();
        
        // Get config manager
        ConfigManager & GetConfigManager();
        
        // Get cmd manager
        CmdManager& GetCmdManager();
        
    private:
        
        std::unique_ptr<UiManager> m_ui;
        std::unique_ptr<ConfigManager> m_cfg;
        std::unique_ptr<CmdManager> m_cmd;
        
        Manager();
        ~Manager();
    };
    
    
    // macro to declare a manager class in the header
    #define DECLARE_MANAGER() \
        private : \
            friend class ::fbide::Manager;
    
}
