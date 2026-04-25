//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "InstanceHandler.hpp"
#include <cmake/config.hpp>
#include "Context.hpp"
#include "document/DocumentManager.hpp"
#include "ui/UIManager.hpp"

using namespace fbide;

namespace {
/// App name serves as communication topic
wxString kAppName { cmake::project.name };

// ---------------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------------

/// Connection that handles incoming file-open requests from secondary instances.
class Connection final : public wxConnection {
public:
    explicit Connection(Context& ctx)
    : m_ctx(ctx) {}

    auto OnExec(const wxString& /*topic*/, const wxString& data) -> bool override {
        // Open the file if one was provided
        if (!data.IsEmpty()) {
            auto& docManager = m_ctx.getDocumentManager();
            docManager.openFile(data);
        }

        // Bring the main frame to front
        if (auto* frame = m_ctx.getUIManager().getMainFrame()) {
            frame->Raise();
            frame->RequestUserAttention();
            frame->SetFocus();
        }

        return true;
    }

private:
    Context& m_ctx;
};

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------

/// IPC server that accepts connections from secondary instances.
class Server final : public wxServer {
public:
    explicit Server(Context& ctx)
    : m_ctx(ctx) {}

    auto OnAcceptConnection(const wxString& topic) -> wxConnectionBase* override {
        if (topic != kAppName) {
            return nullptr;
        }

        // Reject connections while a modal dialog is active
        for (auto node = wxTopLevelWindows.GetFirst(); node != nullptr; node = node->GetNext()) {
            if (const auto* dialog = wxDynamicCast(node->GetData(), wxDialog); dialog != nullptr && dialog->IsModal()) {
                return nullptr;
            }
        }

        return new Connection(m_ctx);
    }

private:
    Context& m_ctx;
};

} // namespace

// ---------------------------------------------------------------------------
// Instance Handler
// ---------------------------------------------------------------------------

InstanceHandler::InstanceHandler(Context& ctx)
: m_ctx(ctx)
, m_checker(kAppName) {
    if (!isAnotherRunning()) {
        m_server = std::make_unique<Server>(m_ctx);
        if (not m_server->Create(getServicePath())) {
            m_server.reset();
            wxLogWarning("Failed to create IPC service.");
        }
    }
}

InstanceHandler::~InstanceHandler() = default;

auto InstanceHandler::getServicePath() -> wxString {
    return wxFileName::GetTempDir() + wxFileName::GetPathSeparator() + kAppName;
}

auto InstanceHandler::isAnotherRunning() const -> bool {
    return m_checker.IsAnotherRunning();
}

void InstanceHandler::sendFiles(const wxArrayString& files) const {
    wxClient client;
    const std::unique_ptr<wxConnectionBase> connection {
        client.MakeConnection("localhost", getServicePath(), kAppName)
    };

    if (!connection) {
        wxLogWarning("Failed to connect to running FBIde instance.");
        return;
    }

    if (files.IsEmpty()) {
        // No files — just raise the existing instance
        connection->Execute(wxEmptyString);
    } else {
        for (const auto& file : files) {
            connection->Execute(file);
        }
    }

    connection->Disconnect();
}
