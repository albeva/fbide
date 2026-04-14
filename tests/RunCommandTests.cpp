#include "lib/compiler/RunCommand.hpp"
#include "lib/compiler/QuoteUtils.hpp"
#include <gtest/gtest.h>
#include <wx/filename.h>
using namespace fbide;

// RunCommand uses wxFileName internally, which normalizes path separators
// to the platform native format. Helper to get the expected native path.
static auto nativePath(const wxString& path) -> wxString {
    return wxFileName(path).GetFullPath();
}

class RunCommandTests : public testing::Test {};

TEST_F(RunCommandTests, DefaultTemplate) {
    const auto expected = nativePath("/usr/bin/hello");
    const auto cmd = RunCommand::makeDefault("/usr/bin/hello");
    EXPECT_EQ(cmd.build(R"(<$terminal> "<$file>" <$param>)", "open -a Terminal", "--verbose"),
        wxString::Format(R"(open -a Terminal "%s" --verbose)", expected));
}

TEST_F(RunCommandTests, NoTerminal) {
    const auto expected = nativePath("/usr/bin/hello");
    const auto cmd = RunCommand::makeDefault("/usr/bin/hello");
    EXPECT_EQ(cmd.build(R"("<$file>" <$param>)", "", "--verbose"),
        wxString::Format(R"("%s" --verbose)", expected));
}

TEST_F(RunCommandTests, FilePathParts) {
    const wxFileName file("/home/user/project/main.exe");
    const auto cmd = RunCommand::makeDefault("/home/user/project/main.exe");
    EXPECT_EQ(cmd.build("<$file_path>/<$file_name>.<$file_ext>"),
        file.GetPath() + "/" + file.GetName() + "." + file.GetExt());
}

TEST_F(RunCommandTests, EmptyParameters) {
    const auto expected = nativePath("/usr/bin/hello");
    const auto cmd = RunCommand::makeDefault("/usr/bin/hello");
    EXPECT_EQ(cmd.build(R"(<$terminal> "<$file>" <$param>)", "xterm"),
        wxString::Format(R"(xterm "%s")", expected));
}

TEST_F(RunCommandTests, WindowsPath) {
    const auto cmd = RunCommand::makeDefault(R"(C:\Projects\hello.exe)");
    EXPECT_EQ(cmd.build(R"("<$file>" <$param>)", "", "-arg1"), R"("C:\Projects\hello.exe" -arg1)");
}

TEST_F(RunCommandTests, EscapeInnerQuotes) {
    const auto expected = escapeQuotes(nativePath(R"(/path/to/say "hello".exe)"));
    const auto cmd = RunCommand::makeDefault(R"(/path/to/say "hello".exe)");
    EXPECT_EQ(cmd.build(R"("<$file>")"), wxString::Format(R"("%s")", expected));
}

TEST_F(RunCommandTests, PreserveAlreadyEscapedQuotes) {
    const auto expected = escapeQuotes(nativePath(R"(/path/to/say \"hello\".exe)"));
    const auto cmd = RunCommand::makeDefault(R"(/path/to/say \"hello\".exe)");
    EXPECT_EQ(cmd.build(R"("<$file>")"), wxString::Format(R"("%s")", expected));
}
