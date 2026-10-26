//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "format/FormatIO.hpp"

using namespace fbide;

namespace {

auto bytes(std::initializer_list<unsigned char> list) -> std::string {
    return { list.begin(), list.end() };
}

const wxString kEAcute = wxString::FromUTF8("\xC3\xA9"); // é

} // namespace

// ---------------------------------------------------------------------------
// parseEncodingName / encodingNames
// ---------------------------------------------------------------------------

TEST(FormatIOTests, ParsesEveryEditorNameExactly) {
    for (const auto value : TextEncoding::all) {
        const TextEncoding encoding { value };
        const auto parsed = parseEncodingName(encoding.toString());
        ASSERT_TRUE(parsed.has_value()) << encoding.toString();
        EXPECT_EQ(*parsed, encoding);
    }
}

TEST(FormatIOTests, ParsesNamesCaseInsensitively) {
    EXPECT_EQ(parseEncodingName("windows-1252"), TextEncoding { TextEncoding::Windows_1252 });
    EXPECT_EQ(parseEncodingName("utf-8-bom"), TextEncoding { TextEncoding::UTF8_BOM });
    EXPECT_EQ(parseEncodingName("iso-8859-1"), TextEncoding { TextEncoding::ISO_8859_1 });
}

TEST(FormatIOTests, RejectsUnknownNames) {
    EXPECT_FALSE(parseEncodingName("KOI8-R").has_value());
    EXPECT_FALSE(parseEncodingName("").has_value());
    EXPECT_FALSE(parseEncodingName("utf8").has_value());
}

TEST(FormatIOTests, EncodingNamesListsAll) {
    const auto names = encodingNames();
    for (const auto value : TextEncoding::all) {
        EXPECT_NE(names.find(TextEncoding { value }.toString()), std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// encodeWithBom
// ---------------------------------------------------------------------------

TEST(FormatIOTests, EncodesSingleByteCodePages) {
    EXPECT_EQ(encodeWithBom(kEAcute, TextEncoding::ISO_8859_1), bytes({ 0xE9 }));
    EXPECT_EQ(encodeWithBom(kEAcute, TextEncoding::CP437), bytes({ 0x82 }));
}

TEST(FormatIOTests, PrependsBom) {
    EXPECT_EQ(encodeWithBom(kEAcute, TextEncoding::UTF8_BOM), bytes({ 0xEF, 0xBB, 0xBF, 0xC3, 0xA9 }));
    EXPECT_EQ(encodeWithBom(kEAcute, TextEncoding::UTF16_LE), bytes({ 0xFF, 0xFE, 0xE9, 0x00 }));
    EXPECT_EQ(encodeWithBom(kEAcute, TextEncoding::UTF8), bytes({ 0xC3, 0xA9 }));
}

TEST(FormatIOTests, FailsForUnrepresentableCharacters) {
    EXPECT_FALSE(encodeWithBom(wxString::FromUTF8("\xE4\xB8\xAD"), TextEncoding::Windows_1252).has_value());
}
