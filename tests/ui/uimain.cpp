//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <wx/app.h>
#include <wx/frame.h>
#include <gtest/gtest.h>

namespace {
// GUI test harness. Uses wxApp (not wxAppConsole) so wx GUI controls
// (wxStyledTextCtrl, etc.) can be instantiated with a real wx parent.
// A hidden top-level wxFrame is created in OnInit and reachable from
// tests via wxTheApp->GetTopWindow().
class UiTestApp final : public wxApp {
public:
    auto OnInit() -> bool override {
        m_frame = new wxFrame(nullptr, wxID_ANY, "fbide-ui-tests");
        // Frame stays hidden — never call Show().
        SetTopWindow(m_frame);
        return true;
    }

private:
    wxFrame* m_frame = nullptr;
};
} // namespace

auto main(int argc, char** argv) -> int {
    const auto app = fbide::make_unowned<UiTestApp>();
    wxApp::SetInstance(app);
    wxEntryStart(argc, argv);
    app->CallOnInit();

    testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();

    app->OnExit();
    wxEntryCleanup();
    return result;
}
