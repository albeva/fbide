//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "AiContext.hpp"
using namespace fbide;
using namespace fbide::ai;

FileContextItem::FileContextItem(std::filesystem::path path)
: m_path(std::move(path)) {}

void FileContextItem::appendTo(wxString& out) const {
    // Append in pieces — `out += "x" + pathWx + "y"` would allocate two
    // intermediate wxStrings before the final assignment.
    out += "\n--- File: ";
    out += toWx(m_path);
    out += " ---\n";
    out += readContent();
    out += "\n";
}

auto FileContextItem::readContent() const -> wxString {
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(m_path, ec);
    if (ec) {
        // Stat failed — file missing, permission denied, etc. Drop the
        // cache so a subsequent send retries (the user may have just
        // restored the file).
        m_cacheValid = false;
        m_cachedContent.clear();
        return "<could not read file>";
    }
    if (m_cacheValid && m_cachedMtime == mtime) {
        return m_cachedContent;
    }

    wxString content;
    if (wxFFile file(toWx(m_path), "rb"); file.IsOpened() && file.ReadAll(&content, wxConvUTF8)) {
        m_cachedContent = content;
        m_cachedMtime = mtime;
        m_cacheValid = true;
        return content;
    }
    m_cacheValid = false;
    m_cachedContent.clear();
    return "<could not read file>";
}

auto FileContextItem::label() const -> wxString {
    return toWx(m_path.filename());
}

EditTargetItem::EditTargetItem(std::filesystem::path path)
: m_path(std::move(path)) {}

void EditTargetItem::appendTo(wxString& out) const {
    // A distinct header so the model recognises this file as the one it
    // is allowed to modify, not merely context to read.
    out += "\n--- Edit target: ";
    out += toWx(m_path);
    out += " ---\n";
    out += readContent();
    out += "\n";
}

auto EditTargetItem::readContent() const -> wxString {
    // Mirror of FileContextItem::readContent — see that for the cache
    // semantics. Kept verbatim rather than factored out because the
    // header format around the call site differs.
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(m_path, ec);
    if (ec) {
        m_cacheValid = false;
        m_cachedContent.clear();
        return "<could not read file>";
    }
    if (m_cacheValid && m_cachedMtime == mtime) {
        return m_cachedContent;
    }

    wxString content;
    if (wxFFile file(toWx(m_path), "rb"); file.IsOpened() && file.ReadAll(&content, wxConvUTF8)) {
        m_cachedContent = content;
        m_cachedMtime = mtime;
        m_cacheValid = true;
        return content;
    }
    m_cacheValid = false;
    m_cachedContent.clear();
    return "<could not read file>";
}

auto EditTargetItem::label() const -> wxString {
    // Pencil glyph marks the edit target apart from read-only context.
    return wxString(wxUniChar(0x270E)) + " " + toWx(m_path.filename());
}

BufferContextItem::BufferContextItem(wxString label, wxString content)
: m_label(std::move(label))
, m_content(std::move(content)) {}

void BufferContextItem::appendTo(wxString& out) const {
    out += "\n--- File: ";
    out += m_label;
    out += " ---\n";
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
