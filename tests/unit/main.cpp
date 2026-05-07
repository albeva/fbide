//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <wx/app.h>
#include <gtest/gtest.h>

namespace {
class TestApp final : public wxAppConsole {
public:
    auto OnInit() -> bool override { return true; }
    auto OnRun() -> int override { return 0; }
    void CleanUp() override { wxAppConsole::CleanUp(); }
};
} // namespace

auto main(int argc, char** argv) -> int {
    const auto app = fbide::make_unowned<TestApp>();
    wxApp::SetInstance(app);
    wxEntryStart(argc, argv);
    app->CallOnInit();

    testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();

    app->OnExit();
    wxEntryCleanup();
    return result;
}
