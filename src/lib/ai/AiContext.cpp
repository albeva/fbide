//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiContext.hpp"
#include <wx/ffile.h>
using namespace fbide;

FileContextItem::FileContextItem(wxString path)
: m_path(std::move(path)) {}

void FileContextItem::appendTo(wxString& out) const {
    wxString content;
    if (wxFFile file(m_path, "rb"); file.IsOpened()) {
        file.ReadAll(&content, wxConvUTF8);
    } else {
        content = "<could not read file>";
    }
    out += "\n--- File: " + m_path + " ---\n";
    out += content;
    out += "\n";
}

auto FileContextItem::label() const -> wxString {
    return wxFileName(m_path).GetFullName();
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
