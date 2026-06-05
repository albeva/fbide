//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "document/DocumentType.hpp"

class wxStyledTextCtrlMiniMap;

namespace fbide {
class Context;
class Document;
class Editor;

/**
 * View wrapper around a single `Editor` widget.
 *
 * Inherits `wxPanel` so the document notebook can host it as a tab
 * page directly — no separate container needed. Owns the `Editor`
 * widget and the optional minimap; manages the sizer that docks the
 * minimap to the right of the editor and auto-hides it when the
 * page is too narrow.
 *
 * This is the view side of the `Document` model/view split: any
 * future view kind (image viewer, markdown preview, …) would be a
 * sibling class — `ImagePanel`, `MarkdownPanel`, etc. — with the
 * same "wxPanel that hosts a domain widget" shape.
 *
 * **Owns:** the `Editor` widget, the optional minimap, the sizer.
 * **Owned by:** wx parent (the document notebook). `Document` keeps
 * a non-owning `Unowned<EditorPanel>` back-link.
 * **Threading:** UI thread only.
 */
class EditorPanel final : public wxPanel {
public:
    NO_COPY_AND_MOVE(EditorPanel)

    /// Build the panel as a child of `parent`, hosting the editor for
    /// `doc`. Reads minimap defaults (width + initial visibility)
    /// from config, constructs the editor, lays everything out, and
    /// publishes the back-link via `doc.attachView(this)` so the
    /// model knows where its presenting view is.
    EditorPanel(wxWindow* parent, Context& ctx, DocumentType type, Document& doc);

    /// Notifies `m_doc` to drop its back-link. wx parent-driven
    /// destruction (notebook page close) routes through this so
    /// `Document::m_panel` doesn't dangle.
    ~EditorPanel() override;

    /// The hosted editor widget. Non-null for the lifetime of the
    /// panel — the editor is wx-parented to the panel and destroyed
    /// with it.
    [[nodiscard]] auto getEditor() -> Editor* { return m_editor; }
    /// Const overload of `getEditor`.
    [[nodiscard]] auto getEditor() const -> const Editor* { return m_editor; }

    /// The document this panel presents. Lets the notebook map a page back to
    /// its document without a global document registry.
    [[nodiscard]] auto getDocument() const -> Document& { return m_doc; }

    /// Toggle the minimap. Visibility is also gated on the page
    /// being wide enough (`kMinEditorWidth` of editor area must
    /// remain) — see `updateMinimapVisibility`.
    void showMinimap(bool enabled);

    /// Re-apply editor settings (font, theme, keywords) after a
    /// config / theme reload. Also rebuilds the minimap's editor
    /// binding so its rendering picks up the new colours.
    void updateSettings();

private:
    /// Page resized — re-evaluate whether the minimap still fits.
    void onSize(wxSizeEvent& event);
    /// Show/hide the minimap based on the current page width.
    void updateMinimapVisibility() const;
    /// Create the minimap widget and dock it into the layout.
    void createMinimap();
    /// Destroy the minimap widget and drop it from the layout.
    void destroyMinimap();

    Document& m_doc;                            ///< Back reference — used to publish / clear the view link.
    Unowned<Editor> m_editor;                   ///< Editor widget — child of this panel.
    Unowned<wxStyledTextCtrlMiniMap> m_minimap; ///< Minimap — lazily created; null while disabled.
    int m_minimapWidth;                         ///< Minimap width in px — `editor.minimapWidth` config key.
    bool m_minimapEnabled;                      ///< Minimap toggle state — `commands.viewMinimap`.

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
