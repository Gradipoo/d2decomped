// SPDX-License-Identifier: GPL-3.0-or-later
// test-links: game
// test-includes: apps/d2d
// A touch tap starts Howl in place with no enemy target.
#include "touch_input.hpp"

#include <character.hpp>
#include <character_store.hpp>
#include <fight.hpp>
#include <gamedata.hpp>
#include <gamedata_load.hpp>
#include <level_ids.hpp>
#include <protocol.hpp>
#include <world.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

using namespace d2d::game;

int main() {
    const char* directory = std::getenv("D2_MPQ_DIR");
    if (!directory || !std::filesystem::exists(std::filesystem::path(directory) / "d2data.mpq")) {
        std::puts("SKIP: set D2_MPQ_DIR to a Diablo II installation");
        return D2D_TEST_SKIPPED;
    }
    const char* patch = std::getenv("D2_PATCH_INSTALLER");
    auto data = load_game_data(directory, patch ? std::filesystem::path(patch) : std::filesystem::path{}, 0x1234);
    assert(data);
    if (!data->patched) {
        std::puts("SKIP: requires 1.14d data");
        return D2D_TEST_SKIPPED;
    }
    const auto howl = data->skills.by_name.find("Howl");
    assert(howl != data->skills.by_name.end());
    d2d::rules::Rng rng(7);
    auto made = new_character(*data, 4, "Headless", false, true, rng);
    Character character;
    character.character_class = 4;
    character.name = "Headless";
    character.header = made.header;
    character.stats = made.stats;
    character.items = made.items;
    character.panel = panel_stats(*data, character.header, character.items, character.stats);
    World world(&*data, -1, -1);
    world.enter(character);
    world.level = data->level(d2d::rules::level_ids::kBloodMoor);
    assert(world.level);
    world.fight.skill_level = [](int) { return 1; };
    assert(world.fight.missile_skill(*data->skills.get(howl->second)));
    assert(world.actor->pmode < 0);
    namespace touch = d2d::client::touch;
    touch::Input input;
    const auto box = touch::layout_boxes().skills.front();
    const float finger_x = box.x + box.width / 2, finger_y = box.y + box.height / 2;
    const float caster_x = world.actor->player.x, caster_y = world.actor->player.y;
    input.event(1, touch::Phase::Down, finger_x, finger_y, touch::caster_target(howl->second, caster_x, caster_y), true);
    assert(input.casts.empty());
    input.event(1, touch::Phase::Up, finger_x, finger_y);
    assert(input.casts.size() == 1);
    const auto& cast = input.casts.front();
    world.apply(cmd::UseSkill{ cast.skill, cast.x, cast.y, cast.unit, cast.left }, 1000);
    assert(world.actor->pmode >= 0 && world.actor->swing_skill == howl->second);
    assert(world.actor->player.x == caster_x && world.actor->player.y == caster_y);
    assert(!world.actor->player.walking && world.actor->attack_mon < 0);
}
