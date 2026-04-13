#include "lib/compiler/CompileCommand.hpp"
#include <gtest/gtest.h>
using namespace fbide;

TEST(CompileCommandTests, SimpleCommand) {
    const auto cmd = CompileCommand::makeDefault("/usr/bin/fbc", "main.bas");
    EXPECT_EQ(cmd.build(), R"("/usr/bin/fbc" "main.bas")");
}

TEST(CompileCommandTests, TrimWhitespace) {
    const auto cmd = CompileCommand::makeDefault("  /usr/bin/fbc  ", "  main.bas  ");
    EXPECT_EQ(cmd.build(), R"("/usr/bin/fbc" "main.bas")");
}

TEST(CompileCommandTests, AlreadyQuoted) {
    const auto cmd = CompileCommand::makeDefault(R"(  "/usr/bin/fbc"  )", R"("main.bas")");
    EXPECT_EQ(cmd.build(), R"("/usr/bin/fbc" "main.bas")");
}

TEST(CompileCommandTests, EscapeInnerQuotes) {
    const auto cmd = CompileCommand::makeDefault("/usr/bin/fbc", R"(say "hello".bas)");
    EXPECT_EQ(cmd.build(), R"("/usr/bin/fbc" "say \"hello\".bas")");
}

TEST(CompileCommandTests, EscapeInnerQuotesInAlreadyQuoted) {
    const auto cmd = CompileCommand::makeDefault("/usr/bin/fbc", R"("say "hello".bas")");
    EXPECT_EQ(cmd.build(), R"("/usr/bin/fbc" "say \"hello\".bas")");
}

TEST(CompileCommandTests, PreserveAlreadyEscapedQuotes) {
    const auto cmd = CompileCommand::makeDefault("/usr/bin/fbc", R"(say \"hello\".bas)");
    EXPECT_EQ(cmd.build(), R"("/usr/bin/fbc" "say \"hello\".bas")");
}

TEST(CompileCommandTests, ExtraFlags) {
    CompileCommand cmd;
    cmd.setCompiler("/usr/bin/fbc");
    cmd.setExtra("v");
    cmd.setExtra("lang", "fb");
    cmd.addFile("main.bas");
    const auto result = cmd.build();
    EXPECT_TRUE(result.Contains("-v"));
    EXPECT_TRUE(result.Contains("-lang fb"));
}

TEST(CompileCommandTests, WindowsPaths) {
    const auto cmd = CompileCommand::makeDefault(R"(C:\FreeBASIC\fbc.exe)", R"(D:\My Projects\main.bas)");
    EXPECT_EQ(cmd.build(), R"("C:\FreeBASIC\fbc.exe" "D:\My Projects\main.bas")");
}

TEST(CompileCommandTests, WindowsPathsAlreadyQuoted) {
    const auto cmd = CompileCommand::makeDefault(R"("C:\Program Files\FreeBASIC\fbc.exe")", "main.bas");
    EXPECT_EQ(cmd.build(), R"("C:\Program Files\FreeBASIC\fbc.exe" "main.bas")");
}
