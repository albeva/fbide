//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentType.hpp"

namespace fbide {

auto documentTypeFromPath(const wxString& path) -> DocumentType {
    const auto ext = wxFileName(path).GetExt().Lower();
    if (ext == "bas" || ext == "bi") {
        return DocumentType::FreeBASIC;
    }
    if (ext == "html" || ext == "htm") {
        return DocumentType::HTML;
    }
    return DocumentType::Text;
}

} // namespace fbide
