//
//  StandardArtProvider.hpp
//  fbide
//
//  Created by Albert on 02/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "pch.h"
#include "IArtProvider.hpp"

namespace fbide {

/**
 * fbide standard art provider
 */
class StandardArtProvider final: public IArtProvider {
    NON_COPYABLE(StandardArtProvider)
public:
    StandardArtProvider() = default;
    ~StandardArtProvider() final = default;

    /**
     * Get bitmap
     */
    const wxBitmap& GetIcon(const wxString& name) final;

    /**
     * Get bitmap size
     */
    const wxSize& GetIconSize() final;
};

} // namespace fbide
