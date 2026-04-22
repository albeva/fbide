//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <wx/file.h>
#include <wx/filename.h>
#include <gtest/gtest.h>
#include "editor/DocumentIO.hpp"

using namespace fbide;

namespace {

auto writeTempFile(const void* bytes, const std::size_t len) -> wxString {
    const auto path = wxFileName::CreateTempFileName("fbide_io");
    wxFile f(path, wxFile::write);
    f.Write(bytes, len);
    f.Close();
    return path;
}

auto readAllBytes(const wxString& path) -> std::vector<unsigned char> {
    wxFile f(path, wxFile::read);
    const auto len = static_cast<std::size_t>(f.Length());
    std::vector<unsigned char> out(len);
    f.Read(out.data(), len);
    return out;
}

const TextEncoding kUtf8 { TextEncoding::UTF8 };
const TextEncoding kWin1252 { TextEncoding::Windows_1252 };
const EolMode kLf { EolMode::LF };
const EolMode kCrlf { EolMode::CRLF };

} // namespace

class DocumentIOTests : public testing::Test {};

// ---------------------------------------------------------------------------
// Load — encoding detection
// ---------------------------------------------------------------------------

TEST_F(DocumentIOTests, LoadUtf8NoBom) {
    // "hello é" — valid non-ASCII UTF-8
    const unsigned char bytes[] = { 'h', 'e', 'l', 'l', 'o', ' ', 0xC3, 0xA9, '\n' };
    const auto path = writeTempFile(bytes, sizeof(bytes));

    const auto result = DocumentIO::load(path, kWin1252, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::UTF8);
    EXPECT_EQ(result->text, wxString::FromUTF8("hello \xC3\xA9\n"));

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, LoadUtf8WithBomStripsBom) {
    const unsigned char bytes[] = { 0xEF, 0xBB, 0xBF, 'h', 'i', '\n' };
    const auto path = writeTempFile(bytes, sizeof(bytes));

    const auto result = DocumentIO::load(path, kUtf8, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::UTF8_BOM);
    // BOM stripped from decoded text
    EXPECT_EQ(result->text, wxString("hi\n"));

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, LoadUtf16LeWithBom) {
    // BOM (FF FE) + "hi" in UTF-16 LE
    const unsigned char bytes[] = { 0xFF, 0xFE, 'h', 0x00, 'i', 0x00 };
    const auto path = writeTempFile(bytes, sizeof(bytes));

    const auto result = DocumentIO::load(path, kUtf8, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::UTF16_LE);
    EXPECT_EQ(result->text, wxString("hi"));

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, LoadWindows1252FallbackWhenNotUtf8) {
    // "café" in Windows-1252: c=0x63 a=0x61 f=0x66 é=0xE9 (invalid UTF-8 sequence)
    const unsigned char bytes[] = { 'c', 'a', 'f', 0xE9, '\n' };
    const auto path = writeTempFile(bytes, sizeof(bytes));

    const auto result = DocumentIO::load(path, kWin1252, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::Windows_1252);
    EXPECT_EQ(result->text, wxString::FromUTF8("caf\xC3\xA9\n"));

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, LoadAsciiFallsBackToDefault) {
    const char bytes[] = "pure ascii\n";
    const auto path = writeTempFile(bytes, sizeof(bytes) - 1);

    // ASCII is ambiguous — should pick the default
    const auto result = DocumentIO::load(path, kWin1252, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::Windows_1252);

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, LoadReturnsNulloptOnMissingFile) {
    const auto result = DocumentIO::load("C:/this/file/does/not/exist.bas", kUtf8, kLf);
    EXPECT_FALSE(result.has_value());
}

TEST_F(DocumentIOTests, LoadEmptyFileUsesDefault) {
    const auto path = writeTempFile("", 0);
    const auto result = DocumentIO::load(path, kUtf8, kCrlf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::UTF8);
    EXPECT_EQ(result->eolMode, EolMode::CRLF);
    EXPECT_TRUE(result->text.empty());
    wxRemoveFile(path);
}

// ---------------------------------------------------------------------------
// Load — EOL detection
// ---------------------------------------------------------------------------

TEST_F(DocumentIOTests, LoadDetectsLf) {
    const char bytes[] = "line1\nline2\nline3\n";
    const auto path = writeTempFile(bytes, sizeof(bytes) - 1);

    const auto result = DocumentIO::load(path, kUtf8, kCrlf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->eolMode, EolMode::LF);

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, LoadDetectsCrlf) {
    const char bytes[] = "line1\r\nline2\r\nline3\r\n";
    const auto path = writeTempFile(bytes, sizeof(bytes) - 1);

    const auto result = DocumentIO::load(path, kUtf8, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->eolMode, EolMode::CRLF);

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, LoadFallsBackToDefaultEolWhenNoLineBreaks) {
    const char bytes[] = "single line no break";
    const auto path = writeTempFile(bytes, sizeof(bytes) - 1);

    const auto result = DocumentIO::load(path, kUtf8, kCrlf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->eolMode, EolMode::CRLF);

    wxRemoveFile(path);
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

TEST_F(DocumentIOTests, SaveUtf8NoBom) {
    const auto path = wxFileName::CreateTempFileName("fbide_save");
    const bool ok = DocumentIO::save(path, wxString("hello\n"), kUtf8, kLf);
    ASSERT_TRUE(ok);

    const auto bytes = readAllBytes(path);
    ASSERT_EQ(bytes.size(), 6u);
    EXPECT_EQ(bytes[0], 'h');
    EXPECT_EQ(bytes[5], '\n');

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, SaveUtf8BomPrependsBom) {
    const auto path = wxFileName::CreateTempFileName("fbide_save");
    const bool ok = DocumentIO::save(path, wxString("hi"), TextEncoding { TextEncoding::UTF8_BOM }, kLf);
    ASSERT_TRUE(ok);

    const auto bytes = readAllBytes(path);
    ASSERT_EQ(bytes.size(), 5u);
    EXPECT_EQ(bytes[0], 0xEF);
    EXPECT_EQ(bytes[1], 0xBB);
    EXPECT_EQ(bytes[2], 0xBF);
    EXPECT_EQ(bytes[3], 'h');
    EXPECT_EQ(bytes[4], 'i');

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, SaveUtf16LePrependsBom) {
    const auto path = wxFileName::CreateTempFileName("fbide_save");
    const bool ok = DocumentIO::save(path, wxString("hi"), TextEncoding { TextEncoding::UTF16_LE }, kLf);
    ASSERT_TRUE(ok);

    const auto bytes = readAllBytes(path);
    // BOM (2) + "hi" in UTF-16 LE (4) = 6 bytes
    ASSERT_EQ(bytes.size(), 6u);
    EXPECT_EQ(bytes[0], 0xFF);
    EXPECT_EQ(bytes[1], 0xFE);
    EXPECT_EQ(bytes[2], 'h');
    EXPECT_EQ(bytes[3], 0x00);
    EXPECT_EQ(bytes[4], 'i');
    EXPECT_EQ(bytes[5], 0x00);

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, SaveConvertsLfToCrlf) {
    const auto path = wxFileName::CreateTempFileName("fbide_save");
    const bool ok = DocumentIO::save(path, wxString("a\nb\nc"), kUtf8, kCrlf);
    ASSERT_TRUE(ok);

    const auto bytes = readAllBytes(path);
    // Expect: "a\r\nb\r\nc" = 7 bytes
    ASSERT_EQ(bytes.size(), 7u);
    EXPECT_EQ(bytes[1], '\r');
    EXPECT_EQ(bytes[2], '\n');
    EXPECT_EQ(bytes[4], '\r');
    EXPECT_EQ(bytes[5], '\n');

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, SaveConvertsCrlfToLf) {
    const auto path = wxFileName::CreateTempFileName("fbide_save");
    const bool ok = DocumentIO::save(path, wxString("a\r\nb\r\nc"), kUtf8, kLf);
    ASSERT_TRUE(ok);

    const auto bytes = readAllBytes(path);
    // Expect: "a\nb\nc" = 5 bytes
    ASSERT_EQ(bytes.size(), 5u);
    EXPECT_EQ(bytes[1], '\n');
    EXPECT_EQ(bytes[3], '\n');

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, SaveWindows1252EncodesNonAscii) {
    const auto path = wxFileName::CreateTempFileName("fbide_save");
    const wxString text = wxString::FromUTF8("caf\xC3\xA9");
    const bool ok = DocumentIO::save(path, text, kWin1252, kLf);
    ASSERT_TRUE(ok);

    const auto bytes = readAllBytes(path);
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[3], 0xE9); // é in Windows-1252

    wxRemoveFile(path);
}

// ---------------------------------------------------------------------------
// Round-trip
// ---------------------------------------------------------------------------

TEST_F(DocumentIOTests, RoundTripUtf8BomAndCrlf) {
    const auto path = wxFileName::CreateTempFileName("fbide_rt");
    const wxString text = wxString::FromUTF8("Hello \xC3\xA9\nSecond line\n");
    const auto enc = TextEncoding { TextEncoding::UTF8_BOM };
    const auto eol = kCrlf;

    ASSERT_TRUE(DocumentIO::save(path, text, enc, eol));

    const auto result = DocumentIO::load(path, kUtf8, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::UTF8_BOM);
    EXPECT_EQ(result->eolMode, EolMode::CRLF);
    // Text preserved (with CRLF) modulo EOL conversion
    EXPECT_TRUE(result->text.Contains("Hello"));

    wxRemoveFile(path);
}

TEST_F(DocumentIOTests, RoundTripWindows1252) {
    const auto path = wxFileName::CreateTempFileName("fbide_rt");
    const wxString text = wxString::FromUTF8("caf\xC3\xA9 na\xC3\xAFve\n");

    ASSERT_TRUE(DocumentIO::save(path, text, kWin1252, kLf));

    const auto result = DocumentIO::load(path, kWin1252, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::Windows_1252);
    EXPECT_EQ(result->text, text);

    wxRemoveFile(path);
}
