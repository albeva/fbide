//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FormatSettings.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/ThemeCategory.hpp"
#include "transformers/Transform.hpp"
#include "transformers/case/CaseTransform.hpp"
#include "transformers/reformat/ReFormatter.hpp"
using namespace fbide;

auto FormatSettings::load(ConfigManager& config) -> FormatSettings {
    const auto& cfg = config.config();
    return FormatSettings {
        .reIndent = cfg.get_or("format.reindent", true),
        .reFormat = cfg.get_or("format.reformat", true),
        .alignPP = cfg.get_or("format.alignPP", false),
        .applyCase = cfg.get_or("format.applyCase", false),
    };
}

void FormatSettings::save(ConfigManager& config) const {
    auto& fmt = config.config()["format"];
    fmt["reindent"] = reIndent;
    fmt["reformat"] = reFormat;
    fmt["alignPP"] = alignPP;
    fmt["applyCase"] = applyCase;
    config.save(ConfigManager::Category::Config);
}

auto fbide::buildTransforms(Context& ctx, const FormatSettings& settings, const std::size_t baseIndent)
    -> std::vector<std::unique_ptr<Transform>> {
    std::vector<std::unique_ptr<Transform>> transforms;

    if (settings.applyCase) {
        std::array<CaseMode, kThemeKeywordGroupsCount> cases {};
        const auto& cfg = ctx.getConfigManager().keywords().at("cases");
        for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
            const auto key = wxString(getThemeCategoryName(kThemeKeywordCategories[idx]));
            cases[idx] = CaseMode::parse(cfg.get_or(key, "None").ToStdString()).value_or(CaseMode::None);
        }
        transforms.push_back(std::make_unique<CaseTransform>(cases));
    }

    if (settings.reIndent || settings.reFormat) {
        transforms.push_back(std::make_unique<reformat::ReFormatter>(reformat::FormatOptions {
            .tabSize = static_cast<std::size_t>(ctx.getConfigManager().config().get_or("editor.tabSize", 4)),
            .anchoredPP = settings.reIndent && settings.alignPP,
            .reIndent = settings.reIndent,
            .reFormat = settings.reFormat,
            .baseIndent = baseIndent,
        }));
    }

    return transforms;
}
