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
        return true;
    }

private:
    wxFrame* m_frame = nullptr;
};
} // namespace
wxIMPLEMENT_APP_NO_MAIN(UiTestApp);

class WxTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        int argc = 0;
        wxEntryStart(argc, static_cast<char**>(nullptr));
        if (!wxTheApp || !wxTheApp->CallOnInit()) {
            throw std::runtime_error("wxWidgets init failed");
        }
    }

    void TearDown() override {
        wxTheApp->OnExit();
        wxEntryCleanup();
    }
};

auto main(int argc, char** argv) -> int {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new WxTestEnvironment);
    return RUN_ALL_TESTS();
}
