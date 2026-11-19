// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "touch_input.hpp"
#include <algorithm>
#include <cstdint>
#include <vector>

#include <world.hpp>

namespace d2d::client::touch {
inline Box effect_box(const Layout& layout, std::size_t index) {
    return { layout.quest_track.x + layout.quest_track.width + 6.f, layout.quest_track.y + float(index) * 34.f, 30.f, 30.f };
}
inline Box effect_confirm() { return { to_viewport_x(310.f), 300.f, 100.f, 44.f }; }
inline Box effect_cancel() { return { to_viewport_x(420.f), 300.f, 100.f, 44.f }; }
inline std::size_t effect_capacity(const Layout& layout) {
    return std::size_t(std::max(0.f, (layout.move.y - layout.quest_track.y) / 34.f));
}
inline std::size_t effects_per_page(std::size_t count, std::size_t capacity) {
    return count > capacity && capacity > 1 ? capacity - 1 : capacity;
}
inline std::size_t effect_page_start(std::size_t count, std::size_t capacity, std::size_t page) {
    const auto size = effects_per_page(count, capacity);
    if (size == 0) return 0;
    const auto pages = (count + size - 1) / size;
    return pages ? (page % pages) * size : 0;
}
struct EffectGesture {
    std::size_t page = 0;
    std::int64_t finger = -1;
    int kind = -1, last_kind = -1;
    std::uint64_t instance = 0, last_instance = 0;
    std::uint32_t down_ms = 0, last_ms = 0;
    bool modal = false;
    void clear_press() { finger = -1; }
    const game::View::Effect* current(const std::vector<game::View::Effect>& effects) const {
        const auto found = std::ranges::find_if(effects, [&](const auto& effect) { return effect.kind == kind && effect.instance == instance; });
        return found == effects.end() ? nullptr : &*found;
    }
    bool inspecting(std::uint32_t now) const { return finger != -1 && now >= down_ms && now - down_ms >= 500; }
    void release(const std::vector<game::View::Effect>& effects, std::uint32_t now, bool inside) {
        const auto* effect = current(effects);
        if (inside && effect && now >= down_ms && now - down_ms < 500) {
            if (effect->dismissible && last_kind == effect->kind && last_instance == effect->instance
                && now >= last_ms && now - last_ms <= 350) {
                modal = true; last_kind = -1;
            } else { last_kind = effect->kind; last_instance = effect->instance; last_ms = now; }
        }
        clear_press();
    }
    const game::View::Effect* confirm(const std::vector<game::View::Effect>& effects) {
        const auto* effect = current(effects);
        modal = false;
        return effect && effect->dismissible ? effect : nullptr;
    }
    void cancel() { modal = false; clear_press(); }
};
}
