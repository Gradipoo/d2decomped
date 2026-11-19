// SPDX-License-Identifier: GPL-3.0-or-later
// test-links:
// test-includes: apps/d2d
#include "touch_icon.hpp"

#include <cassert>
#include <cmath>

int main() {
    using namespace d2d::client::touch;
    const auto square = circular_icon_rect(10, 20, 48, 48, 64, 32);
    assert(square.width == 42 && square.height == 21);
    assert(std::abs(square.scale - 42.f / 64.f) < .001f);
    assert(square.x == 13 && square.y == 34);
    const auto wide = circular_icon_rect(0, 0, 80, 48, 32, 64);
    assert(wide.width == 21 && wide.height == 42); // circle is bounded by the short side
    assert(inside_icon_circle(23, 43, 0, 0, 48, 48));
    assert(!inside_icon_circle(0, 0, 0, 0, 48, 48));
    assert(!inside_icon_circle(24, 24, 0, 0, 48, 48, 24)); // no area inside a full rim
    assert(circular_icon_rect(0, 0, 8, 8, 16, 16, 4).scale == 0.f);
}
