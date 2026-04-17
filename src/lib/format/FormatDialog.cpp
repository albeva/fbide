//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FormatDialog.hpp"
#include "transformers/case/CaseTransform.hpp"
#include "transformers/reformat/ReFormatter.hpp"
#include "renderers/HtmlRenderer.hpp"
#include "lib/analyses/lexer/Lexer.hpp"
#include "renderers/PlainTextRenderer.hpp"
#include "lib/app/Context.hpp"
#include "lib/config/Config.hpp"
#include "lib/config/Keywords.hpp"
#include "lib/config/Lang.hpp"
#include "lib/config/Theme.hpp"
#include "lib/editor/Document.hpp"
#include "lib/editor/DocumentManager.hpp"
#include "lib/editor/Editor.hpp"
#include "lib/ui/UIManager.hpp"
#include <wx/clipbrd.h>
using namespace fbide;

namespace {

enum ControlId {
    ID_REINDENT = wxID_HIGHEST + 1,
    ID_REFORMAT,
    ID_ALIGN_PP,
    ID_CASE_UNCHANGED,
    ID_CASE_KEYWORD,
    ID_CASE_KEYWORD_UPPER,
    ID_CASE_KEYWORD_LOWER,
    ID_RENDER_CODE,
    ID_RENDER_HTML,
    ID_BROWSER,
};

} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(FormatDialog, Layout<wxDialog>)
    EVT_CHECKBOX(ID_REINDENT,               FormatDialog::onTransformChanged)
    EVT_CHECKBOX(ID_REFORMAT,               FormatDialog::onTransformChanged)
    EVT_CHECKBOX(ID_ALIGN_PP,               FormatDialog::onTransformChanged)
    EVT_RADIOBUTTON(ID_CASE_UNCHANGED,      FormatDialog::onTransformChanged)
    EVT_RADIOBUTTON(ID_CASE_KEYWORD,        FormatDialog::onTransformChanged)
    EVT_RADIOBUTTON(ID_CASE_KEYWORD_UPPER,  FormatDialog::onTransformChanged)
    EVT_RADIOBUTTON(ID_CASE_KEYWORD_LOWER,  FormatDialog::onTransformChanged)
    EVT_RADIOBUTTON(ID_RENDER_CODE,         FormatDialog::renderCode)
    EVT_RADIOBUTTON(ID_RENDER_HTML,         FormatDialog::renderHtml)
    EVT_BUTTON(wxID_OK,                     FormatDialog::onApply)
    EVT_BUTTON(ID_BROWSER,                  FormatDialog::onBrowser)
wxEND_EVENT_TABLE()
// clang-format on

FormatDialog::FormatDialog(wxWindow* parent, Context& ctx)
: Layout(
      parent, wxID_ANY, ctx.getLang()[LangId::ViewFormatTitle],
      wxDefaultPosition, wxDefaultSize,
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX
  )
, m_ctx(ctx) {}

FormatDialog::~FormatDialog() = default;

void FormatDialog::create() {
    // Options bar
    hbox("Format options", { .center = true }, [&] {
        m_reindentCheck = checkBox("Re-indent", { .expand = false }, ID_REINDENT);
        m_reindentCheck->SetToolTip("Rebuild indentation from code structure (overrides any existing indent).");

        m_alignPPCheck = checkBox("Align PP", { .expand = false }, ID_ALIGN_PP);
        m_alignPPCheck->SetToolTip(
            "Pin '#' of preprocessor directives to column 0 and indent the directive name.\n"
            "Requires Re-indent.");

        m_reformatCheck = checkBox("Re-format", { .expand = false }, ID_REFORMAT);
        m_reformatCheck->SetToolTip(
            "Normalise spacing between tokens (one space around binary ops, no space inside parens, etc.).\n"
            "Off = preserve the original inter-token whitespace.");

        separator({ .space = false });

        m_caseUnchanged = radio("Unchanged", { .expand = false }, ID_CASE_UNCHANGED, wxRB_GROUP);
        m_caseKeyWord = radio("KeyWord", { .expand = false }, ID_CASE_KEYWORD);
        m_caseKEYWORD = radio("KEYWORD", { .expand = false }, ID_CASE_KEYWORD_UPPER);
        m_casekeyword = radio("keyword", { .expand = false }, ID_CASE_KEYWORD_LOWER);
        m_caseUnchanged->SetValue(true);

        separator({ .space = false });

        radio("Format", { .expand = false }, ID_RENDER_CODE, wxRB_GROUP)->SetValue(true);
        radio("As HTML", { .expand = false }, ID_RENDER_HTML);
    });

    // Preview editor
    vbox("Previw", { .proportion = 1, .border = 0 }, [&] {
        m_preview = make_unowned<Editor>(currentParent(), m_ctx, DocumentType::FreeBASIC, true);
        m_preview->SetReadOnly(true);
        m_preview->SetMinSize(wxSize(-1, 200));
        add(m_preview, { .proportion = 1 });
    });

    // Buttons
    hbox({ .border = 0 }, [&] {
        m_browserBtn = button("Open in Browser", { .expand = false }, ID_BROWSER);

        currentSizer()->AddStretchSpacer();

        m_actionBtn = button("Apply", { .expand = false }, wxID_OK);
        button("Cancel", { .expand = false }, wxID_CANCEL);
    });
    spacer();
    SetSizerAndFit(currentSizer());
    Centre();

    // Tokenise source once (convert to UTF-8 for the lexer)
    const auto source = getSourceText();
    if (!source.empty()) {
        m_source = source.utf8_string();
        lexer::Lexer lexer(m_ctx.getKeywords());
        m_tokens = lexer.tokenise(m_source.c_str());
    }

    m_renderer = std::make_unique<PlainTextRenderer>(m_source.size());
    updatePreview();
}

void FormatDialog::onTransformChanged(wxCommandEvent&) {
    rebuildTransforms();
    updatePreview();
}

void FormatDialog::renderCode(wxCommandEvent&) {
    m_renderer = std::make_unique<PlainTextRenderer>(m_source.size());
    updatePreview();
}

void FormatDialog::renderHtml(wxCommandEvent&) {
    m_renderer = std::make_unique<HtmlRenderer>(m_ctx.getTheme(), m_source.size());
    updatePreview();
}

void FormatDialog::onApply(wxCommandEvent&) {
    const auto rendered = m_preview->GetText();
    if (rendered.IsEmpty()) {
        return;
    }

    if (m_renderer->getType() == DocumentType::FreeBASIC) {
        auto* doc = m_ctx.getDocumentManager().getActive();
        if (doc != nullptr) {
            auto* editor = doc->getEditor();
            editor->BeginUndoAction();
            editor->SetText(rendered);
            editor->EndUndoAction();
        }
    } else if (!m_tokens.empty()) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(make_unowned<wxTextDataObject>(rendered));
            wxTheClipboard->Close();
        }
    }

    EndModal(wxID_OK);
}

void FormatDialog::onBrowser(wxCommandEvent&) {
    if (m_renderer->getType() != DocumentType::HTML) {
        return;
    }

    const auto rendered = m_preview->GetText();
    if (rendered.IsEmpty()) {
        return;
    }

    const auto tmpFile = wxFileName::CreateTempFileName("fbide_html") + ".html";
    wxFile file(tmpFile, wxFile::write);
    if (file.IsOpened()) {
        file.Write(HtmlRenderer::decorate(rendered));
        file.Close();
        wxLaunchDefaultBrowser("file:///" + tmpFile);
    }
}

void FormatDialog::rebuildTransforms() {
    m_transforms.clear();

    if (const auto mode = getKeywordCase()) {
        m_transforms.push_back(std::make_unique<CaseTransform>(*mode));
    }

    const bool reIndent = m_reindentCheck->IsChecked();
    const bool reFormat = m_reformatCheck->IsChecked();
    if (reIndent || reFormat) {
        m_transforms.push_back(std::make_unique<reformat::ReFormatter>(reformat::FormatOptions {
            .tabSize = static_cast<std::size_t>(m_ctx.getConfig().getTabSize()),
            .anchoredPP = reIndent && m_alignPPCheck->IsChecked(),
            .reIndent = reIndent,
            .reFormat = reFormat,
        }));
    }
}

auto FormatDialog::getKeywordCase() const -> std::optional<CaseMode> {
    if (m_caseKeyWord->GetValue()) return CaseMode::Mixed;
    if (m_caseKEYWORD->GetValue()) return CaseMode::Upper;
    if (m_casekeyword->GetValue()) return CaseMode::Lower;
    return std::nullopt;
}

auto FormatDialog::getSourceText() const -> wxString {
    if (const auto* doc = m_ctx.getDocumentManager().getActive()) {
        return doc->getEditor()->GetText();
    }
    return {};
}

auto FormatDialog::isTransforming() const -> bool {
    return !m_transforms.empty() || m_renderer->getType() != DocumentType::FreeBASIC;
}

void FormatDialog::updateButtons() {
    const auto docTy = m_renderer->getType();
    if (docTy != DocumentType::FreeBASIC) {
        m_actionBtn->SetLabel("Copy");
    } else {
        m_actionBtn->SetLabel("Apply");
    }
    m_actionBtn->Enable(isTransforming());
    m_browserBtn->Show(docTy == DocumentType::HTML);

    // Align PP is only meaningful when Re-indent is active.
    m_alignPPCheck->Enable(m_reindentCheck->IsChecked());
}

void FormatDialog::updatePreview() {
    updateButtons();

    if (m_tokens.empty()) {
        return;
    }

    const auto thaw = FreezeLock(this);
    m_preview->SetReadOnly(false);
    m_preview->setDocType(m_renderer->getType());
    if (isTransforming()) {
        auto tokens = m_tokens;
        for (const auto& transform : m_transforms) {
            tokens = transform->apply(tokens);
        }
        m_preview->SetText(m_renderer->render(tokens));
    } else {
        m_preview->SetText(getSourceText());
    }
    m_preview->SetReadOnly(true);
}
