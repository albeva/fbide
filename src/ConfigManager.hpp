//
//  ConfigManager.hpp
//  fbide
//
//  Created by Albert on 06/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once

#include "Manager.hpp"

namespace fbide {
    
    /**
     * Exception thrown when accessing invalid configuration
     * key
     */
    struct InvalidKey : public std::invalid_argument {
        using std::invalid_argument::invalid_argument;
    };
    
    /**
     * Config mnager is responsible for saving/loading
     * application settings.
     *
     * It also supports API to add config settings by
     * various other components and potentially plugins
     */
    class ConfigManager : NonCopyable
    {
        ConfigManager();
        ~ConfigManager();
        
    public:
        
        /**
         * Load file at specified path
         */
        void Load(const wxString & path);
        
        /**
         * get registry value or read default
         */
        wxAny & Get(const std::string & key, const wxAny & def = wxAny());
        const wxAny & Get(const std::string & key, const wxAny & def = wxAny()) const;
        
        /**
         * read / write key
         */
        wxAny & operator[](const std::string & key);
        const wxAny & operator[](const std::string & key) const;
        
        /**
         * Check if key exists
         */
        bool Has(const std::string & key) const;
        
        /**
         * Remove key
         */
        bool Remove(const std::string & key);
        
        /**
         * Storage type for configuration
         */
        typedef std::unordered_map<wxString, wxAny, wxStringHash, wxStringEqual> StoregateType;
        
    private:
        
        // the config
        StoregateType m_config;
        
        DECLARE_MANAGER()
    };
    
}
