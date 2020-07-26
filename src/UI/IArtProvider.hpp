//
//  IArtProvider.h
//  fbide
//
//  Created by Albert on 02/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "app_pch.hpp"

namespace fbide {

/**
     * Art provider for the UI manager
     */
class IArtProvider : NonCopyable {
public:
    /**
         * Get bitmap
         */
    virtual const wxBitmap& GetIcon(const wxString& name) = 0;

    /**
         * Get bitmap size
         */
    virtual const wxSize& GetIconSize() = 0;
};

} // namespace fbide
