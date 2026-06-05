//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "EphemeralProject.hpp"
#include "compiler/CompilerConfigCatalog.hpp"
#include "document/Document.hpp"
#include "document/DocumentType.hpp"

using namespace fbide;

EphemeralProject::EphemeralProject(CompilerConfigCatalog& catalog)
: ProjectBase(catalog) {}

EphemeralProject::~EphemeralProject() = default;

auto EphemeralProject::adopt(std::unique_ptr<Document> doc) -> Document* {
    auto* ptr = doc.get();
    m_documents.push_back(std::move(doc));
    ptr->bindToProject(this);
    return ptr;
}

void EphemeralProject::remove(Document* doc) {
    if (m_active == doc) {
        m_active = nullptr;
    }
    std::erase_if(m_documents, [doc](const auto& owned) { return owned.get() == doc; });
}

void EphemeralProject::setActive(Document* doc) {
    m_active = doc;
}

auto EphemeralProject::getDocuments() const -> std::vector<Document*> {
    std::vector<Document*> result;
    result.reserve(m_documents.size());
    for (const auto& doc : m_documents) {
        result.push_back(doc.get());
    }
    return result;
}

auto EphemeralProject::getSources() const -> std::vector<Document*> {
    // Build targets the focused standalone file — and only when it is
    // FreeBASIC (other types aren't compilable).
    if (m_active != nullptr && m_active->getType() == DocumentType::FreeBASIC) {
        return { m_active };
    }
    return {};
}

auto EphemeralProject::getMenuConfigurations(const wxString& alwaysInclude) const
    -> std::vector<const ResolvedCompilerConfig*> {
    if (m_active != nullptr && m_active->getType() == DocumentType::FreeBASIC) {
        return catalog().menuConfigs(alwaysInclude);
    }
    return {};
}

auto EphemeralProject::getCapabilities() const -> std::uint8_t {
    if (m_active != nullptr && m_active->getType() == DocumentType::FreeBASIC) {
        return +Capability::Compile | +Capability::CompileAndRun | +Capability::Run | +Capability::QuickRun;
    }
    return 0;
}
