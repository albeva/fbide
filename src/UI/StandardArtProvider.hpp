//
//  StandardArtProvider.hpp
//  fbide
//
//  Created by Albert on 02/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "app_pch.hpp"
#include "IArtProvider.hpp"

namespace fbide {

/**
 * fbide standard art provider
 */
class StandardArtProvider final: public IArtProvider {
public:
    /**
     * Get bitmap
     */
    virtual const wxBitmap& GetIcon(const wxString& name) override;

    /**
     * Get bitmap size
     */
    virtual const wxSize& GetIconSize() override;
};

} // namespace fbide
