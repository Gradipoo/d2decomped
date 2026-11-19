// SPDX-License-Identifier: GPL-3.0-or-later
// Shared display and release validation for predicted Teleport destinations.
#pragma once

#include "touch_input.hpp"

#include <cmath>

namespace d2d::client {

[[nodiscard]] inline std::pair<float, float> teleport_stick_drag(float x, float y, float magnitude) {
    const float amount = std::clamp(magnitude, 0.f, 1.f);
    return { x * amount * 185.f, y * amount * 125.f };
}

struct TeleportAim {
    int button = -1, item = -1;
    touch::Target preview{}, displayed{};
    float dx = 0, dy = 0;
    bool manual = false, shown = false;

    void clear() { *this = {}; }
    void display() { displayed = preview; shown = true; }
    [[nodiscard]] bool matches(const touch::Target& current) const {
        return shown && displayed.valid && current.valid && displayed.skill == current.skill
            && std::hypot(displayed.x - current.x, displayed.y - current.y) < .01f;
    }
};

} // namespace d2d::client
