//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiContext.hpp"
#include <wx/ffile.h>
using namespace fbide;

FileContextItem::FileContextItem(std::filesystem::path path)
: m_path(std::move(path)) {}

void FileContextItem::appendTo(wxString& out) const {
    const auto pathWx = toWx(m_path);
    wxString content;
    if (wxFFile file(pathWx, "rb"); file.IsOpened()) {
        file.ReadAll(&content, wxConvUTF8);
    } else {
        content = "<could not read file>";
    }
    out += "\n--- File: " + pathWx + " ---\n";
    out += content;
    out += "\n";
}

auto FileContextItem::label() const -> wxString {
    return toWx(m_path.filename());
}

EditTargetItem::EditTargetItem(std::filesystem::path path)
: m_path(std::move(path)) {}

void EditTargetItem::appendTo(wxString& out) const {
    const auto pathWx = toWx(m_path);
    wxString content;
    if (wxFFile file(pathWx, "rb"); file.IsOpened()) {
        file.ReadAll(&content, wxConvUTF8);
    } else {
        content = "<could not read file>";
    }
    // A distinct header so the model recognises this file as the one it
    // is allowed to modify, not merely context to read.
    out += "\n--- Edit target: " + pathWx + " ---\n";
    out += content;
    out += "\n";
}

auto EditTargetItem::label() const -> wxString {
    // Pencil glyph marks the edit target apart from read-only context.
    return wxString(wxUniChar(0x270E)) + " " + toWx(m_path.filename());
}

BufferContextItem::BufferContextItem(wxString label, wxString content)
: m_label(std::move(label))
, m_content(std::move(content)) {}

void BufferContextItem::appendTo(wxString& out) const {
    out += "\n--- File: " + m_label + " ---\n";
    out += m_content;
    out += "\n";
}

auto BufferContextItem::label() const -> wxString {
    return m_label;
}

void AiContext::add(std::unique_ptr<AiContextItem> item) {
    m_items.push_back(std::move(item));
}

void AiContext::removeAt(const std::size_t index) {
    if (index < m_items.size()) {
        m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

auto AiContext::buildText() const -> wxString {
    wxString out;
    for (const auto& item : m_items) {
        item->appendTo(out);
    }
    return out;
}

auto AiContext::editTarget() const -> const EditTargetItem* {
    for (const auto& item : m_items) {
        if (const auto* target = dynamic_cast<const EditTargetItem*>(item.get())) {
            return target;
        }
    }
    return nullptr;
}

void AiContext::setEditTarget(std::filesystem::path path) {
    // Drop any previously pinned target — only one is allowed at a time.
    std::erase_if(m_items, [](const std::unique_ptr<AiContextItem>& item) {
        return dynamic_cast<const EditTargetItem*>(item.get()) != nullptr;
    });
    if (!path.empty()) {
        m_items.push_back(std::make_unique<EditTargetItem>(std::move(path)));
    }
}
