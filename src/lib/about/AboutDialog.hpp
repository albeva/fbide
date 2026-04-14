//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;

class AboutDialog final : public wxDialog {
public:
    NO_COPY_AND_MOVE(AboutDialog)

    AboutDialog(wxWindow* parent, Context& ctx);
    void create();

private:
    Context& m_ctx;
};

} // namespace fbide
