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

/// Modal dialog showing the FBIde version, license, and embedded readme.
class AboutDialog final : public Layout<wxDialog> {
public:
    NO_COPY_AND_MOVE(AboutDialog)

    AboutDialog(wxWindow* parent, Context& ctx);
    void create();

private:
    [[nodiscard]] auto loadReadme() const -> wxString;

    Context& m_ctx;
};

} // namespace fbide
