/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */
#include "SourceLexer.hpp"
using namespace fbide::FB::Parser;

constexpr size_t DEFAULT_RESERVE = 25 * 500;

SourceLexer::SourceLexer() {
    m_tokens.reserve(DEFAULT_RESERVE);
}

void SourceLexer::Insert(int pos, char ch) noexcept {
    Shift(pos, 1);
    LOG_MESSAGE("Insert at: %d, char: %c", pos, ch);
}

void SourceLexer::Insert(int pos, const std::string_view& text) noexcept {
    Shift(pos, static_cast<int>(text.length()));
    LOG_MESSAGE("Insert at: %d, char: %s", pos, text.data());
}

void SourceLexer::Remove(int pos, int len) noexcept {
    Unshift(pos, len);
    LOG_MESSAGE("Remove at: %d, len: %d", pos, len);
}

/**
 * shift all positions by given amount from position onward
 */
void SourceLexer::Shift(int pos, int len) noexcept {
    auto iter = m_tokens.begin();
    for (; iter != m_tokens.end(); iter++) {
        if (iter->pos >= pos) {
            break;
        }
    }

    for (; iter != m_tokens.end(); iter++) {
        iter->pos += len;
    }
}

/**
 * Unshift all positions bu given amount from bosition onward.
 *
 */
void SourceLexer::Unshift(int pos, int len) noexcept {
    auto iter = m_tokens.begin();
    for (; iter != m_tokens.end(); iter++) {
        if (iter->pos >= pos) {
            break;
        }
    }

    auto begin = iter;
    for (; iter != m_tokens.end(); iter++) {
        if (iter->pos >= pos + len) {
            break;
        }
    }

    if (begin != iter) {
        // TODO: Remove symbols for identifiers
        iter = m_tokens.erase(begin, iter);
    }

    for (; iter != m_tokens.end(); iter++) {
        iter->pos -= len;
    }
}

SourceLexer::~SourceLexer() = default;
