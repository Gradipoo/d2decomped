// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace d2d::client::touch {

enum class UiClick : std::size_t {
    Inventory, Skills, Stats, Quest, Bind, Back, Map, Belt, Run, Destination, WheelPage, BindSet,
    Assignment, TreeAssign, Count
};

/** Rate limit repeated clicks for the same accepted action and destination. */
class UiClickThrottle {
public:
    static constexpr std::uint32_t kRepeatMs = 100;

    bool accept(UiClick action, int destination, std::uint32_t now_ms) {
        auto& last = clicks_[std::size_t(action)];
        if (last.has_time && last.destination == destination && now_ms - last.time < kRepeatMs) return false;
        last = { destination, now_ms, true };
        return true;
    }

private:
    struct Click {
        int destination = -1;
        std::uint32_t time = 0;
        bool has_time = false;
    };
    std::array<Click, std::size_t(UiClick::Count)> clicks_{};
};

} // namespace d2d::client::touch
