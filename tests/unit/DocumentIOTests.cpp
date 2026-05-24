//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include <wx/file.h>
#include <wx/filename.h>
#include <gtest/gtest.h>
#include "document/DocumentIO.hpp"
#include "document/DocumentPath.hpp"

using namespace fbide;

namespace {

auto makeTempPath(const char* prefix) -> std::filesystem::path {
    return toFsPath(wxFileName::CreateTempFileName(prefix));
}

auto writeTempFile(const void* bytes, const std::size_t len) -> std::filesystem::path {
    const auto path = makeTempPath("fbide_io");
    wxFile f(toWxString(path), wxFile::write);
    f.Write(bytes, len);
    f.Close();
    return path;
}

auto readAllBytes(const std::filesystem::path& path) -> std::vector<unsigned char> {
    wxFile f(toWxString(path), wxFile::read);
    const auto len = static_cast<std::size_t>(f.Length());
    std::vector<unsigned char> out(len);
    f.Read(out.data(), len);
    return out;
}

void removeTempFile(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
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

    removeTempFile(path);
}

TEST_F(DocumentIOTests, LoadUtf8WithBomStripsBom) {
    const unsigned char bytes[] = { 0xEF, 0xBB, 0xBF, 'h', 'i', '\n' };
    const auto path = writeTempFile(bytes, sizeof(bytes));

    const auto result = DocumentIO::load(path, kUtf8, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::UTF8_BOM);
    // BOM stripped from decoded text
    EXPECT_EQ(result->text, wxString("hi\n"));

    removeTempFile(path);
}

TEST_F(DocumentIOTests, LoadUtf16LeWithBom) {
    // BOM (FF FE) + "hi" in UTF-16 LE
    const unsigned char bytes[] = { 0xFF, 0xFE, 'h', 0x00, 'i', 0x00 };
    const auto path = writeTempFile(bytes, sizeof(bytes));

    const auto result = DocumentIO::load(path, kUtf8, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::UTF16_LE);
    EXPECT_EQ(result->text, wxString("hi"));

    removeTempFile(path);
}

TEST_F(DocumentIOTests, LoadWindows1252FallbackWhenNotUtf8) {
    // "café" in Windows-1252: c=0x63 a=0x61 f=0x66 é=0xE9 (invalid UTF-8 sequence)
    const unsigned char bytes[] = { 'c', 'a', 'f', 0xE9, '\n' };
    const auto path = writeTempFile(bytes, sizeof(bytes));

    const auto result = DocumentIO::load(path, kWin1252, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::Windows_1252);
    EXPECT_EQ(result->text, wxString::FromUTF8("caf\xC3\xA9\n"));

    removeTempFile(path);
}

TEST_F(DocumentIOTests, LoadAsciiFallsBackToDefault) {
    const char bytes[] = "pure ascii\n";
    const auto path = writeTempFile(bytes, sizeof(bytes) - 1);

    // ASCII is ambiguous — should pick the default
    const auto result = DocumentIO::load(path, kWin1252, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::Windows_1252);

    removeTempFile(path);
}

TEST_F(DocumentIOTests, LoadReturnsNulloptOnMissingFile) {
    const auto result = DocumentIO::load("does/not/exist.bas", kUtf8, kLf);
    EXPECT_FALSE(result.has_value());
}

TEST_F(DocumentIOTests, LoadEmptyFileUsesDefault) {
    const auto path = writeTempFile("", 0);
    const auto result = DocumentIO::load(path, kUtf8, kCrlf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::UTF8);
    EXPECT_EQ(result->eolMode, EolMode::CRLF);
    EXPECT_TRUE(result->text.empty());
    removeTempFile(path);
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

    removeTempFile(path);
}

TEST_F(DocumentIOTests, LoadDetectsCrlf) {
    const char bytes[] = "line1\r\nline2\r\nline3\r\n";
    const auto path = writeTempFile(bytes, sizeof(bytes) - 1);

    const auto result = DocumentIO::load(path, kUtf8, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->eolMode, EolMode::CRLF);

    removeTempFile(path);
}

TEST_F(DocumentIOTests, LoadFallsBackToDefaultEolWhenNoLineBreaks) {
    const char bytes[] = "single line no break";
    const auto path = writeTempFile(bytes, sizeof(bytes) - 1);

    const auto result = DocumentIO::load(path, kUtf8, kCrlf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->eolMode, EolMode::CRLF);

    removeTempFile(path);
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

TEST_F(DocumentIOTests, SaveUtf8NoBom) {
    const auto path = makeTempPath("fbide_save");
    const auto ok = DocumentIO::save(path, wxString("hello\n"), kUtf8, kLf);
    ASSERT_EQ(ok, DocumentIO::SaveResult::Success);

    const auto bytes = readAllBytes(path);
    ASSERT_EQ(bytes.size(), 6u);
    EXPECT_EQ(bytes[0], 'h');
    EXPECT_EQ(bytes[5], '\n');

    removeTempFile(path);
}

TEST_F(DocumentIOTests, SaveUtf8BomPrependsBom) {
    const auto path = makeTempPath("fbide_save");
    const auto ok = DocumentIO::save(path, wxString("hi"), TextEncoding { TextEncoding::UTF8_BOM }, kLf);
    ASSERT_EQ(ok, DocumentIO::SaveResult::Success);

    const auto bytes = readAllBytes(path);
    ASSERT_EQ(bytes.size(), 5u);
    EXPECT_EQ(bytes[0], 0xEF);
    EXPECT_EQ(bytes[1], 0xBB);
    EXPECT_EQ(bytes[2], 0xBF);
    EXPECT_EQ(bytes[3], 'h');
    EXPECT_EQ(bytes[4], 'i');

    removeTempFile(path);
}

TEST_F(DocumentIOTests, SaveUtf16LePrependsBom) {
    const auto path = makeTempPath("fbide_save");
    const auto ok = DocumentIO::save(path, wxString("hi"), TextEncoding { TextEncoding::UTF16_LE }, kLf);
    ASSERT_EQ(ok, DocumentIO::SaveResult::Success);

    const auto bytes = readAllBytes(path);
    // BOM (2) + "hi" in UTF-16 LE (4) = 6 bytes
    ASSERT_EQ(bytes.size(), 6u);
    EXPECT_EQ(bytes[0], 0xFF);
    EXPECT_EQ(bytes[1], 0xFE);
    EXPECT_EQ(bytes[2], 'h');
    EXPECT_EQ(bytes[3], 0x00);
    EXPECT_EQ(bytes[4], 'i');
    EXPECT_EQ(bytes[5], 0x00);

    removeTempFile(path);
}

TEST_F(DocumentIOTests, SaveConvertsLfToCrlf) {
    const auto path = makeTempPath("fbide_save");
    const auto ok = DocumentIO::save(path, wxString("a\nb\nc"), kUtf8, kCrlf);
    ASSERT_EQ(ok, DocumentIO::SaveResult::Success);

    const auto bytes = readAllBytes(path);
    // Expect: "a\r\nb\r\nc" = 7 bytes
    ASSERT_EQ(bytes.size(), 7u);
    EXPECT_EQ(bytes[1], '\r');
    EXPECT_EQ(bytes[2], '\n');
    EXPECT_EQ(bytes[4], '\r');
    EXPECT_EQ(bytes[5], '\n');

    removeTempFile(path);
}

TEST_F(DocumentIOTests, SaveConvertsCrlfToLf) {
    const auto path = makeTempPath("fbide_save");
    const auto ok = DocumentIO::save(path, wxString("a\r\nb\r\nc"), kUtf8, kLf);
    ASSERT_EQ(ok, DocumentIO::SaveResult::Success);

    const auto bytes = readAllBytes(path);
    // Expect: "a\nb\nc" = 5 bytes
    ASSERT_EQ(bytes.size(), 5u);
    EXPECT_EQ(bytes[1], '\n');
    EXPECT_EQ(bytes[3], '\n');

    removeTempFile(path);
}

TEST_F(DocumentIOTests, SaveWindows1252EncodesNonAscii) {
    const auto path = makeTempPath("fbide_save");
    const wxString text = wxString::FromUTF8("caf\xC3\xA9");
    const auto ok = DocumentIO::save(path, text, kWin1252, kLf);
    ASSERT_EQ(ok, DocumentIO::SaveResult::Success);

    const auto bytes = readAllBytes(path);
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[3], 0xE9); // é in Windows-1252

    removeTempFile(path);
}

TEST_F(DocumentIOTests, SaveReportsEncodingErrorWhenCharsUnrepresentable) {
    // Japanese chars are not in Windows-1252 — wxCSConv is strict so
    // encode returns nullopt and save reports EncodingError without
    // touching the file.
    const auto path = makeTempPath("fbide_save");
    const wxString text = wxString::FromUTF8("hello \xE6\x97\xA5\xE6\x9C\xAC"); // "日本"
    const auto ok = DocumentIO::save(path, text, kWin1252, kLf);
    EXPECT_EQ(ok, DocumentIO::SaveResult::EncodingError);

    removeTempFile(path);
}

TEST_F(DocumentIOTests, SaveUtf8HandlesUnicode) {
    const auto path = makeTempPath("fbide_save");
    const wxString text = wxString::FromUTF8("hello \xE6\x97\xA5\xE6\x9C\xAC");
    const auto ok = DocumentIO::save(path, text, kUtf8, kLf);
    ASSERT_EQ(ok, DocumentIO::SaveResult::Success);

    removeTempFile(path);
}

// ---------------------------------------------------------------------------
// Round-trip
// ---------------------------------------------------------------------------

TEST_F(DocumentIOTests, RoundTripUtf8BomAndCrlf) {
    const auto path = makeTempPath("fbide_rt");
    const wxString text = wxString::FromUTF8("Hello \xC3\xA9\nSecond line\n");
    const auto enc = TextEncoding { TextEncoding::UTF8_BOM };
    const auto eol = kCrlf;

    ASSERT_EQ(DocumentIO::save(path, text, enc, eol), DocumentIO::SaveResult::Success);

    const auto result = DocumentIO::load(path, kUtf8, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::UTF8_BOM);
    EXPECT_EQ(result->eolMode, EolMode::CRLF);
    // Text preserved (with CRLF) modulo EOL conversion
    EXPECT_TRUE(result->text.Contains("Hello"));

    removeTempFile(path);
}

TEST_F(DocumentIOTests, RoundTripWindows1252) {
    const auto path = makeTempPath("fbide_rt");
    const wxString text = wxString::FromUTF8("caf\xC3\xA9 na\xC3\xAFve\n");

    ASSERT_EQ(DocumentIO::save(path, text, kWin1252, kLf), DocumentIO::SaveResult::Success);

    const auto result = DocumentIO::load(path, kWin1252, kLf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->encoding, TextEncoding::Windows_1252);
    EXPECT_EQ(result->text, text);

    removeTempFile(path);
}
