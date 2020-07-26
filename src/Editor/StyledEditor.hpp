//
//  Editor.hpp
//  fbide
//
//  Created by Albert on 09/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "app_pch.hpp"

namespace fbide {

/**
     * fbide specific subclass of wxStyledTextCtrl
     */
class StyledEditor : public wxStyledTextCtrl {
public:
    StyledEditor();
    virtual ~StyledEditor();
    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
