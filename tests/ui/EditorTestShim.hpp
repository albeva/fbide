//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <wx/app.h>
#include <wx/evtloop.h>
#include <wx/uiaction.h>
#include "config/ConfigManager.hpp"
#include "document/DocumentType.hpp"
#include "editor/CodeTransformer.hpp"
#include "editor/Editor.hpp"
#ifdef __WXMSW__
#    include <wx/msw/private.h>
#endif

namespace fbide::tests {

/// Spins up a real `fbide::Editor` plus the minimum dependencies it needs
/// for headless tests (ConfigManager + Theme + CodeTransformer). The
/// optional managers (DocumentManager, UIManager) are passed as null —
/// Editor null-guards every callsite that would touch them.
///
/// `FBIDE_TEST_RESOURCES_DIR` is the build-time absolute path to the
/// test-owned `tests/data/resources/` tree, which ships a minimal
/// `ide/config_test.ini` plus its referenced theme / keywords / locale.
/// Tests run against this isolated tree and do not depend on the
/// production `resources/` directory.
class EditorTestShim final : wxFrame {
public:
    NO_COPY_AND_MOVE(EditorTestShim)

    EditorTestShim()
    : wxFrame(nullptr, wxID_ANY, "test")
    , m_configManager(FBIDE_TEST_RESOURCES_DIR, /*idePath=*/ {}, /*configPath=*/"ide/config_test.ini")
    , m_transformer(m_configManager)
    , m_editor(new Editor(
          this,
          m_configManager,
          m_configManager.getTheme(),
          /*documentManager=*/nullptr,
          /*uiManager=*/nullptr,
          &m_transformer,
          DocumentType::FreeBASIC,
          /*preview=*/false
      )) {
        // EOL is normally configured by Document; the shim doesn't use one,
        // so set LF directly to keep test assertions platform-stable.
        m_editor->SetEOLMode(wxSTC_EOL_LF);
        const auto sizer = make_unowned<wxBoxSizer>(wxVERTICAL);
        sizer->Add(m_editor, 1, wxEXPAND);
        SetSizer(sizer);
        wxFrame::Show();
        wxFrame::Raise();
        wxFrame::Update();
#ifdef __WXMSW__
        // Foreground claim. Console-spawned processes don't get this for
        // free; without it wxUIActionSimulator's SendInput delivers to
        // whichever window the OS thinks is foreground (usually the
        // console), not our test frame. AttachThreadInput trick lets us
        // legitimately call SetForegroundWindow.
        const auto hwnd = static_cast<HWND>(wxWindow::GetHandle());
        const auto fg = ::GetForegroundWindow();
        const auto fgThread = ::GetWindowThreadProcessId(fg, nullptr);
        const auto myThread = ::GetCurrentThreadId();
        if (fgThread != myThread) {
            ::AttachThreadInput(myThread, fgThread, TRUE);
            ::BringWindowToTop(hwnd);
            ::SetForegroundWindow(hwnd);
            ::AttachThreadInput(myThread, fgThread, FALSE);
        }
#endif
        m_editor->SetFocus();
        EXPECT_TRUE(m_editor->HasFocus());
    }

    template<typename T>
    void callAfter(const T& fn) {
        CallAfter(fn);
    }

    /// Run `body` inside a real wxEventLoop. The lambda is queued via
    /// CallAfter so it runs only after Run() has entered the loop and
    /// the OS message pump is active. ScheduleExit ends the loop after
    /// the body returns, falling back through to the calling test for
    /// assertions. Without this, wxUIActionSimulator's keystrokes never
    /// dispatch — we have no MainLoop in the test harness.
    template<typename F>
    static void run(F body) {
        wxEventLoop loop;
        bool ran = false;
        wxTheApp->CallAfter([&] {
            body();
            ran = true;
            loop.ScheduleExit();
        });
        loop.Run();
        if (!ran) {
            FAIL() << "wxEventLoop exited before the test body ran";
        }
    }

    /// Editor under test.
    [[nodiscard]] auto editor() -> Editor& { return *m_editor; }

    void typeText(const wxString& text) {
        wxUIActionSimulator sim;
        for (const auto ch : text) {
            sim.Char(ch);
            wxYield();
            wxMilliSleep(10);
        }
    }

    /// Replace the entire buffer.
    void setText(const wxString& s) { m_editor->SetText(s); }

    /// Read the entire buffer back.
    [[nodiscard]] auto getText() const -> wxString { return m_editor->GetText(); }

private:
    ConfigManager m_configManager;
    CodeTransformer m_transformer;
    Editor* m_editor; ///< wx-owned via parent frame; freed via Destroy() in dtor.
};

} // namespace fbide::tests
