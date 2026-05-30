//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "update/UpdateManager.hpp"
#include <nlohmann/json.hpp>
#include <wx/richmsgdlg.h>
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {
/// GitHub Releases REST endpoint for this repository. The list form is
/// used rather than `/releases/latest` because the latter excludes
/// pre-releases — FBIde ships RC/beta builds, so a pre-release is often
/// the newest thing there is. Releases come back newest-first, so one
/// entry is enough.
constexpr auto RELEASES_URL = "https://api.github.com/repos/albeva/fbide/releases?per_page=1";

/// Downloads page opened when the user accepts the update alert.
constexpr auto DOWNLOAD_URL = "https://fbide.freebasic.net/download.html";

/// Parse a release tag into a `Version`. GitHub tags carry an optional
/// leading `v` (e.g. `v0.5.0`); the rest matches the project's own
/// `MAJOR.MINOR.PATCH[.tag-tweak]` form that `Version` already parses.
auto tagToVersion(wxString tag) -> Version {
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) {
        tag = tag.Mid(1);
    }
    return Version { tag };
}
} // namespace

UpdateManager::UpdateManager(Context& ctx)
: m_ctx(ctx) {
    // wxWidgets ships no static event-table macro for wxEVT_WEBREQUEST_STATE,
    // so this handler is wired with Bind() rather than wxDECLARE_EVENT_TABLE.
    Bind(wxEVT_WEBREQUEST_STATE, &UpdateManager::onRequestState, this);
}

void UpdateManager::checkOnStartup() {
    if (!m_ctx.getConfigManager().config().get_or("update.checkOnStartup", true)) {
        return;
    }
    startRequest(false);
}

void UpdateManager::checkManual() {
    startRequest(true);
}

void UpdateManager::startRequest(const bool manual) {
    // One request at a time. A manual click while a silent startup check
    // is still running is simply ignored rather than queued.
    if (m_request.IsOk() && m_request.GetState() == wxWebRequest::State_Active) {
        return;
    }

    m_manual = manual;
    m_request = wxWebSession::GetDefault().CreateRequest(this, RELEASES_URL);
    if (!m_request.IsOk()) {
        notifyError("could not create web request");
        return;
    }

    // GitHub rejects requests without a User-Agent (403); the Accept
    // header pins the documented JSON media type.
    m_request.SetHeader("User-Agent", "fbide");
    m_request.SetHeader("Accept", "application/vnd.github+json");
    wxLogVerbose("Update check: requesting %s", RELEASES_URL);
    m_request.Start();
}

void UpdateManager::onRequestState(wxWebRequestEvent& event) {
    switch (event.GetState()) {
    case wxWebRequest::State_Completed: {
        const auto status = event.GetResponse().GetStatus();
        if (status != 200) {
            notifyError(wxString::Format("HTTP %d", status));
            return;
        }
        handleResult(event.GetResponse().AsString());
        return;
    }
    case wxWebRequest::State_Failed:
        notifyError(event.GetErrorDescription());
        return;
    case wxWebRequest::State_Cancelled:
        return;
    default:
        // Idle / Active / Unauthorized handled elsewhere or ignored.
        return;
    }
}

void UpdateManager::handleResult(const wxString& body) {
    // GitHub returns releases newest-first, so the single entry we asked
    // for is the latest release (including pre-releases).
    Version best;
    try {
        const auto json = nlohmann::json::parse(body.utf8_string());
        if (!json.is_array() || json.empty()) {
            notifyError("no releases in response");
            return;
        }
        const auto tag = wxString::FromUTF8(json.front().value("tag_name", ""));
        if (tag.empty()) {
            notifyError("release has no tag");
            return;
        }
        best = tagToVersion(tag);
    } catch (const std::exception& ex) {
        notifyError(wxString::Format("parse error: %s", ex.what()));
        return;
    }

    const auto current = Version::fbide();
    if (best <= current) {
        wxLogVerbose("Update check: up to date (latest %s, current %s)", best.asString(), current.asString());
        if (m_manual) {
            notifyUpToDate();
        }
        return;
    }

    // A newer release exists. On the silent startup path, suppress the
    // alert if this exact version was already announced. The record is
    // updated whenever we decide to show the alert (manual or not), so a
    // manual check also disarms the next startup nag for that version.
    const auto remoteStr = best.asString();
    auto& config = m_ctx.getConfigManager().config();
    const auto alreadyNotified = config.get_or("update.lastNotifiedVersion", "") == remoteStr;
    if (!m_manual && alreadyNotified) {
        wxLogVerbose("Update check: %s already announced, staying quiet", remoteStr);
        return;
    }

    config["update"]["lastNotifiedVersion"] = remoteStr;
    m_ctx.getConfigManager().save(ConfigManager::Category::Config);
    wxLogMessage("Update check: newer release %s available (current %s)", remoteStr, current.asString());
    showUpdateAlert(best);
}

void UpdateManager::showUpdateAlert(const Version& remote) {
    auto message = m_ctx.tr("messages.updateAvailableMessage");
    message.Replace("{version}", remote.asString());
    message.Replace("{current}", Version::fbide().asString());

    wxRichMessageDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        message,
        m_ctx.tr("messages.updateAvailableTitle"),
        wxYES_NO | wxICON_INFORMATION
    );
    dlg.SetYesNoLabels(
        m_ctx.tr("messages.updateDownload"),
        m_ctx.tr("messages.updateDismiss")
    );

    if (dlg.ShowModal() == wxID_YES) {
        wxLaunchDefaultBrowser(DOWNLOAD_URL);
    }
}

void UpdateManager::notifyUpToDate() {
    wxMessageBox(
        m_ctx.tr("messages.updateUpToDateMessage"),
        m_ctx.tr("messages.updateUpToDateTitle"),
        wxOK | wxICON_INFORMATION,
        m_ctx.getUIManager().getMainFrame()
    );
}

void UpdateManager::notifyError(const wxString& detail) {
    wxLogWarning("Update check failed: %s", detail);
    if (!m_manual) {
        return;
    }
    wxMessageBox(
        m_ctx.tr("messages.updateCheckFailedMessage"),
        m_ctx.tr("messages.updateCheckFailedTitle"),
        wxOK | wxICON_WARNING,
        m_ctx.getUIManager().getMainFrame()
    );
}
