// SPDX-License-Identifier: GPL-3.0-or-later
// test-links: game
// test-includes: apps/d2d
// Touch taps and holds use the same explicit position casts as mouse stand-still.
#include "touch_gestures.hpp"
#include "touch_input.hpp"

#include <d2s.hpp>
#include <ds1.hpp>
#include <fight.hpp>
#include <gamedata.hpp>
#include <level_ids.hpp>
#include <protocol.hpp>
#include <skills.hpp>
#include <world.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>

namespace touch = d2d::client::touch;
using namespace d2d::game;
using d2d::rules::ServerDoFunction;
using d2d::rules::ServerStartFunction;

namespace {

constexpr int kAttack = 0, kMelee = 1, kMissile = 2, kHowl = 3, kCharge = 4, kLeapAttack = 5;

void cast(World& world, const touch::Target& target, std::uint32_t now) {
    assert(target.valid);
    world.apply(cmd::UseSkill{ target.skill, target.x, target.y, target.unit, target.left, -1, target.unit < 0 }, now);
}

} // namespace

int main() {
    GameData data;
    data.skills.rows = {
        { .id = kAttack, .name = "Attack", .srvdofunc = ServerDoFunction::kAttack, .anim = "A1" },
        { .id = kMelee, .name = "Test strike", .srvstfunc = ServerStartFunction::kBuildHit,
          .srvdofunc = ServerDoFunction::kResolveHit, .anim = "A1", .mana = 2 },
        { .id = kMissile, .name = "Test missile", .anim = "SC", .srvmissile = "test missile" },
        { .id = kHowl, .name = "Howl", .srvdofunc = ServerDoFunction::kNova, .anim = "SC", .srvmissilea = "test howl" },
        { .id = kCharge, .name = "Charge", .srvstfunc = ServerStartFunction::kCharge,
          .srvdofunc = ServerDoFunction::kCharge, .anim = "SQ", .par = { 150 }, .seqnum = 4 },
        { .id = kLeapAttack, .name = "Leap Attack", .srvstfunc = ServerStartFunction::kLeapAttack,
          .srvdofunc = ServerDoFunction::kLeapAttack, .anim = "SQ", .seqnum = 14 },
    };
    data.skills.rows[kMelee].delay = d2d::rules::compile_calc("8", data.skills.names);
    data.missiles["test missile"] = { .name = "test missile", .vel = 10, .range = 12 };
    data.missiles["test howl"] = { .name = "test howl", .vel = 10, .range = 12 };
    data.run_velocity[0] = 9;
    for (int mode : { kModeA1, kModeSC }) {
        const std::array<std::uint8_t, 18> key{ 0, std::uint8_t(mode) };
        data.composite_timings.emplace(key, GameData::AnimTiming{ .speed = 256, .frames = 4, .action = 2 });
    }
    auto world = World(&data, 5, 5);
    Level field;
    field.id = d2d::rules::level_ids::kBloodMoor;
    field.ds1 = d2d::ds1::Map(16, 16, 1, 1);
    field.walk.assign(80 * 80, 0);
    world.level = &field;
    world.fight.skill_level = [](int) { return 1; };
    auto& actor = *world.actor;
    const float x = actor.player.x, y = actor.player.y;
    const auto box = touch::layout_boxes().attack;
    const float button_x = box.x + box.width / 2, button_y = box.y + box.height / 2;
    touch::Input input;
    const touch::Target attack{ kAttack, -1, x + 3.f, y, true, true };

    // An ordinary click-to-move still walks. A location attack takes over in place.
    world.apply(cmd::Move{ x + 4.f, y, true }, 1000);
    assert(actor.player.walking && actor.pmode < 0);
    input.event(1, touch::Phase::Down, button_x, button_y, attack);
    input.event(1, touch::Phase::Up, button_x, button_y);
    assert(input.casts.size() == 1);
    cast(world, input.casts.front(), 1000);
    assert(actor.pmode == kModeA1 && actor.player.dir == direction16(3.f, 0.f));
    assert(!actor.player.walking && actor.player.x == x && actor.player.y == y);
    world.fight.player_modes(false, actor.pmode_until, 0.f);
    assert(actor.pstruck && actor.pmode < 0);
    input.frame();

    // A held attack loses a previously queued enemy and swings along the drag.
    Monster enemy;
    enemy.id = 17;
    enemy.hit_points = 1;
    enemy.unit.x = x + 2.f;
    enemy.unit.y = y;
    world.fight.monsters.push_back(enemy);
    cast(world, { kAttack, enemy.id, enemy.unit.x, enemy.unit.y, true, true }, 2000);
    assert(actor.attack_mon == 0 && actor.player.walking);
    world.fight.monsters.front().hit_points = 0;
    auto preview = attack;
    touch::SkillHold hold;
    hold.begin(1, 2000, enemy.id, 3.f, 0.f);
    hold.steer(0.f, -2.f);
    hold.resolve(preview, x, y, {}, {});
    assert(hold.dispatch(2300, true, kTickMs));
    cast(world, preview, 2300);
    assert(actor.pmode == kModeA1 && actor.attack_mon == -1 && !actor.player.walking);
    assert(actor.player.dir == direction16(0.f, -2.f));
    const auto next_swing = actor.pmode_until;
    world.fight.player_modes(true, next_swing, 0.f);
    assert(hold.dispatch(next_swing, true, kTickMs));
    cast(world, preview, next_swing);
    assert(actor.player.mode_ms == next_swing && actor.pmode == kModeA1 && actor.player.x == x && actor.player.y == y);
    world.fight.player_modes(false, actor.pmode_until, 0.f);

    // Empty melee abilities still ask for mana and respect their action-frame cooldown.
    const touch::Target melee{ kMelee, -1, x, y + 2.f, true, true };
    auto& mana = actor.character.stats.values[d2d::d2s::kMana];
    cast(world, melee, 3000);
    assert(actor.pmode < 0);
    mana = 6 * 256;
    cast(world, melee, 3000);
    assert(actor.swing_skill == kMelee && actor.pmode == kModeA1 && mana == 4 * 256);
    const auto end_melee = actor.pmode_until;
    world.fight.player_modes(false, end_melee, 0.f);
    assert(actor.pstruck && actor.skill_delay > end_melee);
    cast(world, melee, end_melee + kTickMs);
    assert(actor.pmode < 0 && mana == 4 * 256);
    cast(world, melee, actor.skill_delay);
    assert(actor.pmode == kModeA1 && mana == 2 * 256);
    world.fight.player_modes(false, actor.pmode_until, 0.f);

    // Directional missiles and centered Howl both discard a stale unit lock.
    actor.attack_mon = 0;
    cast(world, { kMissile, -1, x + 3.f, y, false, true }, 5000);
    assert(actor.pmode == kModeSC && actor.attack_mon == -1);
    world.fight.player_modes(false, actor.pmode_until, 0.f);
    assert(world.fight.pending.size() == 1);
    assert(world.fight.pending.front().velocity_x > 0 && world.fight.pending.front().velocity_y == 0);
    const int facing = actor.player.dir;
    cast(world, touch::caster_target(kHowl, x, y), 6000);
    assert(actor.pmode == kModeSC && actor.attack_mon == -1 && actor.player.dir == facing);
    world.fight.player_modes(false, actor.pmode_until, 0.f);
    assert(world.fight.pending.size() == 65 && actor.player.x == x && actor.player.y == y);

    // The movement attacks follow their own sequences to the aimed point.
    const auto finish_movement = [&](std::uint32_t start) {
        for (std::uint32_t now = start; actor.pmode >= 0 && now < start + 2000; now += kTickMs)
            world.fight.player_modes(false, now, float(kTickMs) / 1000.f);
        assert(actor.pmode < 0 && !actor.smove.active && !actor.player.walking);
    };
    cast(world, { kCharge, -1, x + 3.f, y, true, true }, 7000);
    assert(actor.swing_skill == kCharge && actor.smove.active);
    finish_movement(7000);
    assert(std::abs(actor.player.x - (x + 3.f)) < .05f && actor.player.y == y);
    cast(world, { kLeapAttack, -1, x + 3.f, y - 2.f, true, true }, 9000);
    assert(actor.swing_skill == kLeapAttack && actor.pmode >= 0);
    finish_movement(9000);
    assert(std::abs(actor.player.x - (x + 3.f)) < .05f && std::abs(actor.player.y - (y - 2.f)) < .05f);

    // Neither movement skill enters a blocked destination.
    const float before_x = actor.player.x, before_y = actor.player.y;
    cast(world, { kLeapAttack, -1, -1.f, before_y, true, true }, 11000);
    finish_movement(11000);
    assert(actor.player.x == before_x && actor.player.y == before_y);
    cast(world, { kCharge, -1, 20.f, before_y, true, true }, 13000);
    finish_movement(13000);
    assert(actor.player.x > before_x && !field.unit_blocked(actor.player.x, actor.player.y));

    // The targetless local display of a host swing does not turn into a location leap.
    const float display_x = actor.player.x;
    actor.move_x = before_x;
    world.display_swing(kLeapAttack, before_x, before_y, 15000);
    finish_movement(15000);
    assert(actor.player.x == display_x && actor.player.y == before_y);
}
