//
//  ConfigManager.hpp
//  fbide
//
//  Created by Albert on 06/02/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "Config.hpp"

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
    class ConfigManager final : NonCopyable
    {
    public:
        ConfigManager();
        ~ConfigManager();
        
        /**
         * Get main configuration root object
         */
        inline Config & Get() { return m_root; }
        
        /**
         * Get language
         */
        inline Config & Lang() { return m_lang; }
        
        /**
         * Load file at specified path
         */
        void Load(const wxString & path);
        
        
    private:
        
        Config m_root;
        Config m_lang;
        
    };
    
}
