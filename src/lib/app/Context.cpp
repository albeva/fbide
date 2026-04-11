//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Lang.hpp"

namespace fbide {

Context::Context(const wxString& binaryPath)
: m_config(std::make_unique<Config>(binaryPath))
, m_lang(std::make_unique<Lang>()) {}

Context::~Context() = default;

} // namespace fbide
