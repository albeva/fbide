//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "lib/analyses/lexer/Token.hpp"
#include "lib/ui/Layout.hpp"

namespace fbide {
class Context;
class Editor;
class Renderer;
class TokenTransform;
enum class CaseMode;

/// Format dialog — keyword case conversion and code export.
class FormatDialog final : public Layout<wxDialog> {
public:
    NO_COPY_AND_MOVE(FormatDialog)

    FormatDialog(wxWindow* parent, Context& ctx);
    ~FormatDialog() override;
    void create();

private:
    void onTransformChanged(wxCommandEvent& event);
    void renderCode(wxCommandEvent& event);
    void renderHtml(wxCommandEvent& event);
    void renderBBCode(wxCommandEvent& event);
    void onApply(wxCommandEvent& event);
    void onBrowser(wxCommandEvent& event);

    void updatePreview();
    void updateButtons();

    void rebuildTransforms();

    [[nodiscard]] auto getSourceText() const -> wxString;
    [[nodiscard]] auto isTransforming() const -> bool;
    [[nodiscard]] auto getKeywordCase() const -> std::optional<CaseMode>;

    Context& m_ctx;
    std::string m_source;                // UTF-8 source buffer for tokenisation
    std::vector<lexer::Token> m_tokens;
    std::vector<std::unique_ptr<TokenTransform>> m_transforms;
    std::unique_ptr<Renderer> m_renderer;

    Unowned<wxCheckBox> m_reindentCheck = nullptr;
    Unowned<wxRadioButton> m_caseUnchanged = nullptr;
    Unowned<wxRadioButton> m_caseKeyWord = nullptr;
    Unowned<wxRadioButton> m_caseKEYWORD = nullptr;
    Unowned<wxRadioButton> m_casekeyword = nullptr;
    Unowned<Editor> m_preview = nullptr;
    Unowned<wxButton> m_actionBtn = nullptr;
    Unowned<wxButton> m_browserBtn = nullptr;

    wxDECLARE_EVENT_TABLE();
};

} // namespace fbide
