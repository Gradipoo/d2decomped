// SPDX-License-Identifier: GPL-3.0-or-later
// test-links: game
// A partial data load must not discard the player's corpse at save time.
#include <gamedata.hpp>
#include <world.hpp>

#include <cassert>

int main() {
    d2d::game::GameData data;
    d2d::game::World world(&data, 5, 5);
    world.actor->corpses.resize(3);
    world.actor->corpses[0].items.push_back({ .code = "cap" });
    world.actor->corpses[1].items.push_back({ .code = "hax" });
    const auto* saved = world.saved_corpse();
    assert(saved && saved->size() == 1 && saved->front().code == "hax");
    data.mon_bin.resize(0x202, 7);
    saved = world.saved_corpse();
    assert(saved && saved->size() == 1 && saved->front().code == "hax");
    world.actor->corpses[1].items.clear();
    saved = world.saved_corpse();
    assert(saved && saved->size() == 1 && saved->front().code == "cap");
}
