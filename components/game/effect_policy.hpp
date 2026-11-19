// SPDX-License-Identifier: GPL-3.0-or-later
// d2d's conservative policy for its optional effect dismissal.
#pragma once

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

namespace d2d::game {
inline bool dismissible_self_effect(std::string_view name) {
    return name == "Frozen Armor" || name == "Shiver Armor" || name == "Chilling Armor"
        || name == "Bone Armor" || name == "Cyclone Armor" || name == "Holy Shield"
        || name == "Shout" || name == "Battle Orders" || name == "Battle Command";
}
inline bool dismissible_shrine_effect(int code, const std::vector<std::pair<int, int>>& stats) {
    return code >= 6 && code <= 15 && !stats.empty()
        && std::ranges::all_of(stats, [](const auto& stat) { return stat.second >= 0; });
}
}  // namespace d2d::game
