// SPDX-License-Identifier: GPL-3.0-or-later
// test-links: game
// test-includes: apps/d2d
#include "touch_effects.hpp"

#include <cassert>
#include <vector>

int main() {
    using d2d::client::touch::EffectGesture;
    using d2d::game::View;
    std::vector<View::Effect> effects{ { 0, 54, 101, 1000, true } };
    assert(d2d::client::touch::effects_per_page(10, 4) == 3);
    assert(d2d::client::touch::effect_page_start(10, 4, 1) == 3);
    assert(d2d::client::touch::effect_page_start(10, 4, 4) == 0);
    const auto layout = d2d::client::touch::layout_boxes();
    const auto capacity = d2d::client::touch::effect_capacity(layout);
    assert(capacity > 1);
    const auto last_box = d2d::client::touch::effect_box(layout, capacity - 1);
    assert(last_box.y + last_box.height <= layout.move.y);
    assert(last_box.x >= layout.quest_track.x + layout.quest_track.width);
    EffectGesture gesture;
    gesture.finger = 1; gesture.kind = 0; gesture.instance = 101; gesture.down_ms = 100;
    assert(!gesture.inspecting(599));
    assert(gesture.inspecting(600));
    gesture.release(effects, 650, true);
    assert(!gesture.modal);
    gesture.finger = 1; gesture.down_ms = 700; gesture.release(effects, 750, true);
    gesture.finger = 1; gesture.down_ms = 800; gesture.release(effects, 850, true);
    assert(gesture.modal);
    assert(gesture.confirm(effects)->instance == 101 && !gesture.modal);
    gesture.modal = true; gesture.cancel(); assert(!gesture.modal);
    gesture.modal = true; effects[0].instance = 102;
    assert(!gesture.confirm(effects));
    gesture.instance = 102; effects[0].dismissible = false;
    gesture.finger = 1; gesture.down_ms = 900; gesture.release(effects, 950, true);
    gesture.finger = 1; gesture.down_ms = 1000; gesture.release(effects, 1050, true);
    assert(!gesture.modal && !gesture.confirm(effects));
    gesture.finger = 1; gesture.down_ms = 1100; gesture.release(effects, 1150, false);
    assert(gesture.finger == -1 && !gesture.modal);
    effects.clear(); gesture.modal = true; assert(!gesture.confirm(effects));
}
