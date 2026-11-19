// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "loot_radial.hpp"
#include "touch_input.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace d2d::client {

struct LootChoice { int id = -1; std::string name; std::array<std::uint8_t, 3> rgb{}; };

struct LootChooser {
    bool open = false, touch = false, ready = false;
    std::vector<LootChoice> choices;
    loot_radial::Model radial{ std::span<const int>{} };
    std::size_t scroll = 0;
    std::int64_t finger = -1;
    float start_y = 0;
    int row = -1;
    bool moved = false;

    void clear() { *this = {}; }
    void start(std::vector<LootChoice> items, bool use_touch) {
        clear(); choices = std::move(items); touch = use_touch; open = true;
        std::vector<int> ids; ids.reserve(choices.size());
        for (const auto& choice : choices) ids.push_back(choice.id);
        radial.reset(ids);
    }
    [[nodiscard]] static touch::Box list_box() {
        const float width = std::min(360.f, touch::viewport_width - touch::safe_inset.left - touch::safe_inset.right - 24.f);
        return { std::clamp(touch::viewport_width - touch::safe_inset.right - width - 18.f, touch::safe_inset.left + 6.f,
                            touch::viewport_width - touch::safe_inset.right - width - 6.f),
                 touch::safe_inset.top + 150.f, width, 288.f };
    }
    [[nodiscard]] int touch_event(std::int64_t id, touch::Phase phase, float x, float y) {
        const auto box = list_box();
        if (phase == touch::Phase::Down && finger == -1) {
            finger = id; start_y = y; moved = false;
            row = box.contains(x, y) ? int((y - box.y) / 48.f) : -1;
        }
        if (id != finger) return -1;
        if (phase == touch::Phase::Move && std::abs(y - start_y) > 12.f) moved = true;
        if (phase == touch::Phase::Cancel) { finger = -1; return -1; }
        if (phase != touch::Phase::Up) return -1;
        const int hit = box.contains(x, y) ? int((y - box.y) / 48.f) : -1;
        int result = -1;
        if (moved || std::abs(y - start_y) > 12.f) {
            const int max_scroll = std::max(0, int(choices.size()) - 6);
            const int delta = int(std::round((start_y - y) / 48.f));
            scroll = std::size_t(std::clamp(int(scroll) + delta, 0, max_scroll));
        } else if (row >= 0 && hit == row && scroll + std::size_t(row) < choices.size()) result = choices[scroll + std::size_t(row)].id;
        finger = -1;
        return result;
    }
};

} // namespace d2d::client
