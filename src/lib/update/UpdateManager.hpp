//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <wx/webrequest.h>
#include "config/Version.hpp"

namespace fbide {
class Context;

/**
 * Checks GitHub for a newer FBIde release and offers a download link.
 *
 * Talks to the GitHub Releases REST API over `wxWebRequest` (async, UI
 * thread). The newest release tag is parsed into a `Version` and compared
 * against the running build. There is no in-place update — the alert only
 * opens the release page in the default browser.
 *
 * **Startup check** is silent and gated twice: by the
 * `update.checkOnStartup` config flag and by the once-per-version
 * `update.lastNotifiedVersion` record, so a given release is announced at
 * most once. **Manual check** (Help → Check for updates) always reports a
 * result, including "up to date" and network failures.
 *
 * **Owns:** the in-flight `wxWebRequest` (one at a time).
 * **Owned by:** `Context`.
 * **Threading:** UI thread only — `wxEVT_WEBREQUEST_STATE` is delivered
 * on the main loop.
 */
class UpdateManager final : public wxEvtHandler {
public:
    NO_COPY_AND_MOVE(UpdateManager)

    /// Construct and bind the web-request state handler.
    explicit UpdateManager(Context& ctx);

    /// Silent startup check. No-ops when `update.checkOnStartup` is off
    /// or a request is already in flight; never shows a "no update" or
    /// error popup.
    void checkOnStartup();

    /// User-triggered check. Always reports the outcome (newer release,
    /// up to date, or failure). No-ops only while a request is in flight.
    void checkManual();

private:
    /// Kick off the GitHub releases request. `manual` selects the
    /// reporting behaviour for the eventual result.
    void startRequest(bool manual);

    /// `wxEVT_WEBREQUEST_STATE` sink — routes Completed / Failed states.
    void onRequestState(wxWebRequestEvent& event);

    /// Parse the releases JSON, pick the highest version, and decide
    /// whether to alert based on the current build and notify record.
    void handleResult(const wxString& body);

    /// Show the "update available" alert. "Download" opens the FBIde
    /// downloads page in the default browser.
    void showUpdateAlert(const Version& remote);

    /// Manual-only "you are up to date" popup.
    void notifyUpToDate();

    /// Manual-only failure popup; `detail` is logged, not shown verbatim.
    void notifyError(const wxString& detail);

    Context& m_ctx;          ///< Application context.
    wxWebRequest m_request;  ///< In-flight request (one at a time).
    bool m_manual = false;   ///< True when the current request is user-triggered.
};

} // namespace fbide
