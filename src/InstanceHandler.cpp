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
#include "inc/InstanceHandler.h"
#include "inc/FBIdeMainFrame.h"
#include "inc/wxmynotebook.h"
#include "inc/FBIdeApp.h"

/**
 * Send messages from client to server
 */
class InstanceConnection : public wxConnection {
public:
    virtual bool OnExec(const wxString &topic, const wxString &data) {
        auto frame = wxGetApp().GetMainFrame();
        if (frame == nullptr) {
            wxLogMessage("Missing frame!");
            return false;
        }

        if (data.IsEmpty()) {
            return false;
        }

        wxFileName file(data);
        if (file.GetExt() == "fbs") {
            frame->SessionLoad(data);
        } else {
            int result = frame->bufferList.FileLoaded(data);
            if (result != -1)
                frame->FBNotebook->SetSelection(result);
            else {
                if (::wxFileExists(data)) {
                    frame->NewSTCPage(data, true);
                    frame->m_FileHistory->AddFileToHistory(data);
                }
            }
            frame->SetFocus();
        }

        return true;
    }
};


/**
 * Sending client
 */
class InstanceClient: public wxClient {
public:
    InstanceClient() {};

    wxConnectionBase *OnMakeConnection() {
        return new InstanceConnection();
    }
};


/**
 * Receiving server
 */
class InstanceServer: public wxServer {
public:
    wxConnectionBase *OnAcceptConnection(const wxString &topic) {
        if (topic.Lower() != INTERNAL_NAME) {
            return nullptr;
        }

        // Check that there are no modal dialogs active
        wxWindowList::Node *node = wxTopLevelWindows.GetFirst();
        while (node) {
            wxDialog *dialog = wxDynamicCast(node->GetData(), wxDialog);
            if (dialog && dialog->IsModal()) {
                return nullptr;
            }
            node = node->GetNext();
        }

        return new InstanceConnection();
    }
};


/**
 * Handle instances of FBIde.
 */
InstanceHandler::InstanceHandler()
: m_instanceChecker{INTERNAL_NAME} {
    if (!IsAnotherRunning()) {
        m_server = std::make_unique<InstanceServer>();
        if (!m_server->Create(INTERNAL_NAME)) {
            wxLogMessage("Failed to create an IPC service.");
        }
    }
}

InstanceHandler::~InstanceHandler() = default;

bool InstanceHandler::IsAnotherRunning() {
    return m_instanceChecker.IsAnotherRunning();
}

void InstanceHandler::SendFile(const wxString &file) {
    InstanceClient client;

    std::unique_ptr<wxConnectionBase> connection{client.MakeConnection(
        "localhost",
        INTERNAL_NAME,
        INTERNAL_NAME)};

    if (!connection) {
        wxLogMessage("Failed to create IPC connection");
        return;
    }

    connection->Execute(file);
    connection->Disconnect();
}
