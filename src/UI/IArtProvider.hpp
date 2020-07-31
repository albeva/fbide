//
//  IArtProvider.h
//  fbide
//
//  Created by Albert on 02/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "pch.h"

namespace fbide {

/**
 * Art provider for the UI manager
 */
class IArtProvider {
    NON_COPYABLE(IArtProvider)
public:
    IArtProvider() = default;
    virtual ~IArtProvider();

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
