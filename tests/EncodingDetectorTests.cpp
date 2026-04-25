//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <gtest/gtest.h>
#include "../src/lib/document/TextEncoding.hpp"
#include "editor/EncodingDetector.hpp"

using namespace fbide;

class EncodingDetectorTests : public testing::Test {};

// ---------------------------------------------------------------------------
// detectBom
// ---------------------------------------------------------------------------

TEST_F(EncodingDetectorTests, DetectBomUtf8) {
    const unsigned char bytes[] = { 0xEF, 0xBB, 0xBF, 'h', 'i' };
    const auto bom = EncodingDetector::detectBom(bytes, sizeof(bytes));
    ASSERT_TRUE(bom.has_value());
    EXPECT_EQ(*bom, TextEncoding::UTF8_BOM);
}

TEST_F(EncodingDetectorTests, DetectBomUtf16Le) {
    const unsigned char bytes[] = { 0xFF, 0xFE, 'h', 0, 'i', 0 };
    const auto bom = EncodingDetector::detectBom(bytes, sizeof(bytes));
    ASSERT_TRUE(bom.has_value());
    EXPECT_EQ(*bom, TextEncoding::UTF16_LE);
}

TEST_F(EncodingDetectorTests, DetectBomUtf16Be) {
    const unsigned char bytes[] = { 0xFE, 0xFF, 0, 'h', 0, 'i' };
    const auto bom = EncodingDetector::detectBom(bytes, sizeof(bytes));
    ASSERT_TRUE(bom.has_value());
    EXPECT_EQ(*bom, TextEncoding::UTF16_BE);
}

TEST_F(EncodingDetectorTests, DetectBomNoneWhenAbsent) {
    const unsigned char bytes[] = { 'h', 'e', 'l', 'l', 'o' };
    EXPECT_FALSE(EncodingDetector::detectBom(bytes, sizeof(bytes)).has_value());
}

TEST_F(EncodingDetectorTests, DetectBomNoneOnEmpty) {
    EXPECT_FALSE(EncodingDetector::detectBom(nullptr, 0).has_value());
}

TEST_F(EncodingDetectorTests, DetectBomNoneOnTruncatedBom) {
    // Single 0xEF — ambiguous, not enough bytes to confirm UTF-8 BOM
    const unsigned char bytes[] = { 0xEF };
    EXPECT_FALSE(EncodingDetector::detectBom(bytes, sizeof(bytes)).has_value());
}

// ---------------------------------------------------------------------------
// isValidUtf8 / isLikelyUtf8
// ---------------------------------------------------------------------------

TEST_F(EncodingDetectorTests, IsValidUtf8Ascii) {
    const char bytes[] = "Hello, World!";
    EXPECT_TRUE(EncodingDetector::isValidUtf8(bytes, sizeof(bytes) - 1));
}

TEST_F(EncodingDetectorTests, IsValidUtf8Multibyte) {
    // "é" = 0xC3 0xA9, "ñ" = 0xC3 0xB1, "漢" = 0xE6 0xBC 0xA2
    const unsigned char bytes[] = { 'h', 0xC3, 0xA9, 0xE6, 0xBC, 0xA2 };
    EXPECT_TRUE(EncodingDetector::isValidUtf8(bytes, sizeof(bytes)));
}

TEST_F(EncodingDetectorTests, IsValidUtf8Empty) {
    EXPECT_TRUE(EncodingDetector::isValidUtf8(nullptr, 0));
}

TEST_F(EncodingDetectorTests, IsValidUtf8RejectsInvalidContinuation) {
    // 0xC3 followed by 0xFF (invalid continuation byte)
    const unsigned char bytes[] = { 'h', 0xC3, 0xFF };
    EXPECT_FALSE(EncodingDetector::isValidUtf8(bytes, sizeof(bytes)));
}

TEST_F(EncodingDetectorTests, IsValidUtf8RejectsTruncated) {
    // 0xC3 with no continuation byte
    const unsigned char bytes[] = { 'h', 0xC3 };
    EXPECT_FALSE(EncodingDetector::isValidUtf8(bytes, sizeof(bytes)));
}

TEST_F(EncodingDetectorTests, IsValidUtf8RejectsLoneContinuation) {
    // 0x80 without a leading byte
    const unsigned char bytes[] = { 'h', 0x80 };
    EXPECT_FALSE(EncodingDetector::isValidUtf8(bytes, sizeof(bytes)));
}

TEST_F(EncodingDetectorTests, IsLikelyUtf8RequiresNonAscii) {
    // All ASCII — valid UTF-8 but ambiguous with Windows-1252 etc.
    const char ascii[] = "Hello, World!";
    EXPECT_FALSE(EncodingDetector::isLikelyUtf8(ascii, sizeof(ascii) - 1));

    // Contains multibyte — likely UTF-8
    const unsigned char utf8[] = { 'h', 0xC3, 0xA9 };
    EXPECT_TRUE(EncodingDetector::isLikelyUtf8(utf8, sizeof(utf8)));
}

TEST_F(EncodingDetectorTests, IsLikelyUtf8RejectsInvalid) {
    const unsigned char bytes[] = { 'h', 0xC3, 0xFF };
    EXPECT_FALSE(EncodingDetector::isLikelyUtf8(bytes, sizeof(bytes)));
}

// ---------------------------------------------------------------------------
// detectEol
// ---------------------------------------------------------------------------

TEST_F(EncodingDetectorTests, DetectEolLf) {
    const auto mode = EncodingDetector::detectEol(wxString("line1\nline2\nline3\n"));
    ASSERT_TRUE(mode.has_value());
    EXPECT_EQ(*mode, EolMode::LF);
}

TEST_F(EncodingDetectorTests, DetectEolCrlf) {
    const auto mode = EncodingDetector::detectEol(wxString("line1\r\nline2\r\nline3\r\n"));
    ASSERT_TRUE(mode.has_value());
    EXPECT_EQ(*mode, EolMode::CRLF);
}

TEST_F(EncodingDetectorTests, DetectEolCr) {
    const auto mode = EncodingDetector::detectEol(wxString("line1\rline2\rline3\r"));
    ASSERT_TRUE(mode.has_value());
    EXPECT_EQ(*mode, EolMode::CR);
}

TEST_F(EncodingDetectorTests, DetectEolMixedMajorityCrlf) {
    // 3 CRLF vs 1 LF — CRLF wins
    const auto mode = EncodingDetector::detectEol(wxString("a\r\nb\r\nc\r\nd\ne"));
    ASSERT_TRUE(mode.has_value());
    EXPECT_EQ(*mode, EolMode::CRLF);
}

TEST_F(EncodingDetectorTests, DetectEolMixedMajorityLf) {
    const auto mode = EncodingDetector::detectEol(wxString("a\nb\nc\nd\r\ne"));
    ASSERT_TRUE(mode.has_value());
    EXPECT_EQ(*mode, EolMode::LF);
}

TEST_F(EncodingDetectorTests, DetectEolEmpty) {
    EXPECT_FALSE(EncodingDetector::detectEol(wxString()).has_value());
}

TEST_F(EncodingDetectorTests, DetectEolNoLineBreaks) {
    EXPECT_FALSE(EncodingDetector::detectEol(wxString("single line")).has_value());
}

TEST_F(EncodingDetectorTests, DetectEolTieReturnsNullopt) {
    // 1 LF vs 1 CR — ambiguous. (CRLF is "\r\n", a single LF + single CR don't form one.)
    const auto mode = EncodingDetector::detectEol(wxString("a\nb\rc"));
    EXPECT_FALSE(mode.has_value());
}

// ---------------------------------------------------------------------------
// detect — full pipeline
// ---------------------------------------------------------------------------

TEST_F(EncodingDetectorTests, DetectBomWins) {
    // UTF-8 BOM — should be picked regardless of fallback
    const unsigned char bytes[] = { 0xEF, 0xBB, 0xBF, 'h', 'i' };
    const auto result = EncodingDetector::detect(bytes, sizeof(bytes), TextEncoding { TextEncoding::Windows_1252 });
    EXPECT_EQ(result.encoding, TextEncoding::UTF8_BOM);
    EXPECT_TRUE(result.hadBom);
}

TEST_F(EncodingDetectorTests, DetectUtf16LeBomOverridesFallback) {
    const unsigned char bytes[] = { 0xFF, 0xFE, 'h', 0, 'i', 0 };
    const auto result = EncodingDetector::detect(bytes, sizeof(bytes), TextEncoding { TextEncoding::UTF8 });
    EXPECT_EQ(result.encoding, TextEncoding::UTF16_LE);
    EXPECT_TRUE(result.hadBom);
}

TEST_F(EncodingDetectorTests, DetectUtf8WhenNoBomAndLikelyUtf8) {
    // No BOM, but valid non-ASCII UTF-8 → picks UTF-8 over fallback
    const unsigned char bytes[] = { 'h', 'i', 0xC3, 0xA9 };
    const auto result = EncodingDetector::detect(bytes, sizeof(bytes), TextEncoding { TextEncoding::Windows_1252 });
    EXPECT_EQ(result.encoding, TextEncoding::UTF8);
    EXPECT_FALSE(result.hadBom);
}

TEST_F(EncodingDetectorTests, DetectFallbackWhenAsciiOnly) {
    // All ASCII — ambiguous, use fallback
    const char bytes[] = "pure ascii";
    const auto result = EncodingDetector::detect(bytes, sizeof(bytes) - 1, TextEncoding { TextEncoding::Windows_1252 });
    EXPECT_EQ(result.encoding, TextEncoding::Windows_1252);
    EXPECT_FALSE(result.hadBom);
}

TEST_F(EncodingDetectorTests, DetectFallbackWhenInvalidUtf8) {
    // Invalid UTF-8 — must use fallback
    const unsigned char bytes[] = { 'h', 0xC3, 0xFF };
    const auto result = EncodingDetector::detect(bytes, sizeof(bytes), TextEncoding { TextEncoding::Windows_1252 });
    EXPECT_EQ(result.encoding, TextEncoding::Windows_1252);
    EXPECT_FALSE(result.hadBom);
}

TEST_F(EncodingDetectorTests, DetectEmptyUsesFallback) {
    const auto result = EncodingDetector::detect(nullptr, 0, TextEncoding { TextEncoding::UTF8 });
    EXPECT_EQ(result.encoding, TextEncoding::UTF8);
    EXPECT_FALSE(result.hadBom);
}

TEST_F(EncodingDetectorTests, DetectReturnsFallbackWhenNotUtf8) {
    // Invalid UTF-8 — detection returns the caller's fallback as-is.
    // Any decode failure is handled downstream in DocumentIO via Latin-1.
    const unsigned char bytes[] = { 'h', 0xC3, 0xFF };
    const auto result = EncodingDetector::detect(bytes, sizeof(bytes), TextEncoding { TextEncoding::UTF8 });
    EXPECT_EQ(result.encoding, TextEncoding::UTF8);
    EXPECT_FALSE(result.hadBom);
}
