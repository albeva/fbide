//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include <wx/app.h>

namespace {
class TestApp final : public wxAppConsole {
public:
    auto OnInit() -> bool override { return true; }
    auto OnRun() -> int override { return 0; }
    void CleanUp() override { wxAppConsole::CleanUp(); }
};
} // namespace

auto main(int argc, char** argv) -> int {
    auto* app = new TestApp();
    wxApp::SetInstance(app);
    wxEntryStart(argc, argv);
    app->CallOnInit();

    testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();

    app->OnExit();
    wxEntryCleanup();
    return result;
}
