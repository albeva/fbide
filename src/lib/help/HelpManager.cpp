//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "lib/help/HelpManager.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/editor/Document.hpp"
#include "lib/editor/DocumentManager.hpp"
#include "lib/ui/UIManager.hpp"
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

void showChmBlockedWarning(wxWindow* parent, const wxString& fileName) {
    wxDialog dlg(parent, wxID_ANY, "Help file is locked",
        wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);

    const auto title = make_unowned<wxStaticText>(&dlg, wxID_ANY, "Help file is locked");
    title->SetFont(title->GetFont().Bold());
    sizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    sizer->Add(make_unowned<wxStaticText>(&dlg, wxID_ANY,
        "The file \"" + fileName + "\" is currently locked\n"
        "by Windows. You can unlock it by right-clicking the\n"
        "file, opening Properties, and clicking Unblock."),
        0, wxLEFT | wxRIGHT | wxTOP, 10);

    sizer->Add(make_unowned<wxHyperlinkCtrl>(&dlg, wxID_ANY,
        "More info",
        "https://www.helpscribble.com/chmnetwork.html"),
        0, wxLEFT | wxRIGHT | wxTOP, 10);

    sizer->Add(dlg.CreateStdDialogButtonSizer(wxOK),
        0, wxALL | wxEXPAND, 10);

    dlg.SetSizerAndFit(sizer);
    dlg.CentreOnParent();
    dlg.ShowModal();
}

auto showChmBlockedWithWikiOption(wxWindow* parent, const wxString& fileName) -> bool {
    wxDialog dlg(parent, wxID_ANY, "Help file is locked",
        wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
    const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);

    const auto title = make_unowned<wxStaticText>(&dlg, wxID_ANY, "Help file is locked");
    title->SetFont(title->GetFont().Bold());
    sizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    sizer->Add(make_unowned<wxStaticText>(&dlg, wxID_ANY,
        "The file \"" + fileName + "\" is currently locked\n"
        "by Windows. You can unlock it by right-clicking the\n"
        "file, opening Properties, and clicking Unblock."),
        0, wxLEFT | wxRIGHT | wxTOP, 10);

    sizer->Add(make_unowned<wxHyperlinkCtrl>(&dlg, wxID_ANY,
        "More info",
        "https://www.helpscribble.com/chmnetwork.html"),
        0, wxLEFT | wxRIGHT | wxTOP, 10);

    const auto btnSizer = make_unowned<wxBoxSizer>(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(make_unowned<wxButton>(&dlg, wxID_OK, "Open Online Wiki"), 0, wxRIGHT, 5);
    btnSizer->Add(make_unowned<wxButton>(&dlg, wxID_CANCEL));
    sizer->Add(btnSizer, 0, wxALL | wxEXPAND, 10);

    dlg.SetSizerAndFit(sizer);
    dlg.CentreOnParent();
    return dlg.ShowModal() == wxID_OK;
}
#endif
} // namespace

HelpManager::HelpManager(Context& ctx)
: m_ctx(ctx) {}

auto HelpManager::verifyHelpFileAccessible(wxWindow* parent, const wxString& path) -> bool {
#ifdef __WXMSW__
    if (path.empty() || !wxFileExists(path)) {
        return true;
    }

    if (isChmBlocked(path)) {
        showChmBlockedWarning(parent, wxFileName(path).GetFullName());
        return false;
    }
#else
    wxUnusedVar(parent);
    wxUnusedVar(path);
#endif
    return true;
}

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
auto HelpManager::openChm(const wxString& query) -> bool {
    const auto& helpFile = m_ctx.getConfig().getHelpFile();
    if (helpFile.empty()) {
        return false;
    }

    const auto helpPath = m_ctx.getConfig().resolvePath(helpFile);
    if (!wxFileExists(helpPath)) {
        return false;
    }

    if (isChmBlocked(helpPath)) {
        const auto useWiki = showChmBlockedWithWikiOption(
            m_ctx.getUIManager().getMainFrame(),
            wxFileName(helpPath).GetFullName()
        );
        if (!useWiki) {
            return true; // handled — user cancelled
        }
        return false; // fall through to wiki
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
