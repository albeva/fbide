//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/**
 * One item attached to the AI conversation as context.
 *
 * Abstract so new kinds of context (directories, symbols, ...) can be
 * added later as subclasses without touching `AiContext` or the UI.
 */
class AiContextItem {
public:
    NO_COPY_AND_MOVE(AiContextItem)

    AiContextItem() = default;
    virtual ~AiContextItem() = default;

    /// Append this item's context text to `out`. Content is gathered
    /// fresh at call time (snapshot when the message is sent).
    virtual void appendTo(wxString& out) const = 0;

    /// Short label for the context list in the UI.
    [[nodiscard]] virtual auto label() const -> wxString = 0;
};

/// A single file attached as context. Its content is read fresh each
/// time `appendTo` is called.
class FileContextItem final : public AiContextItem {
public:
    /// Construct for the file at `path`.
    explicit FileContextItem(wxString path);

    void appendTo(wxString& out) const override;
    [[nodiscard]] auto label() const -> wxString override;

private:
    wxString m_path; ///< Absolute path of the attached file.
};

/**
 * Ordered set of context items attached to the conversation.
 *
 * Owned by `AiManager`; manipulated by the chat panel's context bar.
 */
class AiContext final {
public:
    NO_COPY_AND_MOVE(AiContext)

    AiContext() = default;

    /// Append a context item.
    void add(std::unique_ptr<AiContextItem> item);

    /// Remove the item at `index` (no-op when out of range).
    void removeAt(std::size_t index);

    /// True when no items are attached.
    [[nodiscard]] auto empty() const -> bool { return m_items.empty(); }

    /// The attached items, in order.
    [[nodiscard]] auto items() const -> const std::vector<std::unique_ptr<AiContextItem>>& { return m_items; }

    /// Build the combined context text from every item.
    [[nodiscard]] auto buildText() const -> wxString;

private:
    std::vector<std::unique_ptr<AiContextItem>> m_items; ///< Attached items.
};

} // namespace fbide
