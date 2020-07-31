/**
 * FBIde project
 */
#pragma once
#include "pch.h"

namespace fbide {

class ManagerBase;
class Manager;
class UiManager;
class ConfigManager;
class CmdManager;
class TypeManager;
class LogManager;
class Config;
class DocumentManager;

/**
 * Get manager instance.
 */
Manager& GetMgr();

/**
 * Get UI manager
 */
UiManager& GetUiMgr();

/**
 * Get configuration manager
 */
ConfigManager& GetCfgMgr();

/**
 * Get CmdManager
 */
CmdManager& GetCmdMgr();

/**
 * Get Type manager
 */
TypeManager& GetTypeMgr();

/**
 * Get DocumentManager
 */
DocumentManager& GetDocMgr();

/**
 * Get main configuration
 */
Config& GetConfig();

/**
 * Get key path from main config
 */
Config& GetConfig(const wxString& path);

/**
 * Get Language
 */
Config& GetLang();

/**
 * Get translated string
 *
 * GetCfgMgr().GetLang().Get(path, def);
 */
const wxString& GetLang(const wxString& path, const wxString& def = "");

/**
 * Get translated string and replace placeholders
 */
wxString GetLang(const wxString& path, const StringMap<wxString>& map, const wxString& def = "");

/**
 * Main manager class. This is aproxy class that holds
 * the instances and other bookkeeping of the SDK
 * and should be used to access the SDK API
 *
 * This class is a singleton
 */
class Manager final {
    NON_COPYABLE(Manager)
public:
    // Get manager instance
    static Manager& GetInstance();

    // Load
    void Load();

    // clean up
    static void Release();

    // Get UI manager
    UiManager& GetUiManager();

    // Get config manager
    ConfigManager& GetConfigManager();

    // Get cmd manager
    CmdManager& GetCmdManager();

    // Get type manager
    TypeManager& GetTypeManager();

    // Get document manager
    DocumentManager& GetDocManager();

    // Get log
    LogManager& GetLogManager();

private:
    Manager();
    ~Manager();

    static Manager* m_instance; // NOLINT
    std::unique_ptr<UiManager> m_ui;
    std::unique_ptr<ConfigManager> m_cfg;
    std::unique_ptr<CmdManager> m_cmd;
    std::unique_ptr<TypeManager> m_type;
    std::unique_ptr<LogManager> m_log;
    std::unique_ptr<DocumentManager> m_doc;
};

} // namespace fbide
