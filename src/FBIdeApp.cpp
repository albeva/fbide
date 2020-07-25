/*
* This file is part of FBIde, an open-source (cross-platform) IDE for
* FreeBasic compiler.
* Copyright (C) 2020  Albert Varaksin
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
* Contact e-mail: Albert Varaksin <albeva@me.com>
* Program URL: https://github.com/albeva/fbide
*/
#include "inc/FBIdeApp.h"
#include "inc/FBIdeMainFrame.h"
#include "inc/InstanceHandler.h"

bool FBIdeApp::OnInit() {
    SetVendorName("FBIde");
    SetAppName("FBIde");
    m_instanceHandler = std::make_unique<InstanceHandler>();

    if (argc > 1 && m_instanceHandler->IsAnotherRunning()) {
        wxString filename = argv[1];
        m_instanceHandler->SendFile(filename);
        return false;
    }

    m_frame = new FBIdeMainFrame(this, GetAppName());
    return true;
}

wxIMPLEMENT_APP(FBIdeApp);
