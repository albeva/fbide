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
    /// Construct for the file at `path`. Stored as `std::filesystem::path`
    /// throughout — callers feeding paths from wx APIs (wxFileDialog
    /// etc.) convert at their own boundary via `toPath()`.
    explicit FileContextItem(std::filesystem::path path);

    void appendTo(wxString& out) const override;
    [[nodiscard]] auto label() const -> wxString override;

private:
    std::filesystem::path m_path; ///< Absolute path of the attached file.
};

/// The conversation's edit target — the file the model is allowed to
/// propose writes against in agent mode. Distinct from `FileContextItem`
/// because (a) only one may be pinned at a time and (b) the chat UI
/// surfaces the apply / reject affordances only when one is present.
/// Content is read fresh from disk on every send, like `FileContextItem`.
class EditTargetItem final : public AiContextItem {
public:
    explicit EditTargetItem(std::filesystem::path path);

    void appendTo(wxString& out) const override;
    [[nodiscard]] auto label() const -> wxString override;

    /// Absolute path of the pinned file — used by the chat view to
    /// resolve which document to apply SEARCH/REPLACE blocks against.
    [[nodiscard]] auto path() const -> const std::filesystem::path& { return m_path; }

private:
    std::filesystem::path m_path;
};

/// An in-memory buffer attached as context — a snapshot of an editor's text
/// taken when the item was created. Used for open tabs, whose live content
/// (including unsaved edits) is captured at attach time.
class BufferContextItem final : public AiContextItem {
public:
    /// Construct with a display `label` (the tab title) and captured `content`.
    BufferContextItem(wxString label, wxString content);

    void appendTo(wxString& out) const override;
    [[nodiscard]] auto label() const -> wxString override;

private:
    wxString m_label;   ///< Display label — the tab title.
    wxString m_content; ///< Captured editor text.
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

    /// The pinned edit target, or nullptr when none. Walk is linear over
    /// the items list — there is at most one in practice (single-pinned).
    [[nodiscard]] auto editTarget() const -> const EditTargetItem*;

    /// Replace any existing pinned edit target with one for `path`. Pass
    /// an empty path to clear without setting a new one.
    void setEditTarget(std::filesystem::path path);

private:
    std::vector<std::unique_ptr<AiContextItem>> m_items; ///< Attached items.
};

} // namespace fbide
