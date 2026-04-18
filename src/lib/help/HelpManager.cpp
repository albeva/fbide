//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "help/HelpManager.hpp"
#include "app/Context.hpp"
#include "config/Config.hpp"
#include "config/ConfigManager.hpp"
#include "editor/Document.hpp"
#include "editor/DocumentManager.hpp"
#include "ui/Layout.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {
constexpr auto FREEBASIC_WIKI_URL = "https://www.freebasic.net/wiki/";

#ifdef __WXMSW__
auto isChmBlocked(const wxString& path) -> bool {
    const auto zoneStream = path + ":Zone.Identifier";
    const HANDLE handle = CreateFile(
        zoneStream.wc_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, 0, nullptr
    );
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(handle);
    return true;
}

void buildChmBlockedContent(Layout<wxDialog>& dlg, const wxString& fileName) {
    const auto title = dlg.label("Help file is locked");
    title->SetFont(title->GetFont().Bold());

    dlg.label(
        "The file \"" + fileName + "\" is currently locked\n"
                                   "by Windows. You can unlock it by right-clicking the\n"
                                   "file, opening Properties, and clicking Unblock."
    );

    dlg.add(make_unowned<wxHyperlinkCtrl>(
        dlg.currentParent(), wxID_ANY,
        "More info",
        "https://www.helpscribble.com/chmnetwork.html"
    ));
}

void showChmBlockedWarning(wxWindow* parent, const wxString& fileName) {
    Layout<wxDialog> dlg(
        parent, wxID_ANY, "Help file is locked",
        wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE
    );

    buildChmBlockedContent(dlg, fileName);
    dlg.add(dlg.CreateStdDialogButtonSizer(wxOK));
    dlg.spacer();

    dlg.SetSizerAndFit(dlg.currentSizer());
    dlg.CentreOnParent();
    dlg.ShowModal();
}

auto showChmBlockedWithWikiOption(wxWindow* parent, const wxString& fileName) -> bool {
    Layout<wxDialog> dlg(
        parent, wxID_ANY, "Help file is locked",
        wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE
    );

    buildChmBlockedContent(dlg, fileName);

    dlg.hbox({ .border = 0 }, [&] {
        dlg.currentSizer()->AddStretchSpacer();
        dlg.button("Open Online Wiki", { .expand = false }, wxID_OK);
        dlg.button("", { .expand = false }, wxID_CANCEL);
    });
    dlg.spacer();

    dlg.SetSizerAndFit(dlg.currentSizer());
    dlg.CentreOnParent();
    return dlg.ShowModal() == wxID_OK;
}
#endif
} // namespace

HelpManager::HelpManager(Context& ctx)
: m_ctx(ctx) {}

void HelpManager::open() {
    wxString query;
    if (const auto* doc = m_ctx.getDocumentManager().getActive()) {
        query = doc->getKeywordAtCursor();
    }

#ifdef __WXMSW__
    if (openChm(query)) {
        return;
    }
#endif

    openWiki(query);
}

void HelpManager::openWiki(const wxString& query) {
    wxString url = FREEBASIC_WIKI_URL;
    if (!query.empty()) {
        auto pageName = query;
        if (pageName.StartsWith("#")) {
            pageName = pageName.Mid(1);
        }
        if (!pageName.empty()) {
            pageName[0] = wxToupper(pageName[0]);
        }
        url += "KeyPg" + pageName;
    }
    wxLaunchDefaultBrowser(url);
}

#ifdef __WXMSW__
auto HelpManager::verifyHelpFileAccessible(wxWindow* parent, const wxString& path) -> bool {
    if (path.empty() || !wxFileExists(path)) {
        return true;
    }

    if (isChmBlocked(path)) {
        showChmBlockedWarning(parent, wxFileName(path).GetFullName());
        return false;
    }
    return true;
}

auto HelpManager::openChm(const wxString& query) -> bool {
    const wxString helpFile = m_ctx.getConfigManager().config().get_or("paths.helpFile", "");
    if (helpFile.empty()) {
        return false;
    }

    const auto helpPath = m_ctx.getConfig().resolvePath(helpFile);
    if (!wxFileExists(helpPath)) {
        return false;
    }

    if (isChmBlocked(helpPath)) {
        return not showChmBlockedWithWikiOption(
            m_ctx.getUIManager().getMainFrame(),
            wxFileName(helpPath).GetFullName()
        );
    }

    if (!m_help) {
        m_help = std::make_unique<wxCHMHelpController>(m_ctx.getUIManager().getMainFrame());
        m_help->Initialize(helpPath);
    }

    if (!query.empty()) {
        m_help->KeywordSearch(query);
    } else {
        m_help->DisplayContents();
    }
    return true;
}
#endif
