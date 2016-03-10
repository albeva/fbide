//
//  Editor.hpp
//  fbide
//
//  Created by Albert on 09/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once

namespace fbide {
    
    /**
     * fbide specific subclass of wxStyledTextCtrl
     */
    class Editor : NonCopyable, public wxStyledTextCtrl
    {
    public:
        using wxStyledTextCtrl::wxStyledTextCtrl;
        wxDECLARE_EVENT_TABLE();
    };
    
}
