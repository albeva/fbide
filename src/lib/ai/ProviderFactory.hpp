//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Value;
} // namespace fbide

namespace fbide::ai {
class AiProvider;

/// Outcome of `makeProvider` — owns the freshly-built provider (or null
/// when credentials weren't configured) and carries the model name to
/// send with each request.
struct ProviderSelection {
    std::unique_ptr<AiProvider> provider; ///< Null when required credentials missing.
    wxString model;                       ///< Default applied when config omits `model`.
};

/// Build a provider from an `[ai/<name>]` config section.
///
/// `kind` is the section's `provider` field (anthropic / ollama /
/// lm-studio / claude-cli / gemini / mock); an unrecognised kind
/// defaults to anthropic. Providers that require an API key (anthropic,
/// gemini) return a null `provider` when the `key` field is missing or
/// empty.
[[nodiscard]] auto makeProvider(const wxString& kind, const Value& config) -> ProviderSelection;

} // namespace fbide::ai
