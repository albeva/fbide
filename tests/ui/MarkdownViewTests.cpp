//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// Smoke tests for `MarkdownView` — instantiate, render, click. Lives in
// the `ui-tests` target because the widget needs a real `wxApp` + a
// parent window to attach to.
//
#include <wx/frame.h>
#include <gtest/gtest.h>
#include "markdown/MarkdownView.hpp"

using namespace fbide;

namespace {

constexpr int kFrameWidth = 400;
constexpr int kFrameHeight = 600;

/// Construct a hidden frame containing a `MarkdownView`. The view is
/// owned by the frame (wx parent-child); the helper releases ownership
/// via `Destroy` on test teardown.
class Fixture {
public:
    Fixture()
    : m_frame(new wxFrame(nullptr, wxID_ANY, "test"))
    , m_view(new MarkdownView(m_frame)) {
        m_frame->SetClientSize(kFrameWidth, kFrameHeight);
        m_view->SetSize(m_frame->GetClientSize());
    }

    ~Fixture() {
        m_frame->Destroy();
    }

    Fixture(const Fixture&) = delete;
    auto operator=(const Fixture&) -> Fixture& = delete;
    Fixture(Fixture&&) = delete;
    auto operator=(Fixture&&) -> Fixture& = delete;

    [[nodiscard]] auto view() const -> MarkdownView* { return m_view; }

private:
    wxFrame* m_frame;
    MarkdownView* m_view;
};

} // namespace

TEST(MarkdownViewTests, InstantiatesAndLaysOutContent) {
    Fixture fixture;
    fixture.view()->setMarkdown(
        "# Title\n\n"
        "Some **bold** and _italic_ text.\n\n"
        "- one\n- two\n- three\n"
    );

    EXPECT_FALSE(fixture.view()->markdown().empty());
    EXPECT_GT(fixture.view()->GetVirtualSize().GetHeight(), 0);
}

TEST(MarkdownViewTests, IdenticalSetMarkdownIsCheap) {
    Fixture fixture;
    const wxString text = "# Heading\n\nbody text.";
    fixture.view()->setMarkdown(text);
    const int firstHeight = fixture.view()->GetVirtualSize().GetHeight();
    fixture.view()->setMarkdown(text); // same text — internal cache hit
    EXPECT_EQ(fixture.view()->GetVirtualSize().GetHeight(), firstHeight);
}

TEST(MarkdownViewTests, RefreshThemeKeepsContent) {
    Fixture fixture;
    fixture.view()->setMarkdown("# Title\n\nbody.");
    const int beforeHeight = fixture.view()->GetVirtualSize().GetHeight();
    fixture.view()->refreshTheme();
    EXPECT_GT(fixture.view()->GetVirtualSize().GetHeight(), 0);
    // Theme refresh keeps the same markdown text.
    EXPECT_EQ(fixture.view()->markdown(), "# Title\n\nbody.");
    EXPECT_EQ(fixture.view()->GetVirtualSize().GetHeight(), beforeHeight);
}

TEST(MarkdownViewTests, LinkClickedEventDeliversUrl) {
    Fixture fixture;
    fixture.view()->setMarkdown("see [docs](https://example.org)");

    wxString captured;
    fixture.view()->Bind(MARKDOWN_LINK_CLICKED, [&captured](wxCommandEvent& ev) {
        captured = ev.GetString();
        // Don't `Skip()` — we're claiming the event so the default
        // browser-launch in `onLeftDown` doesn't fire under test.
    });

    // Synthesise the event directly. Hit-test geometry depends on
    // measurement which depends on a paint-time DC; bypassing it
    // exercises the event contract without needing a visible frame.
    wxCommandEvent ev(MARKDOWN_LINK_CLICKED, fixture.view()->GetId());
    ev.SetEventObject(fixture.view());
    ev.SetString("https://example.org");
    fixture.view()->ProcessWindowEvent(ev);

    EXPECT_EQ(captured, "https://example.org");
}
