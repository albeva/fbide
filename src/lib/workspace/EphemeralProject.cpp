//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "EphemeralProject.hpp"
#include "compiler/CompilerConfigCatalog.hpp"
#include "document/Document.hpp"

using namespace fbide;

EphemeralProject::EphemeralProject(CompilerConfigCatalog& catalog, Document& doc)
: ProjectBase(catalog)
, m_document(&doc)
, m_path(doc.getFilePath()) {}

auto EphemeralProject::getDocuments() const -> std::vector<Document*> {
    // Report the document only while it is still mutually bound to this
    // project — mirrors the persistent tree's "File::doc != null" so a
    // close / teardown doesn't re-enter through a stale back-link.
    return (m_document != nullptr && m_document->getProject() == this)
             ? std::vector<Document*> { m_document }
             : std::vector<Document*> {};
}

auto EphemeralProject::getSources() const -> std::vector<Document*> {
    // The single bound document is the project's only source.
    return getDocuments();
}

auto EphemeralProject::getMenuConfigurations(const wxString& alwaysInclude) const
    -> std::vector<const ResolvedCompilerConfig*> {
    return catalog().menuConfigs(alwaysInclude);
}

auto EphemeralProject::getCapabilities() const -> std::uint8_t {
    return +Capability::Compile | +Capability::CompileAndRun | +Capability::Run | +Capability::QuickRun;
}
