//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "KeywordTables.hpp"
using namespace fbide::lexer;

auto fbide::lexer::structuralKeywords() -> const std::unordered_map<std::string, KeywordKind>& {
    static const std::unordered_map<std::string, KeywordKind> map = {
        // Block openers
        { "sub", KeywordKind::Sub },
        { "function", KeywordKind::Function },
        { "constructor", KeywordKind::Constructor },
        { "destructor", KeywordKind::Destructor },
        { "operator", KeywordKind::Operator },
        { "do", KeywordKind::Do },
        { "while", KeywordKind::While },
        { "for", KeywordKind::For },
        { "with", KeywordKind::With },
        { "scope", KeywordKind::Scope },
        { "enum", KeywordKind::Enum },
        { "union", KeywordKind::Union },
        { "select", KeywordKind::Select },
        { "asm", KeywordKind::Asm },
        { "namespace", KeywordKind::Namespace },
        // Block closers
        { "end", KeywordKind::End },
        { "endif", KeywordKind::End },
        { "loop", KeywordKind::Loop },
        { "next", KeywordKind::Next },
        { "wend", KeywordKind::Wend },
        // Mid-block
        { "else", KeywordKind::Else },
        { "elseif", KeywordKind::ElseIf },
        { "case", KeywordKind::Case },
        // Conditional
        { "if", KeywordKind::If },
        { "then", KeywordKind::Then },
        // Type
        { "type", KeywordKind::Type },
        { "as", KeywordKind::As },
        // Declaration
        { "declare", KeywordKind::Declare },
        // Early-exit (keeps following block keyword from opening a scope)
        { "exit", KeywordKind::Exit },
        { "continue", KeywordKind::Continue },
    };
    return map;
}

auto fbide::lexer::structuralKeywordsList() -> const std::string& {
    static const std::string list = [] {
        std::string out;
        for (const auto& text : structuralKeywords() | std::views::keys) {
            if (!out.empty())
                out += ' ';
            out += text;
        }
        return out;
    }();
    return list;
}

auto fbide::lexer::ppKeywords() -> const std::unordered_map<std::string, KeywordKind>& {
    static const std::unordered_map<std::string, KeywordKind> map = {
        // Block openers
        { "if", KeywordKind::PpIf },
        { "ifdef", KeywordKind::PpIfDef },
        { "ifndef", KeywordKind::PpIfNDef },
        { "macro", KeywordKind::PpMacro },
        // Block closers
        { "endif", KeywordKind::PpEndIf },
        { "endmacro", KeywordKind::PpEndMacro },
        // Mid-block
        { "else", KeywordKind::PpElse },
        { "elseif", KeywordKind::PpElseIf },
        { "elseifdef", KeywordKind::PpElseIfDef },
        { "elseifndef", KeywordKind::PpElseIfNDef },
        // Non-block directives we care to classify
        { "include", KeywordKind::PpInclude },
    };
    return map;
}
