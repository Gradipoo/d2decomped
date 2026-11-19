// SPDX-License-Identifier: GPL-3.0-or-later
// test-links:
// test-includes: apps/d2d
#include "loot_chooser.hpp"

#include <cassert>
#include <vector>

int main() {
    using namespace d2d::client;
    using touch::Phase;
    std::vector<LootChoice> items;
    for (int id = 0; id < 10; ++id) items.push_back({ 100 + id, "Item", { 255, 255, 255 } });
    LootChooser chooser;
    chooser.start(items, true);
    const auto box = LootChooser::list_box();
    const float x = box.x + 10.f, y = box.y + 10.f;
    assert(chooser.touch_event(1, Phase::Down, x, y) == -1);
    assert(chooser.touch_event(1, Phase::Up, x, y) == 100);
    assert(chooser.touch_event(1, Phase::Down, x, y + 100.f) == -1);
    assert(chooser.touch_event(1, Phase::Move, x, y + 4.f) == -1);
    assert(chooser.touch_event(1, Phase::Up, x, y + 4.f) == -1);
    assert(chooser.scroll == 2);
    assert(chooser.touch_event(1, Phase::Down, x, y) == -1);
    assert(chooser.touch_event(1, Phase::Cancel, x, y) == -1);
    assert(chooser.touch_event(1, Phase::Up, x, y) == -1);
    assert(chooser.touch_event(1, Phase::Down, x, y) == -1);
    assert(chooser.touch_event(2, Phase::Up, x, y) == -1);
    assert(chooser.touch_event(1, Phase::Up, x, y) == 102);
    chooser.clear();
    assert(!chooser.open && chooser.choices.empty());
}
