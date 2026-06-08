//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "ui/controls/Layout.hpp"

namespace fbide {
class Context;
namespace markdown {
    class CodeHighlighter;
}

/// Modal dialog showing the FBIde version, license, and embedded readme.
class AboutDialog final : public Layout<wxDialog> {
public:
    NO_COPY_AND_MOVE(AboutDialog)

    /// Construct without populating widgets; `create()` builds the UI.
    AboutDialog(wxWindow* parent, Context& ctx);
    /// Out-of-line — `m_highlighter` holds a forward-declared type.
    ~AboutDialog() override;
    /// Build the dialog widgets.
    void create();

private:
    /// Load the bundled `readme.md` from the IDE resources directory.
    [[nodiscard]] auto loadReadme() const -> wxString;

    Context& m_ctx;                                           ///< Application context.
    std::unique_ptr<markdown::CodeHighlighter> m_highlighter; ///< FreeBASIC fence highlighter for the readme view.
};

} // namespace fbide
