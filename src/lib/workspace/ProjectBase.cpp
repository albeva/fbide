//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ProjectBase.hpp"
#include "compiler/CompilerConfigCatalog.hpp"

using namespace fbide;

ProjectBase::ProjectBase(CompilerConfigCatalog& catalog)
: m_id(Id::generate())
, m_catalog(catalog) {}

auto ProjectBase::getCompilerConfig() const -> const ResolvedCompilerConfig& {
    // Shared: resolve whatever slug the concrete project reports against
    // the injected catalog.
    return m_catalog.resolveByPinnedSlug(getConfigurationSlug());
}
