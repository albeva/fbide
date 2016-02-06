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
        
        DECLARE_MANAGER()
    };
    
}
