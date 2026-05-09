//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "document/TextEncoding.hpp"

using namespace fbide;

class TextEncodingTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Enum fundamentals
// ---------------------------------------------------------------------------

TEST_F(TextEncodingTests, AllEncodingsListPopulated) {
    EXPECT_GE(TextEncoding::all.size(), 10u);
    EXPECT_NE(std::ranges::find(TextEncoding::all, TextEncoding::UTF8), TextEncoding::all.end());
    EXPECT_NE(std::ranges::find(TextEncoding::all, TextEncoding::UTF8_BOM), TextEncoding::all.end());
    EXPECT_NE(std::ranges::find(TextEncoding::all, TextEncoding::UTF16_LE), TextEncoding::all.end());
    EXPECT_NE(std::ranges::find(TextEncoding::all, TextEncoding::Windows_1252), TextEncoding::all.end());
    EXPECT_NE(std::ranges::find(TextEncoding::all, TextEncoding::CP437), TextEncoding::all.end());
    EXPECT_NE(std::ranges::find(TextEncoding::all, TextEncoding::System), TextEncoding::all.end());
}

TEST_F(TextEncodingTests, AllEolModesListPopulated) {
    EXPECT_EQ(EolMode::all.size(), 3u);
    EXPECT_NE(std::ranges::find(EolMode::all, EolMode::LF), EolMode::all.end());
    EXPECT_NE(std::ranges::find(EolMode::all, EolMode::CRLF), EolMode::all.end());
    EXPECT_NE(std::ranges::find(EolMode::all, EolMode::CR), EolMode::all.end());
}

TEST_F(TextEncodingTests, EncodingStringRoundTrip) {
    for (const auto value : TextEncoding::all) {
        const TextEncoding enc { value };
        const auto name = enc.toString();
        EXPECT_FALSE(name.empty());
        const auto parsed = TextEncoding::parse(name);
        ASSERT_TRUE(parsed.has_value()) << "Failed: " << name;
        EXPECT_EQ(*parsed, enc);
    }
}

TEST_F(TextEncodingTests, EolStringRoundTrip) {
    for (const auto value : EolMode::all) {
        const EolMode mode { value };
        const auto name = mode.toString();
        EXPECT_FALSE(name.empty());
        const auto parsed = EolMode::parse(name);
        ASSERT_TRUE(parsed.has_value()) << "Failed: " << name;
        EXPECT_EQ(*parsed, mode);
    }
}

TEST_F(TextEncodingTests, EncodingFromStringRejectsGarbage) {
    EXPECT_FALSE(TextEncoding::parse("").has_value());
    EXPECT_FALSE(TextEncoding::parse("utf-42").has_value());
    EXPECT_FALSE(TextEncoding::parse("garbage").has_value());
}

TEST_F(TextEncodingTests, EolFromStringRejectsGarbage) {
    EXPECT_FALSE(EolMode::parse("").has_value());
    EXPECT_FALSE(EolMode::parse("lol").has_value());
}

TEST_F(TextEncodingTests, EncodingConfigKeysStable) {
    // Config keys are durable — changing breaks existing user INIs
    EXPECT_EQ(TextEncoding { TextEncoding::UTF8 }.toString(), "UTF-8");
    EXPECT_EQ(TextEncoding { TextEncoding::UTF8_BOM }.toString(), "UTF-8-BOM");
    EXPECT_EQ(TextEncoding { TextEncoding::UTF16_LE }.toString(), "UTF-16-LE");
    EXPECT_EQ(TextEncoding { TextEncoding::UTF16_BE }.toString(), "UTF-16-BE");
    EXPECT_EQ(TextEncoding { TextEncoding::Windows_1252 }.toString(), "Windows-1252");
    EXPECT_EQ(TextEncoding { TextEncoding::CP437 }.toString(), "CP437");
    EXPECT_EQ(TextEncoding { TextEncoding::System }.toString(), "System");
}

TEST_F(TextEncodingTests, EolConfigKeysStable) {
    EXPECT_EQ(EolMode { EolMode::LF }.toString(), "LF");
    EXPECT_EQ(EolMode { EolMode::CRLF }.toString(), "CRLF");
    EXPECT_EQ(EolMode { EolMode::CR }.toString(), "CR");
}

// ---------------------------------------------------------------------------
// wx mapping helpers
// ---------------------------------------------------------------------------

TEST_F(TextEncodingTests, ToStcEolMode) {
    EXPECT_EQ(EolMode { EolMode::LF }.toStc(), wxSTC_EOL_LF);
    EXPECT_EQ(EolMode { EolMode::CRLF }.toStc(), wxSTC_EOL_CRLF);
    EXPECT_EQ(EolMode { EolMode::CR }.toStc(), wxSTC_EOL_CR);
}

TEST_F(TextEncodingTests, FromStcEolMode) {
    EXPECT_EQ(EolMode::fromStc(wxSTC_EOL_LF), EolMode::LF);
    EXPECT_EQ(EolMode::fromStc(wxSTC_EOL_CRLF), EolMode::CRLF);
    EXPECT_EQ(EolMode::fromStc(wxSTC_EOL_CR), EolMode::CR);
}

TEST_F(TextEncodingTests, ToWxBom) {
    EXPECT_EQ(TextEncoding { TextEncoding::UTF8_BOM }.toWxBom(), wxBOM_UTF8);
    EXPECT_EQ(TextEncoding { TextEncoding::UTF16_LE }.toWxBom(), wxBOM_UTF16LE);
    EXPECT_EQ(TextEncoding { TextEncoding::UTF16_BE }.toWxBom(), wxBOM_UTF16BE);
    // Non-BOM encodings map to wxBOM_None
    EXPECT_EQ(TextEncoding { TextEncoding::UTF8 }.toWxBom(), wxBOM_None);
    EXPECT_EQ(TextEncoding { TextEncoding::Windows_1252 }.toWxBom(), wxBOM_None);
    EXPECT_EQ(TextEncoding { TextEncoding::CP437 }.toWxBom(), wxBOM_None);
}

TEST_F(TextEncodingTests, FromWxBom) {
    const auto utf8 = TextEncoding::fromWxBom(wxBOM_UTF8);
    ASSERT_TRUE(utf8.has_value());
    EXPECT_EQ(*utf8, TextEncoding::UTF8_BOM);

    const auto utf16le = TextEncoding::fromWxBom(wxBOM_UTF16LE);
    ASSERT_TRUE(utf16le.has_value());
    EXPECT_EQ(*utf16le, TextEncoding::UTF16_LE);

    const auto utf16be = TextEncoding::fromWxBom(wxBOM_UTF16BE);
    ASSERT_TRUE(utf16be.has_value());
    EXPECT_EQ(*utf16be, TextEncoding::UTF16_BE);

    EXPECT_FALSE(TextEncoding::fromWxBom(wxBOM_None).has_value());
    EXPECT_FALSE(TextEncoding::fromWxBom(wxBOM_Unknown).has_value());
}

// ---------------------------------------------------------------------------
// BOM bytes
// ---------------------------------------------------------------------------

TEST_F(TextEncodingTests, BomLengthForBomVariants) {
    EXPECT_EQ(TextEncoding { TextEncoding::UTF8_BOM }.bomLength(), 3u);
    EXPECT_EQ(TextEncoding { TextEncoding::UTF16_LE }.bomLength(), 2u);
    EXPECT_EQ(TextEncoding { TextEncoding::UTF16_BE }.bomLength(), 2u);
}

TEST_F(TextEncodingTests, BomLengthForNonBomEncodings) {
    EXPECT_EQ(TextEncoding { TextEncoding::UTF8 }.bomLength(), 0u);
    EXPECT_EQ(TextEncoding { TextEncoding::Windows_1252 }.bomLength(), 0u);
    EXPECT_EQ(TextEncoding { TextEncoding::CP437 }.bomLength(), 0u);
    EXPECT_EQ(TextEncoding { TextEncoding::System }.bomLength(), 0u);
}

TEST_F(TextEncodingTests, BomBytesUtf8) {
    const auto bytes = TextEncoding { TextEncoding::UTF8_BOM }.bomBytes();
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(static_cast<unsigned char>(bytes[0]), 0xEF);
    EXPECT_EQ(static_cast<unsigned char>(bytes[1]), 0xBB);
    EXPECT_EQ(static_cast<unsigned char>(bytes[2]), 0xBF);
}

TEST_F(TextEncodingTests, BomBytesUtf16Le) {
    const auto bytes = TextEncoding { TextEncoding::UTF16_LE }.bomBytes();
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(static_cast<unsigned char>(bytes[0]), 0xFF);
    EXPECT_EQ(static_cast<unsigned char>(bytes[1]), 0xFE);
}

TEST_F(TextEncodingTests, BomBytesUtf16Be) {
    const auto bytes = TextEncoding { TextEncoding::UTF16_BE }.bomBytes();
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(static_cast<unsigned char>(bytes[0]), 0xFE);
    EXPECT_EQ(static_cast<unsigned char>(bytes[1]), 0xFF);
}

TEST_F(TextEncodingTests, BomBytesEmptyForNonBom) {
    EXPECT_TRUE(TextEncoding { TextEncoding::UTF8 }.bomBytes().empty());
    EXPECT_TRUE(TextEncoding { TextEncoding::Windows_1252 }.bomBytes().empty());
    EXPECT_TRUE(TextEncoding { TextEncoding::CP437 }.bomBytes().empty());
}

// ---------------------------------------------------------------------------
// Codec — decode / encode
// ---------------------------------------------------------------------------

TEST_F(TextEncodingTests, EncodeAsciiUtf8) {
    const TextEncoding enc { TextEncoding::UTF8 };
    const auto bytes = enc.encode(wxString("hello"));
    ASSERT_TRUE(bytes.has_value());
    ASSERT_EQ(bytes->length(), 5u);
    EXPECT_EQ(std::memcmp(bytes->data(), "hello", 5), 0);
}

TEST_F(TextEncodingTests, EncodeNonAsciiUtf8) {
    // "é" (U+00E9) encodes to 0xC3 0xA9 in UTF-8
    const wxString text = wxString::FromUTF8("\xC3\xA9");
    const auto bytes = TextEncoding { TextEncoding::UTF8 }.encode(text);
    ASSERT_TRUE(bytes.has_value());
    ASSERT_EQ(bytes->length(), 2u);
    EXPECT_EQ(static_cast<unsigned char>(bytes->data()[0]), 0xC3);
    EXPECT_EQ(static_cast<unsigned char>(bytes->data()[1]), 0xA9);
}

TEST_F(TextEncodingTests, EncodeUtf8BomSameAsUtf8) {
    // BOM is handled separately — codec returns payload only
    const wxString text = wxString::FromUTF8("hello \xC3\xA9");
    const auto plain = TextEncoding { TextEncoding::UTF8 }.encode(text);
    const auto withBomEnum = TextEncoding { TextEncoding::UTF8_BOM }.encode(text);
    ASSERT_TRUE(plain.has_value());
    ASSERT_TRUE(withBomEnum.has_value());
    ASSERT_EQ(plain->length(), withBomEnum->length());
    EXPECT_EQ(std::memcmp(plain->data(), withBomEnum->data(), plain->length()), 0);
}

TEST_F(TextEncodingTests, DecodeAsciiUtf8) {
    const char bytes[] = "hello";
    const auto str = TextEncoding { TextEncoding::UTF8 }.decode(bytes, 5);
    ASSERT_TRUE(str.has_value());
    EXPECT_EQ(*str, wxString("hello"));
}

TEST_F(TextEncodingTests, DecodeNonAsciiUtf8) {
    const unsigned char bytes[] = { 0xC3, 0xA9 };
    const auto str = TextEncoding { TextEncoding::UTF8 }.decode(bytes, sizeof(bytes));
    ASSERT_TRUE(str.has_value());
    EXPECT_EQ(*str, wxString::FromUTF8("\xC3\xA9"));
}

TEST_F(TextEncodingTests, RoundTripUtf8) {
    const wxString text = wxString::FromUTF8("Hello, \xC3\xA9\xC3\xA1\xC3\xB1!");
    const TextEncoding enc { TextEncoding::UTF8 };
    const auto bytes = enc.encode(text);
    ASSERT_TRUE(bytes.has_value());
    const auto back = enc.decode(bytes->data(), bytes->length());
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, text);
}

TEST_F(TextEncodingTests, RoundTripUtf16Le) {
    const wxString text = wxString::FromUTF8("Test \xC3\xA9 string");
    const TextEncoding enc { TextEncoding::UTF16_LE };
    const auto bytes = enc.encode(text);
    ASSERT_TRUE(bytes.has_value());
    // UTF-16 encoding should NOT include a BOM — that's the document-level concern.
    // First 2 bytes: 'T' in UTF-16 LE = 0x54 0x00, not BOM (0xFF 0xFE).
    EXPECT_EQ(static_cast<unsigned char>(bytes->data()[0]), 0x54);
    EXPECT_EQ(static_cast<unsigned char>(bytes->data()[1]), 0x00);

    const auto back = enc.decode(bytes->data(), bytes->length());
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, text);
}

TEST_F(TextEncodingTests, RoundTripUtf16Be) {
    const wxString text = wxString::FromUTF8("Test \xC3\xA9 string");
    const TextEncoding enc { TextEncoding::UTF16_BE };
    const auto bytes = enc.encode(text);
    ASSERT_TRUE(bytes.has_value());
    // First 2 bytes: 0x00 0x54 ('T' in UTF-16 BE), not BOM
    EXPECT_EQ(static_cast<unsigned char>(bytes->data()[0]), 0x00);
    EXPECT_EQ(static_cast<unsigned char>(bytes->data()[1]), 0x54);

    const auto back = enc.decode(bytes->data(), bytes->length());
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, text);
}

TEST_F(TextEncodingTests, RoundTripWindows1252) {
    // "café" — "caf" ASCII + "é" = 0xE9 in Windows-1252
    const wxString text = wxString::FromUTF8("caf\xC3\xA9");
    const TextEncoding enc { TextEncoding::Windows_1252 };
    const auto bytes = enc.encode(text);
    ASSERT_TRUE(bytes.has_value());
    ASSERT_EQ(bytes->length(), 4u);
    EXPECT_EQ(static_cast<unsigned char>(bytes->data()[3]), 0xE9);

    const auto back = enc.decode(bytes->data(), bytes->length());
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, text);
}

TEST_F(TextEncodingTests, RoundTripCp437) {
    // CP437 — original IBM PC / DOS codepage. ASCII round-trip must be clean.
    const wxString text = wxString("Hello, World!");
    const TextEncoding enc { TextEncoding::CP437 };
    const auto bytes = enc.encode(text);
    ASSERT_TRUE(bytes.has_value());
    const auto back = enc.decode(bytes->data(), bytes->length());
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, text);
}

TEST_F(TextEncodingTests, DecodeInvalidUtf8ReturnsNullopt) {
    // Invalid continuation byte after 0xC3
    const unsigned char bytes[] = { 'h', 'i', 0xC3, 0xFF };
    const auto str = TextEncoding { TextEncoding::UTF8 }.decode(bytes, sizeof(bytes));
    EXPECT_FALSE(str.has_value());
}
