// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

namespace d2d::client::touch {

struct IconRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    float scale = 0.f;
};

/** Fit art inside the button's circular interior without changing its aspect ratio. */
inline IconRect circular_icon_rect(float x, float y, float width, float height,
                                   float art_width, float art_height, float rim = 3.f) {
    if (width <= 0.f || height <= 0.f || art_width <= 0.f || art_height <= 0.f) return {};
    const float diameter = std::max(0.f, std::min(width, height) - 2.f * rim);
    if (diameter <= 0.f) return {};
    const float scale = diameter / std::max(art_width, art_height);
    const int dest_width = std::max(1, int(std::lround(art_width * scale)));
    const int dest_height = std::max(1, int(std::lround(art_height * scale)));
    return { int(std::lround(x + (width - dest_width) * .5f)),
             int(std::lround(y + (height - dest_height) * .5f)), dest_width, dest_height, scale };
}

/** Pixel-center clip against the button's circular interior, leaving its rim visible. */
inline bool inside_icon_circle(int pixel_x, int pixel_y, float x, float y,
                               float width, float height, float rim = 3.f) {
    const float radius = std::max(0.f, std::min(width, height) * .5f - rim);
    const float dx = float(pixel_x) + .5f - (x + width * .5f);
    const float dy = float(pixel_y) + .5f - (y + height * .5f);
    return radius > 0.f && dx * dx + dy * dy <= radius * radius;
}

} // namespace d2d::client::touch
