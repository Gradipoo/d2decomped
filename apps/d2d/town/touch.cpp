// SPDX-License-Identifier: GPL-3.0-or-later
// Opt-in touch prototype: separate per-character destinations, shared skill
// eligibility, targeting and gameplay commands. No platform code in the World.
#include "town.hpp"

#include "common.hpp"
#include "cursor.hpp"
#include "gamepad_map.hpp"
#include "hold_confirm.hpp"
#include "item_use.hpp"
#include "pad_hold.hpp"
#include "panels.hpp"
#include "skillpad.hpp"
#include "skilltree.hpp"
#include "store.hpp"
#include "touch_glyph.hpp"
#include "touch_icon.hpp"
#include "touch_input.hpp"
#include "touch_orb.hpp"
#include "touch_potions.hpp"
#include "use_item.hpp"

#include <inventory.hpp>

#include <d2s_items.hpp>
#include <dc6.hpp>
#include <fight.hpp>
#include <gamedata.hpp>
#include <hud_glide.hpp>
#include <hud_globe.hpp>
#include <level_ids.hpp>
#include <log.hpp>
#include <monsters.hpp>
#include <rules.hpp>
#include <skills.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numbers>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace d2d::client {

namespace {

template <typename Items>
touch::PotionSummary potion_summary(const Items& items) {
    std::vector<touch::PotionEntry> entries;
    entries.reserve(items.size());
    for (const auto& item : items)
        entries.push_back({ item.code, item.location == d2d::d2s::item_location::kBelt ? touch::PotionPlace::Belt
                                                                                        : touch::PotionPlace::Other,
                            item.column });
    return touch::summarize_touch_potions(entries);
}

/** True when the contextual interact chrome should accept touches and block field taps. */
bool touch_interact_visible(const Town::PadPick& pick) {
    return pick.kind == Town::PadPick::kItem || pick.kind == Town::PadPick::kThing;
}

bool centered_skill(const d2d::rules::Skill& skill) {
    using d2d::rules::ServerDoFunction;
    // Howl uses the same radial missile dispatch as Nova (FUN_005c9b50).
    return self_cast(skill) || skill.srvdofunc == ServerDoFunction::kNova || skill.srvdofunc == ServerDoFunction::kWarCry
        || skill.srvdofunc == ServerDoFunction::kStaticField || skill.srvdofunc == ServerDoFunction::kStateAroundCaster;
}

bool repeatable_skill(const d2d::rules::Skill& skill, const Fight& fight) {
    using d2d::rules::ServerDoFunction;
    if (skill.aura || skill.passive || skill.target_corpse || skill.target_ally || skill.target_pet
        || !skill.summon.empty() || self_cast(skill) || fight.summon_skill(skill)) return false;
    switch (skill.srvdofunc) {
        case ServerDoFunction::kBladeSentinel:
        case ServerDoFunction::kGrimWard:
        case ServerDoFunction::kWakeOfDestruction:
            return false;
        case ServerDoFunction::kNova:
        case ServerDoFunction::kWarCry:
            return (skill.maxdam > 0 || skill.emax > 0) && fight.missile_skill(skill);
        case ServerDoFunction::kAttack:
        case ServerDoFunction::kDoubleThrow:
        case ServerDoFunction::kMindBlast:
        case ServerDoFunction::kPsychicHammer:
        case ServerDoFunction::kStaticField:
        case ServerDoFunction::kThrow:
            return true;
        default:
            return skill_built(skill) || fight.missile_skill(skill);
    }
}

pad::WorldStep touch_skill_offset(float dx, float dy) {
    // Limit the two-to-one drag to the visible viewport before projecting.
    const float screen_x = std::clamp(dx * 2.f, -touch::kSkillAimX, touch::kSkillAimX);
    const float screen_y = std::clamp(dy * 2.f, -touch::kSkillAimUp, touch::kSkillAimDown);
    return { screen_x / kIsoW + screen_y / kIsoH, screen_y / kIsoH - screen_x / kIsoW };
}

pad::WorldStep touch_facing(const UnitState& unit) {
    const auto sector = std::ranges::find(game::kDir16FromSector, unit.dir) - std::begin(game::kDir16FromSector);
    const float angle = float(sector) * std::numbers::pi_v<float> / 8.f;
    return pad::world_step(-std::sin(angle), std::cos(angle), kIsoW, kIsoH, 1.f);
}

pad::Aim touch_skill_aim(const d2d::rules::Skill& skill, const Fight& fight, bool steering) {
    if (!fight.missile_skill(skill) && !Fight::spot_skill(skill)) return {};
    return { pad::kPadRangedAcquireReach, pad::kPadRangedStickyReach, steering ? pad::Aim{}.cone_cos : -2.f };
}

std::vector<pad::Candidate> touch_skill_foes(const View& view, const GameData& scene, const Level* level) {
    std::vector<pad::Candidate> foes;
    for (const auto& monster : view.monsters)
        if (monster.targetable() && monster.align != 2 && !monster.ally_not_interact(scene, level ? level->id : 0))
            foes.push_back({ monster.id, monster.unit.x, monster.unit.y });
    return foes;
}

} // namespace

auto Town::touch_hides_classic_hud() const -> bool {
    return touch::touch_hud_visible(touch_enabled, last_device, touch::g_touch_classic_hud);
}

auto Town::touch_shows_field_controls() const -> bool {
    return touch::touch_interface_visible(touch_enabled, last_device) && pad_field();
}

auto Town::touch_compact_field() const -> bool {
    return touch_shows_field_controls();
}

auto Town::touch_store_panel_x() const -> int {
    return kCharPanelX;
}

auto Town::touch_bag_item() const -> const d2d::d2s::Item* {
    const auto found = std::ranges::find(character.items, touch_item, &d2d::d2s::Item::id);
    if (found == character.items.end() || found->location != d2d::d2s::item_location::kStored
        || found->panel != d2d::d2s::item_panel::kInventory) return nullptr;
    return &*found;
}

bool Town::touch_can_sell(const d2d::d2s::Item* item) const {
    return scene && item && inv_open && !held && store.npc >= 0 && !store.gamble && store.mode != 1 && store.mode != 3
        && identify_with < 0 && !use_item.open() && d2d::rules::sellable(scene->rules, *item);
}

touch::Box Town::touch_trade_drop() const {
    if (!touch_item_drag || touch_item_finger == -1 || touch_trade_pending || !inv_open || held) return {};
    Scene::InvLayout grid;
    if (touch_stock.index(store) >= 0) grid = touch_inv_layout();
    else if (touch_sell_vendor == store.npc && touch_can_sell(touch_bag_item())) grid = store_grid(touch_store_panel_x());
    else return {};
    return { float(grid.grid_x), float(grid.grid_y), float(grid.cols * grid.box_w), float(grid.rows * grid.box_h) };
}

void Town::close_touch_item() {
    touch_item_finger = -1;
    touch_item = touch_sell_vendor = -1;
    touch_stock = {};
    touch_item_drag = touch_sold = false;
    touch_hold.clear();
    touch_pointer = {};
    touch_item_hint.clear();
}

void Town::cancel_touch_item() {
    if (touch_item_finger != -1) touch_cancelled_item_finger = touch_item_finger;
    else if (touch_sheet.trade_finger != -1) touch_cancelled_item_finger = touch_sheet.trade_finger;
    touch_sheet = {};
    close_touch_item();
}

bool Town::touch_trade(touch::ItemAction action) {
    if (touch_trade_pending || held) return false;
    if (action == touch::ItemAction::Buy) {
        const int index = touch_stock.index(store);
        if (index < 0) return false;
        send(cmd::Buy{ index, touch_stock.tab });
    } else if (action == touch::ItemAction::Sell) {
        if (touch_sell_vendor != store.npc || !touch_can_sell(touch_bag_item())) return false;
        send(cmd::Sell{ touch_item });
    } else return false;
    touch_trade_pending = true;
    return true;
}

void Town::touch_rebuild_bind_wheel() {
    touch_bind_ids = assignable_skills(character.name);
    touch_bind_ids.insert(touch_bind_ids.end(), skillpad::kActions.begin(), skillpad::kActions.end());
    touch_bind_wheel_page %= skillpad::bind_wheel_page_count(touch_bind_skill_count());
    touch_bind_wheel_at = -1;
    touch_bind_finger = -1;
}

int Town::touch_bind_skill_count() const {
    return int(touch_bind_ids.size()) - int(skillpad::kActions.size());
}

void Town::touch_open_bind() {
    const bool page = touch_input.alternate;
    touch_bind = TouchBindPhase::PickSlot;
    touch_bind_slot = -1;
    touch_bind_wheel_page = 0;
    touch_input.clear();
    touch_input.alternate = page;
    touch_feedback = "Tap a skill button";
    touch_rebuild_bind_wheel();
}

void Town::touch_close_bind() {
    touch_bind = TouchBindPhase::Off;
    touch_bind_slot = -1;
    touch_bind_finger = -1;
    touch_feedback.clear();
}

bool Town::touch_apply_bind(int slot, int skill) {
    if (slot < 0 || slot > touch::kAttackBind || skill == skillpad::kUnset) return false;
    const auto key = sheet_key(character.name);
    const auto found = touch_sheets.find(key);
    skillpad::Sheet sheet = found == touch_sheets.end() ? skillpad::Sheet{} : found->second;
    if (slot == touch::kAttackBind) {
        if (touch_input.alternate) sheet.alt_main = skill;
        else sheet.main = skill;
    } else sheet.set_slot(touch_input.alternate, slot, skill);
    const auto button = std::size_t(slot);
    if (!save_touch_sheet(character.name, sheet)) {
        touch_feedback = "Could not save assignment";
        touch_pulses.pulse(button, touch::FeedbackKind::Rejected, now_ms);
        return false;
    }
    touch_sheets.insert_or_assign(key, std::move(sheet));
    touch_pulses.pulse(button, touch::FeedbackKind::Accepted, now_ms);
    touch_play_ui_click(touch::UiClick::Assignment, slot + (touch_input.alternate ? touch::kAttackBind + 1 : 0));
    return true;
}

void Town::touch_play_ui_click(touch::UiClick action, int destination) {
    if (last_device == touch::LastDevice::Touch && g_on_button_press
        && touch_clicks.accept(action, destination, now_ms))
        g_on_button_press();
}

int Town::touch_bound_skill(int slot) const {
    const auto found = touch_sheets.find(sheet_key(character.name));
    if (slot == touch::kAttackBind) {
        if (found == touch_sheets.end()) return skill_known(character.name, 0) ? 0 : std::max(0, int(character.header.left_skill));
        const int skill = found->second.trigger(touch_input.alternate);
        if (skill == skillpad::kUnset)
            return skill_known(character.name, 0) ? 0 : std::max(0, int(character.header.left_skill));
        return skill;
    }
    if (slot < 0 || slot >= touch::kSkillButtons || found == touch_sheets.end()) return skillpad::kEmpty;
    return found->second.slot(touch_input.alternate, slot);
}

Scene::InvLayout Town::touch_inv_layout() const {
    const int cls = std::max(character.character_class, 0);
    if (!scene) return {};
    return scene->inv_layout[std::size_t(cls)];
}

/**
 * Load the named character touch assignments independently of controller positions. Remove the
 * cached sheet if its sidecar cannot be opened.
 */
void Town::load_touch_sheet(const std::string& name) {
    if (key_dir.empty() || name.empty()) return;
    std::ifstream file(key_dir / (name + ".touchskills"));
    if (file) touch_sheets[sheet_key(name)] = skillpad::read(std::string{ std::istreambuf_iterator<char>(file), {} });
    else touch_sheets.erase(sheet_key(name));
}

/**
 * Persist a cached touch sheet through a checked temporary file and rename. Leave the original
 * destination in place on a failed write and report the failure.
 */
bool Town::save_touch_sheet(const std::string& name, const skillpad::Sheet& sheet) const {
    if (key_dir.empty() || name.empty()) return true;
    const auto path = key_dir / (name + ".touchskills");
    auto temporary = path; temporary += ".tmp";
    std::ofstream file(temporary);
    if (!file) { d2d::log::warn("Touch assignments could not be written"); return false; }
    file << skillpad::write(sheet);
    file.close();
    std::error_code error;
    if (file) std::filesystem::rename(temporary, path, error);
    if (!file || error) {
        d2d::log::warn("Touch assignments could not be written");
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

/**
 * Resolve a controller/touch skill preview for the selected owner without issuing a command. Manual
 * aim uses screen drag distance; self casts, eligible corpses and Teleport each apply their own
 * target policy.
 */
touch::Target Town::resolve_skill(const PadPlayer& player_pad, int skill, float dx, float dy, bool manual, const UnitState* owner, std::string_view owner_name, int charged_item, int sticky) const {
    touch::Target target;
    target.skill = skill;
    if (skillpad::binds_action(skill)) { target.valid = true; return target; }
    if (!scene) return target;
    const Character* charge_owner = owner_name.empty() || owner_name == character.name ? &character : nullptr;
    if (!charge_owner) for (const auto& candidate : world.players)
        if (candidate && candidate->character.name == owner_name) { charge_owner = &candidate->character; break; }
    const auto entries = charged_item >= 0 && charge_owner ? d2d::game::charged_skills(charge_owner->items) : std::vector<d2d::game::ChargedSkill>{};
    const bool charged = std::ranges::any_of(entries, [&](const auto& entry) { return entry.item == charged_item && entry.skill == skill; });
    if (!charged && !skill_known(owner_name.empty() ? std::string_view(character.name) : owner_name, skill)) return target;
    const auto* info = scene->skills.get(skill);
    if (!info || info->passive) return target;
    if (skillpad::portal_name(info->name)) {
        target.valid = portal_item(owner_name.empty() ? std::string_view(character.name) : owner_name) >= 0 && level && !d2d::rules::level_ids::is_town(level->id);
        return target;
    }
    const auto& unit = owner ? *owner : view.player;
    target.valid = true;
    target.x = unit.x; target.y = unit.y;
    target.left = skillpad::swings({ info->leftskill, !info->srvmissile.empty(), info->range });
    if (info->aura || centered_skill(*info)) return touch::caster_target(skill, unit.x, unit.y);
    const bool teleport = info->srvdofunc == d2d::rules::ServerDoFunction::kTeleport;
    const auto facing = owner || player_pad.move.active
        ? pad::WorldStep{ player_pad.face_x, player_pad.face_y } : touch_facing(unit);
    const float reach = teleport ? 6.f : 3.f;
    const auto offset = manual && (teleport || dx != 0 || dy != 0) ? touch_skill_offset(dx, dy)
        : pad::WorldStep{ facing.x * reach, facing.y * reach };
    target.x += offset.x;
    target.y += offset.y;
    if (info->target_corpse || info->srvdofunc == d2d::rules::ServerDoFunction::kCorpseExplosion) {
        const float wanted_x = manual ? target.x : unit.x, wanted_y = manual ? target.y : unit.y;
        float distance = manual ? 4.f : 12.f;
        target.valid = false;
        for (const auto& corpse : view.monsters)
            if (corpse.usable_corpse()) {
                const float apart = std::hypot(corpse.unit.x - wanted_x, corpse.unit.y - wanted_y);
                if (apart < distance) {
                    distance = apart; target.valid = true;
                    target.x = corpse.unit.x; target.y = corpse.unit.y; target.unit = corpse.id;
                }
            }
        return target;
    }
    if (teleport && level) {
        // Preview the same room constraint and nearest free subtile the
        // existing do-27 path uses (FUN_005ca360 / FUN_0064e7b0).
        if (d2d::rules::level_ids::is_town(level->id) || level->teleport == 0) target.valid = false;
        const int spot_x = int(std::floor(target.x * 5)), spot_y = int(std::floor(target.y * 5));
        const auto rooms = missile_rooms(*scene, *level, unit.x, unit.y);
        if (!std::ranges::any_of(rooms, [&](const auto& room) {
            const int sub_x = spot_x + level->world_x * 5, sub_y = spot_y + level->world_y * 5;
            return sub_x >= room[0] && sub_y >= room[1] && sub_x < room[0] + room[2] && sub_y < room[1] + room[3];
        })) target.valid = false;
        const auto spot = level->warp_spot(spot_x, spot_y);
        if (!spot) target.valid = false;
        else if (*spot != std::pair{ spot_x, spot_y }) {
            target.x = (float(spot->first) + .5f) / 5; target.y = (float(spot->second) + .5f) / 5;
        }
        if (level->teleport == 2) {
            const auto wall = [&](int at_x, int at_y) { return level->blocked((float(at_x) + .5f) / 5, (float(at_y) + .5f) / 5, 0x804); };
            if (d2d::rules::sight_blocked(int(std::floor(unit.x * 5)), int(std::floor(unit.y * 5)), 2, spot_x, spot_y, 2, wall)) target.valid = false;
        }
    }
    if (!teleport) {
        const auto foes = touch_skill_foes(view, *scene, level);
        const auto aim = touch_skill_aim(*info, world.fight, manual || player_pad.move.active);
        if (const int picked = pad::pick_target(unit.x, unit.y, offset.x, offset.y, foes, sticky, aim); picked >= 0) {
            const auto& foe = foes[std::size_t(picked)];
            target.x = foe.x; target.y = foe.y; target.unit = foe.id;
        }
    }
    return target;
}

/**
 * Dispatch a committed skill preview after rechecking availability and player state. Panel
 * shortcuts open UI, portal bindings use a carried scroll or tome, and auras select the active hand.
 */
void Town::cast_resolved(PadPlayer& player_pad, const touch::Target& target) {
    if (!target.valid || view.dead) return;
    if (const int panel_index = skillpad::panel_index(target.skill); panel_index >= 0) {
        using touch::UiClick;
        const std::array panels{
            std::pair{ &char_open, UiClick::Stats }, std::pair{ &inv_open, UiClick::Inventory },
            std::pair{ &tree_open, UiClick::Skills }, std::pair{ &automap.open, UiClick::Map },
            std::pair{ &quest_log.open, UiClick::Quest }
        };
        const auto [open_flag, click] = panels[std::size_t(panel_index)];
        const bool was_open = *open_flag;
        skill_strip = panel_index;
        pad::Actions open; open.confirm = true; drive_strip(open);
        if (was_open != *open_flag) touch_play_ui_click(click, *open_flag);
        touch_input.clear();
        return;
    }
    if (target.skill == skillpad::kInteract) {
        const auto pick = pad_pick(player_pad, view.player, true, {}, false);
        if (pick.kind == PadPick::kItem) {
            if (!open_loot_chooser(player_pad, view.player, true)) send(cmd::Pickup{ pick.id });
        } else if (pick.kind == PadPick::kThing) send(cmd::Interact{ pick.id });
        return;
    }
    if (!scene || !skill_known(character.name, target.skill)) return;
    const auto* info = scene->skills.get(target.skill);
    if (!info) return;
    if (skillpad::portal_name(info->name)) {
        if (const int item = portal_item(character.name); item >= 0) send(cmd::UseItem{ item });
        return;
    }
    if (info->aura) {
        const auto* current = scene->skills.get(skillbar.right);
        const auto choice = skillpad::aura_choice(view.aura, target.skill, skillbar.right, current && current->aura, player_pad.aura_return);
        player_pad.aura_return = choice.remembered;
        skillbar.choose(false, { choice.next, -1 });
        if (net_game) net_game->hand(choice.next, false);
        send(cmd::SelectSkill{ choice.next, false });
    } else {
        const auto cast = centered_skill(*info)
            ? touch::caster_target(target.skill, view.player.x, view.player.y) : target;
        send(cmd::UseSkill{ cast.skill, cast.x, cast.y, cast.unit, cast.left, -1, cast.unit < 0 });
    }
}

/**
 * Route a logical touch event to assignment, inventory/panel interaction or field gestures. Return
 * true for consumed events; field gestures retain separate movement and skill ownership.
 */
bool Town::touch_event(std::int64_t finger, touch::Phase phase, float x, float y) {
    if (!touch_enabled) return false;
    if (loot_chooser.open) {
        if (!loot_chooser.touch) { loot_chooser.clear(); return true; }
        const auto box = LootChooser::list_box();
        const touch::Box close{ box.x + box.width - 70.f, box.y - 48.f, 70.f, 44.f };
        if (phase == touch::Phase::Down && close.contains(x, y)) { loot_chooser.clear(); return true; }
        if (const int picked = loot_chooser.touch_event(finger, phase, x, y); picked >= 0) commit_loot(picked);
        return true;
    }
    if (phase == touch::Phase::Down) ++touch_fingers;
    else if (phase == touch::Phase::Up || phase == touch::Phase::Cancel)
        touch_fingers = std::max(0, touch_fingers - 1);
    // Paths that return before event() still drop this finger. event() sees the Up first,
    // so a skill release can cast before its owner is cleared.
    struct ReleaseUnlessForwarded {
        touch::Input& input;
        std::int64_t finger;
        touch::Phase phase;
        bool armed = true;
        ~ReleaseUnlessForwarded() {
            if (armed && (phase == touch::Phase::Up || phase == touch::Phase::Cancel)) input.release_finger(finger);
        }
    } release{ touch_input, finger, phase };
    if (touch_cancelled_item_finger == finger) {
        if (phase != touch::Phase::Move) touch_cancelled_item_finger = -1;
        if (phase != touch::Phase::Down) return true;
    }
    const touch::Layout chrome = touch::layout_boxes();
    using touch::UiClick;
    const auto toggle_touch_map = [&] {
        touch_input.clear();
        automap.open = !automap.open;
        if (automap.open) { automap.mini = 0; automap.fade = 2; automap.panel_side = 0; automap.covered = false; }
        touch_play_ui_click(UiClick::Map, automap.open);
    };
    // Menu controls act on touch-down; keep their highlight until that finger lifts.
    if (phase == touch::Phase::Down && !touch_effect.modal) {
        const std::array<touch::Box, 8> menu_boxes{ chrome.menu, chrome.skills_bar, chrome.stats,
            chrome.quest, chrome.bind, chrome.done, chrome.map, chrome.page };
        for (std::size_t index = 0; index < menu_boxes.size(); ++index) {
            const std::size_t channel = index + 7;
            if (touch_assign != skillpad::kUnset && index > 0 && index < 5) continue;
            if (store.npc >= 0 && index < 5) continue;
            if (menu_boxes[index].contains(x, y) && touch_pulse_fingers[channel] == -1) {
                touch_pulse_fingers[channel] = finger;
                touch_pulses.press(channel, now_ms);
                break;
            }
        }
    } else if (phase == touch::Phase::Up || phase == touch::Phase::Cancel) {
        for (std::size_t channel = 7; channel < 15; ++channel) {
            if (touch_pulse_fingers[channel] != finger) continue;
            if (touch_pulses.sample(channel, now_ms).kind == touch::FeedbackKind::Pressed)
                touch_pulses.release(channel);
            touch_pulse_fingers[channel] = -1;
        }
    }
    if (touch_effect.modal) {
        const auto* effect = touch_effect.current(view.effects);
        if (!effect || !effect->dismissible) { touch_effect.modal = false; return true; }
        if (phase == touch::Phase::Down && touch::effect_cancel().contains(x, y)) touch_effect.cancel();
        else if (phase == touch::Phase::Down && touch::effect_confirm().contains(x, y)) {
            if (const auto* confirmed = touch_effect.confirm(view.effects))
                send(cmd::DismissEffect{ confirmed->kind, confirmed->instance });
        }
        return true;
    }
    if (touch_effect.finger == finger) {
        if (phase == touch::Phase::Cancel) touch_effect.clear_press();
        else if (phase == touch::Phase::Up) {
            bool inside = false;
            const auto capacity = touch::effect_capacity(chrome);
            const auto start = touch::effect_page_start(view.effects.size(), capacity, touch_effect.page);
            const auto count = std::min(view.effects.size() - start, touch::effects_per_page(view.effects.size(), capacity));
            for (std::size_t index = 0; index < count; ++index)
                if (view.effects[start + index].kind == touch_effect.kind && view.effects[start + index].instance == touch_effect.instance)
                    inside = touch::effect_box(chrome, index).contains(x, y);
            touch_effect.release(view.effects, now_ms, inside);
        }
        return true;
    }
    if (touch_shows_field_controls() && phase == touch::Phase::Down) {
        const auto capacity = touch::effect_capacity(chrome);
        const auto start = touch::effect_page_start(view.effects.size(), capacity, touch_effect.page);
        const auto count = std::min(view.effects.size() - start, touch::effects_per_page(view.effects.size(), capacity));
        if (view.effects.size() > capacity && capacity > 1 && touch::effect_box(chrome, capacity - 1).contains(x, y)) {
            ++touch_effect.page; touch_effect.cancel(); touch_effect.last_kind = -1; return true;
        }
        for (std::size_t index = 0; index < count; ++index) if (touch::effect_box(chrome, index).contains(x, y)) {
            touch_effect.finger = finger; touch_effect.kind = view.effects[start + index].kind;
            touch_effect.instance = view.effects[start + index].instance; touch_effect.down_ms = now_ms;
            return true;
        }
    }
    if (touch_shows_field_controls() && !view.dead && phase == touch::Phase::Down) {
        int button = touch::bind_slot_at(chrome, x, y);
        if (chrome.healing.contains(x, y)) button = 5;
        else if (chrome.mana.contains(x, y)) button = 6;
        if (button >= 0 && button < int(touch_pulse_fingers.size()) && touch_pulse_fingers[std::size_t(button)] == -1) {
            touch_pulse_fingers[std::size_t(button)] = finger;
            touch_pulses.press(std::size_t(button), now_ms);
        }
    } else if (phase == touch::Phase::Up || phase == touch::Phase::Cancel) {
        for (std::size_t button = 0; button < 7; ++button) if (touch_pulse_fingers[button] == finger) {
            // A binding may already have replaced the press with its save result.
            if (touch_pulses.sample(button, now_ms).kind != touch::FeedbackKind::Pressed) {
                touch_pulse_fingers[button] = -1;
                continue;
            }
            auto result = touch::FeedbackKind::Cancelled;
            if (phase == touch::Phase::Up) {
                bool accepted = true;
                if (button >= 5) {
                    const auto summary = potion_summary(character.items);
                    const bool inside = button == 5 ? chrome.healing.contains(x, y) : chrome.mana.contains(x, y);
                    if (inside) accepted = button == 5 ? summary.strongest_healing_slot.has_value() : summary.strongest_mana_slot.has_value();
                    else { touch_pulses.release(button); touch_pulses.pulse(button, result, now_ms); touch_pulse_fingers[button] = -1; continue; }
                } else if (button < 4 && touch_input.owns_skill(finger)) accepted = touch_input.preview.valid;
                result = accepted ? touch::FeedbackKind::Accepted : touch::FeedbackKind::Rejected;
            }
            touch_pulses.release(button);
            touch_pulses.pulse(button, result, now_ms); touch_pulse_fingers[button] = -1;
        }
    }
    if (pad_field()) touch_interact_pick = pad_pick(touch_player, view.player, true, {}, false);
    if (speech.npc >= 0 || npc_menu.npc >= 0) {
        if (touch_item_finger != -1 || touch_stock.tab >= 0 || touch_sell_vendor >= 0) cancel_touch_item();
        if (touch_cancelled_item_finger == finger && phase != touch::Phase::Down) return true;
        if (phase == touch::Phase::Down && touch::touch_interface_visible(touch_enabled, last_device) && chrome.map.contains(x, y)) {
            toggle_touch_map();
            return true;
        }
        if (phase == touch::Phase::Down && chrome.done.contains(x, y)) {
            touch_input.clear();
            touch_back_requested = true;
            return true;
        }
        return false;
    }
    if (store.npc >= 0 && !touch_sheet.open && !held && touch_bind == TouchBindPhase::Off
        && touch_assign == skillpad::kUnset && phase == touch::Phase::Down) {
        if (const int tab = store_tab_at(int(touch::to_legacy_x(x)), int(y), touch_store_panel_x()); tab >= 0) {
            if (store.tab != tab && touch_stock.tab >= 0) cancel_touch_item();
            store.tab = tab;
            return true;
        }
    }
    if (phase == touch::Phase::Down && store.npc < 0 && chrome.bind.contains(x, y)) {
        if (touch_bind != TouchBindPhase::Off) {
            touch_input.clear(); touch_ground_finger = -1;
            touch_close_bind();
        } else {
            touch_open_bind();
        }
        cancel_touch_item();
        touch_play_ui_click(UiClick::Bind, touch_bind != TouchBindPhase::Off);
        return true;
    }
    if (store.npc < 0 && touch_bind == TouchBindPhase::Off && phase == touch::Phase::Down
        && (chrome.quest.contains(x, y) || (chrome.quest_track.contains(x, y) && touch_hides_classic_hud() && pad_field() && !quest_log.open))) {
        touch_input.clear();
        cancel_touch_item();
        toggle_quest_log();
        touch_play_ui_click(UiClick::Quest, quest_log.open);
        return true;
    }
    if (store.npc < 0 && touch_bind == TouchBindPhase::Off && touch_assign == skillpad::kUnset && phase == touch::Phase::Down && chrome.menu.contains(x, y)) {
        touch_input.clear();
        cancel_touch_item();
        const bool was_open = inv_open;
        toggle_inventory();
        if (was_open != inv_open) touch_play_ui_click(UiClick::Inventory, inv_open);
        else touch_pulses.pulse(7, touch::FeedbackKind::Rejected, now_ms);
        return true;
    }
    if (store.npc < 0 && touch_bind == TouchBindPhase::Off && phase == touch::Phase::Down && chrome.skills_bar.contains(x, y)) {
        touch_input.clear();
        cancel_touch_item();
        const bool was_open = tree_open;
        toggle_skill_tree();
        if (was_open != tree_open) touch_play_ui_click(UiClick::Skills, tree_open);
        else touch_pulses.pulse(8, touch::FeedbackKind::Rejected, now_ms);
        return true;
    }
    if (store.npc < 0 && touch_bind == TouchBindPhase::Off && phase == touch::Phase::Down && chrome.stats.contains(x, y)) {
        touch_input.clear();
        cancel_touch_item();
        const bool was_open = char_open;
        toggle_character();
        if (was_open != char_open) touch_play_ui_click(UiClick::Stats, char_open);
        else touch_pulses.pulse(9, touch::FeedbackKind::Rejected, now_ms);
        return true;
    }
    if (phase == touch::Phase::Down && chrome.done.contains(x, y)) {
        touch_input.clear(); touch_ground_finger = -1;
        cancel_touch_item();
        if (touch_bind != TouchBindPhase::Off) {
            touch_close_bind();
            touch_play_ui_click(UiClick::Back);
        } else if (touch_assign != skillpad::kUnset) {
            touch_assign = skillpad::kUnset;
            tree_open = touch_assignment_from_tree;
            touch_assignment_from_tree = false;
            touch_play_ui_click(UiClick::Back);
        } else touch_back_requested = true;
        return true;
    }
    if (touch_bind != TouchBindPhase::Off && phase == touch::Phase::Down && chrome.page.contains(x, y)) {
        touch_input.alternate = !touch_input.alternate;
        touch_bind_finger = -1;
        touch_bind_wheel_at = -1;
        touch_play_ui_click(UiClick::BindSet, touch_input.alternate);
        return true;
    }
    if (touch_bind == TouchBindPhase::PickSkill) {
        const auto wheel = touch::bind_wheel_layout(chrome);
        const int pages = skillpad::bind_wheel_page_count(touch_bind_skill_count());
        const auto page = skillpad::bind_wheel_page(touch_bind_skill_count(), touch_bind_wheel_page);
        const float wheel_x = x - wheel.center_x, wheel_y = y - wheel.center_y;
        const auto pick_at = [&] {
            if (x < 0 || x >= touch::viewport_width || y < 0 || y >= kScreenHeight
                || wheel.previous.contains(x, y) || wheel.indicator.contains(x, y) || wheel.next.contains(x, y))
                return -1;
            return skillpad::bind_wheel_pick(wheel_x, wheel_y, page, touch_bind_from_hub);
        };
        if (phase == touch::Phase::Down) {
            if (pages > 1 && (wheel.previous.contains(x, y) || wheel.next.contains(x, y))) {
                const int step = wheel.previous.contains(x, y) ? -1 : 1;
                touch_bind_wheel_page = (touch_bind_wheel_page + step + pages) % pages;
                touch_bind_wheel_at = -1;
                touch_bind_finger = -1;
                touch_play_ui_click(UiClick::WheelPage, touch_bind_wheel_page);
                return true;
            }
            const int slot = touch::bind_slot_at(chrome, x, y);
            if (slot >= 0) {
                if (slot != touch_bind_slot) touch_play_ui_click(UiClick::Destination, slot);
                touch_bind_slot = slot;
                touch_rebuild_bind_wheel();
                touch_feedback = "Choose a skill or action";
                return true;
            }
            if (touch_bind_finger == -1) {
                touch_bind_from_hub = std::hypot(wheel_x, wheel_y) <= skillpad::kBindWheelHub;
                touch_bind_wheel_at = pick_at();
                if (touch_bind_from_hub || touch_bind_wheel_at >= 0) touch_bind_finger = finger;
            }
            return true;
        }
        if (finger == touch_bind_finger) {
            if (phase == touch::Phase::Move) touch_bind_wheel_at = pick_at();
            if (phase == touch::Phase::Up) {
                const int index = pick_at();
                if (index >= 0 && touch_apply_bind(touch_bind_slot, touch_bind_ids[std::size_t(index)])) {
                    touch_bind = TouchBindPhase::PickSlot;
                    touch_feedback = "Tap a skill button";
                }
            }
            if (phase == touch::Phase::Up || phase == touch::Phase::Cancel) {
                touch_bind_finger = -1;
                touch_bind_wheel_at = -1;
            }
        }
        return true;
    }
    if (touch_bind == TouchBindPhase::PickSlot) {
        if (phase == touch::Phase::Down) {
            const int slot = touch::bind_slot_at(chrome, x, y);
            if (slot >= 0) {
                touch_bind_slot = slot;
                touch_rebuild_bind_wheel();
                touch_bind = TouchBindPhase::PickSkill;
                touch_feedback = "Choose a skill or action";
                touch_play_ui_click(UiClick::Destination, slot);
                return true;
            }
        }
        return true;
    }
    if (touch_assign != skillpad::kUnset) {
        if (phase == touch::Phase::Down && chrome.menu.contains(x, y)) { touch_assign = skillpad::kInventory; return true; }
        const bool was_alternate = touch_input.alternate;
        touch_input.event(finger, phase, x, y, {}, false, true);
        if (was_alternate != touch_input.alternate) touch_play_ui_click(UiClick::BindSet, touch_input.alternate);
        const int destination = std::exchange(touch_input.destination, -1);
        if (destination >= 0 && touch_apply_bind(destination, touch_assign)) {
            const auto* info = scene ? scene->skills.get(touch_assign) : nullptr;
            const std::string name = info ? info->name : std::string(skillpad::action_label(touch_assign, true));
            touch_feedback = (destination == touch::kAttackBind ? std::string("Attack")
                               : std::to_string(destination + 1)) + " . " + name;
        }
        return true;
    }
    if (!pad_field()) {
        const float viewport_x = x;
        x = touch::to_legacy_x(x);
        touch_input.clear();
        const auto follow_pointer = [&] {
            touch_pointer = { true, false, false, x, y };
        };
        const auto place_held_at = [&](float point_x, float point_y) {
            if (!scene || !held) return;
            const auto [item_w, item_h] = d2d::rules::item_size(scene->rules, held->code);
            OpenPanels open{ inv_open, stash_open, cube_open, belt_open, character.expansion };
            auto click = item_cursor_command(*scene, character.items, held, std::max(character.character_class, 0),
                                             open, int(point_x), int(point_y));
            if (!click.consumed) {
                float snapped_x = point_x, snapped_y = point_y;
                const auto try_grid = [&](const Scene::InvLayout& layout) {
                    touch::SnapGrid grid{ float(layout.grid_x), float(layout.grid_y), float(layout.box_w), float(layout.box_h),
                                          layout.cols, layout.rows, item_w, item_h };
                    int col = 0, row = 0;
                    if (!touch::snap_grid_cell(grid, point_x, point_y, col, row)) return false;
                    const auto snapped = touch::snap_grid_point(grid, col, row);
                    snapped_x = snapped.first; snapped_y = snapped.second;
                    return true;
                };
                bool snapped = false;
                if (inv_open) snapped = try_grid(touch_inv_layout());
                if (!snapped && cube_open) snapped = try_grid(scene->cube_layout);
                if (!snapped && stash_open) snapped = try_grid(scene->stash_layout[character.expansion ? 1 : 0]);
                if (!snapped && inv_open) {
                    const auto& inv = touch_inv_layout();
                    float best = 48.f;
                    int best_slot = -1;
                    for (int slot = d2d::d2s::body_location::kFirst; slot <= d2d::d2s::body_location::kLast; ++slot) {
                        const auto& rect = inv.slots[std::size_t(slot)];
                        if (rect[2] <= 0) continue;
                        const float centre_x = float(rect[0]) + float(rect[2]) * .5f, centre_y = float(rect[1]) + float(rect[3]) * .5f;
                        const float dist = std::hypot(point_x - centre_x, point_y - centre_y);
                        if (dist < best) { best = dist; best_slot = slot; }
                    }
                    if (best_slot >= 0) {
                        const auto& rect = inv.slots[std::size_t(best_slot)];
                        snapped_x = float(rect[0]) + float(rect[2]) * .5f;
                        snapped_y = float(rect[1]) + float(rect[3]) * .5f;
                        snapped = true;
                    }
                }
                if (snapped)
                    click = item_cursor_command(*scene, character.items, held, std::max(character.character_class, 0),
                                                open, int(snapped_x), int(snapped_y));
            }
            if (click.cmd && !std::holds_alternative<cmd::ToCursor>(*click.cmd)) send(*click.cmd);
            else if (!click.consumed) {
                if (use_item.open() && use_item::in_slot(int(point_x), int(point_y))) {
                    const bool accepted = held->identified
                        && (use_item.hc_idx == use_item::kCharsi ? d2d::rules::imbuable(scene->rules, *held, character.expansion)
                            : use_item.hc_idx == use_item::kLarzuk ? d2d::rules::socketable(scene->rules, *held)
                            : use_item.hc_idx == use_item::kAnya && d2d::rules::nameable(scene->rules, *held));
                    const auto out = use_item::press(use_item, int(point_x), int(point_y), now_ms, false, false, true, accepted);
                    if (out.act == use_item::Act::kPlace) use_item.item = held->id;
                } else send(cmd::Drop{ held->id });
            }
        };
        const auto run_sheet_action = [&](touch::ItemAction action) {
            if (action == touch::ItemAction::Use && touch_item >= 0) {
                const auto found = std::ranges::find(character.items, touch_item, &d2d::d2s::Item::id);
                if (found == character.items.end()) return;
                const auto used = item_use(*found);
                if (used.kind == ItemUseKind::Identify) identify_with = found->id;
                else if (used.kind != ItemUseKind::None) send(cmd::UseItem{ found->id });
            }
        };
        if (touch_sheet.open) {
            auto trade = touch::ItemAction::None;
            if (touch_sheet.trade_event(finger, phase, x, y, trade)) {
                if (trade != touch::ItemAction::None) {
                    touch_trade(trade);
                    touch_sheet = {};
                    close_touch_item();
                }
                return true;
            }
            if (phase == touch::Phase::Down) {
                const auto hit = touch_sheet.hit(x, y);
                if (hit == touch::ItemAction::Cancel || hit == touch::ItemAction::None) {
                    if (hit == touch::ItemAction::Cancel || !touch_sheet.contains(x, y)) {
                        if (touch_sheet.identifying_target) touch_identify_target = -1;
                        else {
                            close_touch_item();
                            touch_cancelled_item_finger = finger;
                        }
                        touch_sheet = {};
                    }
                    return true;
                }
                if (hit == touch::ItemAction::Confirm) {
                    if (touch_sheet.identifying_target) {
                        if (identify_with >= 0 && touch_identify_target >= 0)
                            send(cmd::IdentifyWith{ identify_with, touch_identify_target });
                        identify_with = -1; touch_identify_target = -1;
                        touch_sheet = {};
                        return true;
                    }
                    if (touch_sheet.chosen != touch::ItemAction::None) {
                        run_sheet_action(touch_sheet.chosen);
                        touch_sheet = {};
                        close_touch_item();
                    }
                    return true;
                }
                if (hit == touch_sheet.chosen && touch_sheet.confirming) {
                    run_sheet_action(hit);
                    touch_sheet = {};
                    close_touch_item();
                    return true;
                }
                touch_sheet.chosen = hit;
                touch_sheet.confirming = true;
                touch_item_hint = "Tap Confirm";
                return true;
            }
            return phase != touch::Phase::Move;
        }
        if (touch_item_finger != -1 && finger != touch_item_finger) return true;
        if (touch_item_finger != -1 && finger == touch_item_finger
            && (phase == touch::Phase::Cancel || touch_sold || (held && (touch_stock.tab >= 0 || touch_sell_vendor >= 0)))) {
            if (phase == touch::Phase::Cancel || phase == touch::Phase::Up) close_touch_item();
            return true;
        }
        if (held) {
            if (phase == touch::Phase::Cancel) {
                close_touch_item();
                return true;
            }
            follow_pointer();
            if (phase == touch::Phase::Up) {
                place_held_at(x, y);
                close_touch_item();
            }
            return true;
        }
        if (touch_item_finger != -1 && finger == touch_item_finger && (phase == touch::Phase::Move || phase == touch::Phase::Up)) {
            const bool vendor_drag = touch_stock.tab >= 0 || touch_sell_vendor >= 0;
            if (!touch_item_drag && touch::item_dragged(touch_item_x, touch_item_y, x, y) && scene) {
                touch_item_drag = true;
                touch_hold.clear();
                if (!vendor_drag) {
                    const auto click = item_cursor_command(*scene, character.items, held, std::max(character.character_class, 0),
                                                           { inv_open, stash_open, cube_open, belt_open, character.expansion },
                                                           int(touch_item_x), int(touch_item_y));
                    if (click.cmd) send(*click.cmd);
                }
            }
            if (touch_item_drag) {
                follow_pointer();
                if (phase == touch::Phase::Up) {
                    if (vendor_drag) {
                        if (touch_trade_drop().contains(x, y))
                            touch_trade(touch_stock.tab >= 0 ? touch::ItemAction::Buy : touch::ItemAction::Sell);
                    } else place_held_at(x, y);
                    close_touch_item();
                }
                return true;
            }
            if (phase == touch::Phase::Move) return true;
            const auto* bag_item = touch_bag_item();
            const auto action = bag_item ? item_use(*bag_item) : ItemUse{};
            const bool can_use = bag_item && !action.label.empty() && action.kind != ItemUseKind::None;
            const bool can_sell = touch_sell_vendor == store.npc && touch_can_sell(bag_item);
            const bool can_buy = touch_stock.index(store) >= 0;
            touch_item_finger = -1;
            touch_hold.clear();
            touch_sheet = touch::make_item_sheet(can_use && !can_sell, can_sell, can_buy);
            if (!touch_sheet.open) {
                close_touch_item();
                return true;
            }
            touch_item_hint = (can_buy || can_sell) ? "" : "Tap an action, then Confirm";
            return true;
        }
        if (phase == touch::Phase::Down && scene && identify_with >= 0 && !held) {
            const auto* item = bag_at(int(x), int(y));
            if (item && item->id != identify_with && !item->identified) {
                touch_identify_target = item->id;
                touch_sheet = touch::make_confirm_sheet();
                touch_item_hint = "Confirm to identify";
            }
            return true;
        }
        if (phase == touch::Phase::Down && scene && identify_with < 0 && !held) {
            const int stock_index = store.npc >= 0 ? store_item_at(*scene, store, int(x), int(y), touch_store_panel_x()) : -1;
            const auto* item = stock_index < 0 ? bag_at(int(x), int(y)) : nullptr;
            const auto action = item ? item_use(*item) : ItemUse{};
            const bool can_sell = touch_can_sell(item);
            if (stock_index >= 0 || (item && (!action.label.empty() || can_sell))) {
                touch_stock = stock_index >= 0 ? StoreSelection::capture(store, stock_index) : StoreSelection{};
                touch_sell_vendor = can_sell ? store.npc : -1;
                touch_item_finger = finger; touch_item = item ? item->id : -1;
                touch_sold = false; touch_item_drag = false;
                touch_hold.clear();
                touch_item_x = x; touch_item_y = y;
                touch_item_hint = stock_index >= 0 ? "Tap to buy or drag to inventory" : std::string(action.label);
                if (can_sell) touch_item_hint = "Tap or drag to the vendor to sell";
                if (stock_index >= 0 || can_sell) follow_pointer();
                return true;
            }
        }
        if (phase == touch::Phase::Down && chrome.map.contains(viewport_x, y)) {
            toggle_touch_map();
            return true;
        }
        if (tree_open && scene) {
            const int tree_panel_right = kTreeR;
            const int cls = std::max(character.character_class, 0);
            const auto clear_tree_preview = [&] {
                touch_learn_skill = -1;
                touch_tree_tap_skill = -1;
                touch_tree_tap_ms = 0;
                touch_selected = skillpad::kEmpty;
                touch_pointer = {};
            };
            if (phase == touch::Phase::Down && touch_learn_skill >= 0 && touch::kTreeLearnConfirm.contains(x, y)) {
                send(cmd::SkillPoint{ touch_learn_skill });
                clear_tree_preview();
                return true;
            }
            if (phase == touch::Phase::Down && touch::kTreeLearnCancel.contains(x, y)) {
                clear_tree_preview();
                return true;
            }
            if (phase == touch::Phase::Down && touch::kTreeAssign.contains(x, y) && touch_selected >= 0) {
                touch_assign = touch_selected; touch_feedback.clear(); touch_assignment_from_tree = true; tree_open = false;
                clear_tree_preview();
                touch_play_ui_click(UiClick::TreeAssign);
                return true;
            }
            const int index = skill_at(*scene, cls, tree_tab, int(x), int(y), tree_panel_right);
            const auto& ids = scene->skills.class_ids[std::size_t(cls)];
            if (index >= 0 && std::size_t(index) < ids.size()) {
                if (phase == touch::Phase::Down) {
                    touch_pointer.x = x; touch_pointer.y = y; touch_pointer.active = true;
                    touch_pointer.press = touch_pointer.release = false;
                    const bool can = character.stats.get(d2d::d2s::kSkillPts) > 0
                        && d2d::rules::can_learn(scene->rules, cls, index, character.stats.skills, int(character.stats.get(d2d::d2s::kLevel)));
                    const bool double_tap = can && touch_tree_tap_skill == index
                        && now_ms >= touch_tree_tap_ms && now_ms - touch_tree_tap_ms <= touch::kTreeDoubleTapMs;
                    if (double_tap) {
                        send(cmd::SkillPoint{ index });
                        clear_tree_preview();
                    } else {
                        touch_tree_tap_skill = index;
                        touch_tree_tap_ms = now_ms;
                        touch_learn_skill = can ? index : -1;
                        touch_selected = skill_known(character.name, ids[std::size_t(index)]) ? ids[std::size_t(index)] : skillpad::kEmpty;
                    }
                }
                return true; // inspect first; spend only on Confirm or a second tap
            }
            if (phase == touch::Phase::Down
                && (touch_learn_skill >= 0 || touch_selected >= 0 || touch_pointer.active)
                && !touch::tree_preview_chrome_hit(x, y, touch_learn_skill >= 0, touch_selected >= 0)) {
                clear_tree_preview();
                // fall through: tabs and the tree close button still use pointer input
            }
        }
        return false; // other panels use their ordinary pointer input
    }
    const bool stamina_available = touch_shows_field_controls() && !view.dead && !inv_open && !stash_open
        && !cube_open && !belt_open && store.npc < 0 && !touch_sheet.open;
    if (touch_input.stamina_press.owns(finger) || (stamina_available && phase == touch::Phase::Down
        && touch::stamina_bar_box(chrome).contains(x, y))) {
        if (!stamina_available) { touch_input.stamina_press.clear(); return true; }
        const bool activated = touch_input.stamina_press.event(finger, phase, x, y, touch::stamina_bar_box(chrome));
        if (phase == touch::Phase::Up && activated) {
            touch_input.run_toggle = true;
            touch_play_ui_click(UiClick::Run, !view.running);
        }
        return true;
    }
    if (touch_input.potion_press.owns(finger) || (!touch_sheet.open && phase == touch::Phase::Down
        && (chrome.healing.contains(x, y) || chrome.mana.contains(x, y)))) {
        const auto pressed = touch_input.potion_press.event(finger, phase, x, y, chrome);
        if (pressed != touch::PotionButton::None) {
            const auto summary = potion_summary(character.items);
            const auto slot = pressed == touch::PotionButton::Healing ? summary.strongest_healing_slot : summary.strongest_mana_slot;
            if (slot) send(cmd::UseBelt{ *slot });
        }
        return true;
    }
    if (!touch_sheet.open && phase == touch::Phase::Down && chrome.belt_more.contains(x, y)) {
        const bool expand = !touch_input.belt_expanded;
        touch_input.clear();
        touch_input.belt_expanded = expand;
        touch_sheet = {};
        touch_play_ui_click(UiClick::Belt, expand);
        return true;
    }
    const auto belt_potion = [&](std::string_view code) {
        return potion_code(code) || (scene && scene->rules.potions.contains(std::string(code)));
    };
    const auto belt_can_fill = [&] {
        if (!scene) return false;
        auto trial = character.items;
        const int boxes = scene->belts[std::size_t(belt_index(*scene, character.items))].boxes;
        for (const auto& carried : character.items) {
            if (carried.location != d2d::d2s::item_location::kStored || !belt_potion(carried.code)) continue;
            auto copy = trial;
            if (d2d::rules::auto_belt(scene->rules, copy, carried.id, boxes)) return true;
        }
        return false;
    };
    const auto belt_do_fill = [&] {
        if (!scene) return;
        auto trial = character.items;
        const int boxes = scene->belts[std::size_t(belt_index(*scene, character.items))].boxes;
        std::vector<int> moved;
        for (const auto& carried : character.items)
            if (carried.location == d2d::d2s::item_location::kStored && belt_potion(carried.code)
                && d2d::rules::auto_belt(scene->rules, trial, carried.id, boxes)) moved.push_back(carried.id);
        for (const int id : moved) send(cmd::AutoBelt{ id });
    };
    if (touch_input.belt_expanded && touch_sheet.open && touch_sheet.belt_index >= 0) {
        if (phase == touch::Phase::Down) {
            if (chrome.belts[std::size_t(touch_sheet.belt_index)].contains(x, y)) {
                send(cmd::UseBelt{ touch_sheet.belt_index });
                touch_sheet = {};
                return true;
            }
            const auto hit = touch_sheet.hit(touch::to_legacy_x(x), y);
            if (hit == touch::ItemAction::Cancel || hit == touch::ItemAction::None) {
                if (hit == touch::ItemAction::Cancel || !touch_sheet.contains(touch::to_legacy_x(x), y)) touch_sheet = {};
                return true;
            }
            if (hit == touch::ItemAction::Confirm) {
                if (touch_sheet.chosen == touch::ItemAction::Use) send(cmd::UseBelt{ touch_sheet.belt_index });
                else if (touch_sheet.chosen == touch::ItemAction::Fill) belt_do_fill();
                touch_sheet = {};
                return true;
            }
            if (hit == touch_sheet.chosen && touch_sheet.confirming) {
                if (hit == touch::ItemAction::Use) send(cmd::UseBelt{ touch_sheet.belt_index });
                else if (hit == touch::ItemAction::Fill) belt_do_fill();
                touch_sheet = {};
                return true;
            }
            touch_sheet.chosen = hit;
            touch_sheet.confirming = true;
            touch_item_hint = "Tap Confirm";
            return true;
        }
        return phase != touch::Phase::Move;
    }
    if (touch_input.belt_expanded && phase == touch::Phase::Down) {
        for (int index = 0; index < 4; ++index)
            if (chrome.belts[std::size_t(index)].contains(x, y)) {
                const d2d::d2s::Item* belt_item = nullptr;
                for (const auto& item : character.items)
                    if (item.location == d2d::d2s::item_location::kBelt && item.column == index) { belt_item = &item; break; }
                const bool can_fill = belt_can_fill();
                if (!belt_item && !can_fill) return true;
                touch_sheet = touch::make_item_sheet(belt_item != nullptr, false, false, can_fill);
                touch_sheet.belt_index = index;
                touch_item_hint = "Tap Use or Fill, then Confirm. Second tap on the slot uses.";
                return true;
            }
    }
    if (phase == touch::Phase::Down && chrome.map.contains(x, y)) {
        toggle_touch_map();
        return true;
    }
    if (pad_field()) {
        if (phase == touch::Phase::Down && touch_interact_visible(touch_interact_pick) && chrome.interact.contains(x, y)) {
            if (touch_get_hold.finger == -1) touch_get_hold.begin(finger, now_ms);
            return true;
        }
        if (touch_get_hold.finger == finger) {
            if (phase == touch::Phase::Cancel || !chrome.interact.contains(x, y)) touch_get_hold.clear();
            else if (phase == touch::Phase::Up) {
                const bool long_press = touch_get_hold.update(now_ms);
                touch_get_hold.clear();
                if (!long_press && touch_interact_pick.kind == PadPick::kItem) {
                    if (open_loot_chooser(touch_player, view.player, true)) {
                        touch_input.clear(); touch_get_hold.clear();
                    } else send(cmd::Pickup{ touch_interact_pick.id });
                } else if (!long_press && touch_interact_pick.kind == PadPick::kThing) send(cmd::Interact{ touch_interact_pick.id });
            }
            return true;
        }
        if (phase == touch::Phase::Down && !touch::chrome_hit(x, y, touch_interact_visible(touch_interact_pick), true, touch_input.belt_expanded)) {
            touch_ground_finger = finger; touch_ground_x = x; touch_ground_y = y;
        } else if (finger == touch_ground_finger) {
            if (phase == touch::Phase::Move &&
                std::hypot(x - touch_ground_x, y - touch_ground_y) > 14.f)
                touch_ground_finger = -1;
            if (phase == touch::Phase::Up && touch_ground_finger == finger) {
                const float iso_u = (touch::to_legacy_x(x) - float(kScreenWidth) / 2) / (kIsoW / 2);
                const float iso_v = float(y - kViewY) / (kIsoH / 2);
                const float world_x = cam_x + (iso_u + iso_v) / 2, world_y = cam_y + (iso_v - iso_u) / 2;
                send(cmd::Move{ world_x, world_y, true });
                touch_ground_finger = -1;
            }
            if (phase == touch::Phase::Cancel) touch_ground_finger = -1;
        }
    }
    touch::Target target;
    bool stationary = false;
    if (phase == touch::Phase::Down) {
        const int slot = touch::bind_slot_at(chrome, x, y);
        if (slot >= 0) {
            target = resolve_skill(touch_player, touch_bound_skill(slot));
            const auto* info = scene ? scene->skills.get(target.skill) : nullptr;
            stationary = skillpad::binds_action(target.skill)
                || (info && (info->aura || centered_skill(*info) || skillpad::portal_name(info->name)));
        }
    }
    const auto aim_from_drag = [&] {
        if (!touch_input.owns_skill(finger)) return;
        touch_input.aim_x = x;
        touch_input.aim_y = y;
        const float dx = x - touch_input.origin_x, dy = y - touch_input.origin_y;
        if (touch_skill_hold.finger == finger && touch::beyond_skill_aim(dx, dy)) {
            touch_skill_hold.cancel();
            touch_cancel_skill_chase();
        }
        if (touch_skill_hold.cancelled) { touch_input.preview.valid = false; return; }
        touch_input.dragged = touch_input.dragged || (!touch_input.stationary && std::hypot(dx, dy) > 10);
        if (touch_skill_hold.release_casts()) touch_refresh_skill_aim();
    };
    if (phase == touch::Phase::Down && target.skill >= 0 && scene) {
        const auto* info = scene->skills.get(target.skill);
        if (info && info->srvdofunc == d2d::rules::ServerDoFunction::kTeleport) touch_teleport.clear();
    }
    if (phase == touch::Phase::Move || phase == touch::Phase::Up) aim_from_drag();
    if (phase == touch::Phase::Up && touch_input.owns_skill(finger) && scene) {
        const auto* info = scene->skills.get(touch_input.preview.skill);
        if (info && info->srvdofunc == d2d::rules::ServerDoFunction::kTeleport) {
            const float dx = touch_input.aim_x - touch_input.origin_x;
            const float dy = touch_input.aim_y - touch_input.origin_y;
            const auto current = resolve_skill(touch_player, touch_input.preview.skill, dx, dy, touch_input.dragged);
            if ((touch_input.dragged || touch_teleport.shown) && !touch_teleport.matches(current))
                touch_input.preview.valid = false;
            else touch_input.preview = current;
            touch_teleport.clear();
        }
    }
    if (phase == touch::Phase::Cancel && touch_input.owns_skill(finger)) touch_teleport.clear();
    if ((phase == touch::Phase::Up || phase == touch::Phase::Cancel) && touch_skill_hold.finger == finger) {
        if (!touch_skill_hold.release_casts()) touch_input.preview.valid = false;
        touch_cancel_skill_chase();
        touch_skill_hold.clear();
    }
    release.armed = false;
    const bool was_alternate = touch_input.alternate;
    touch_input.event(finger, phase, x, y, target, stationary, false, now_ms);
    if (was_alternate != touch_input.alternate) touch_play_ui_click(UiClick::BindSet, touch_input.alternate);
    if (phase == touch::Phase::Down && touch_input.owns_skill(finger) && target.valid && scene) {
        const auto* info = scene->skills.get(target.skill);
        if (info && repeatable_skill(*info, world.fight)) {
            const auto facing = touch_player.move.active
                ? pad::WorldStep{ touch_player.face_x, touch_player.face_y } : touch_facing(view.player);
            touch_skill_hold.begin(finger, now_ms, target.unit, facing.x * 3.f, facing.y * 3.f, centered_skill(*info));
            touch_skill_hold.chases = !stationary && !world.fight.missile_skill(*info) && !Fight::spot_skill(*info);
        }
    }
    return true;
}

void Town::touch_cancel_skill_chase() {
    if (touch_skill_hold.sent_to < 0 || !touch_skill_hold.chases) return;
    touch_skill_hold.sent_to = -1;
    touch_skill_hold.sent = false;
    touch_stop_chase = true;
}

void Town::touch_stop_skill_chase(bool keep_walk) {
    if (!std::exchange(touch_stop_chase, false)) return;
    // A fresh stick destination clears the queued approach and keeps walking.
    if (keep_walk) touch_player.walking = false;
    else if (view.pmode < 0 && (!net_game || view.player.walking))
        send(cmd::Move{ view.player.x, view.player.y, true });
}

void Town::touch_refresh_skill_aim() {
    const float dx = touch_input.aim_x - touch_input.origin_x, dy = touch_input.aim_y - touch_input.origin_y;
    const auto& preview = touch_input.preview;
    touch_input.preview = resolve_skill(touch_player, preview.skill, dx, dy, touch_input.dragged,
        nullptr, {}, -1, preview.unit);
    if (touch_input.owns_skill(touch_skill_hold.finger)) touch_skill_hold.target = touch_input.preview.unit;
}

void Town::drive_touch_skill_hold() {
    if (!touch_skill_hold.active(now_ms) || !scene || !level) return;
    auto& target = touch_input.preview;
    const auto* skill = scene->skills.get(target.skill);
    if (!skill || (!skill->in_town && d2d::rules::level_ids::is_town(level->id))) return;
    if (touch_input.dragged) {
        const auto offset = touch_skill_offset(touch_input.aim_x - touch_input.origin_x, touch_input.aim_y - touch_input.origin_y);
        // Returning to the button center keeps the drag's last direction.
        if (offset.x != 0 || offset.y != 0) touch_skill_hold.steer(offset.x, offset.y);
    } else if (touch_player.move.active) {
        touch_skill_hold.steer(touch_player.face_x * 3.f, touch_player.face_y * 3.f);
    }
    const auto foes = touch_skill_hold.at_caster ? std::vector<pad::Candidate>{} : touch_skill_foes(view, *scene, level);
    const auto aim = touch_skill_aim(*skill, world.fight, true);
    touch_skill_hold.resolve(target, view.player.x, view.player.y, foes, aim);
    constexpr std::uint32_t tick = d2d::game::kTickMs;
    bool ready = view.pmode < 0 && skill_known(character.name, skill->id);
    std::uint32_t interval = tick;
    if (lan_client || net_game) {
        // No remote cooldown timestamp is exposed. Use the row's delay
        // and space retries while waiting for an animation or chase update.
        if (ready) {
            const int lvl = std::max(1, skill_level(*scene, character, skill->id, {}));
            const int frames = d2d::rules::eval_calc(scene->skills, skill->delay, {}, skill->id, lvl);
            interval = std::max(4 * tick, std::uint32_t(std::max(0, frames)) * tick);
        }
    } else {
        const auto& actor = focused();
        const auto next_tick = world_ms + tick;
        const int index = touch_skill_hold.chases ? world.fight.monster_index(target.unit) : -1;
        const bool approaching = actor.pmode < 0 && touch_skill_hold.chases && index >= 0
            && actor.attack_mon == index && actor.attack_skill == skill->id;
        ready = !approaching && (actor.pmode < 0 || next_tick >= actor.pmode_until)
            && (skill->delay.empty() || next_tick >= actor.skill_delay) && skill_known(character.name, skill->id);
    }
    if (touch_skill_hold.dispatch(now_ms, ready, interval)) cast_resolved(touch_player, target);
}

void Town::drive_touch_hold(float seconds) {
    if (!touch_shows_field_controls() || view.dead) {
        for (std::size_t button = 0; button < 7; ++button) {
            if (touch_pulses.sample(button, now_ms).kind == touch::FeedbackKind::Pressed)
                touch_pulses.release(button);
            touch_pulse_fingers[button] = -1;
        }
    }
    touch_hold_fraction = 0.f;
    if (!touch_enabled || touch_item_finger == -1 || touch_sold || !scene || touch_sheet.open || touch_item_drag) {
        if (touch_item_finger == -1) touch_hold.clear();
        return;
    }
    const auto* item = touch_bag_item();
    if (!item) return;
    PadHoldQuery query;
    query.shop = store.npc >= 0;
    query.gamble = store.gamble;
    query.identify = identify_with >= 0;
    query.imbue = use_item.open();
    query.hand_empty = !held;
    query.bag = true;
    query.store_mode = store.mode;
    if (world.game_data) {
        query.sellable = touch_sell_vendor == store.npc && touch_can_sell(item);
        query.potion = potion_code(item->code) || scene->rules.potions.contains(item->code);
        const int save_cls = std::max(character.character_class, 0);
        const int level_needed = d2d::game::required_level(*world.game_data, *item, save_cls);
        const auto worn_by = d2d::game::wearer(*world.game_data, save_cls, character.items, character.stats);
        query.equippable = d2d::rules::bag_equip_slot(scene->rules, character.items, held, item->id, worn_by, level_needed) >= 0;
    }
    query.equippable = pad_hold_can_equip(net_game != nullptr, query.equippable);
    const PadHoldKind kind = pad_hold_kind(query);
    if (kind == PadHoldKind::Sell) touch_hold.delay = pad::kHoldSellSeconds;
    else if (kind == PadHoldKind::Equip) touch_hold.delay = pad::kHoldEquipSeconds;
    else touch_hold.delay = pad::kHoldFillSeconds;
    const bool eligible = kind != PadHoldKind::None && inv_open;
    const hold_confirm::Step step = touch_hold.step(false, true, eligible, item->id, int(kind), seconds);
    if (touch_hold.armed && !touch_hold.cancelled) touch_hold_fraction = touch_hold.fraction();
    if (step != hold_confirm::Step::Act) return;
    if (kind == PadHoldKind::Sell) touch_sold = touch_trade(touch::ItemAction::Sell);
    else if (kind == PadHoldKind::Equip) send(cmd::EquipFromBag{ item->id });
    else if (kind == PadHoldKind::Fill || kind == PadHoldKind::FillOne) {
        auto trial = character.items;
        const int boxes = scene->belts[std::size_t(belt_index(*scene, character.items))].boxes;
        std::vector<int> moved;
        const auto try_item = [&](int carried_id) {
            if (d2d::rules::auto_belt(scene->rules, trial, carried_id, boxes)) moved.push_back(carried_id);
        };
        const auto is_potion = [&](std::string_view code) {
            return potion_code(code) || scene->rules.potions.contains(std::string(code));
        };
        try_item(item->id);
        if (kind == PadHoldKind::Fill)
            for (const auto& carried : character.items)
                if (carried.id != item->id && is_potion(carried.code)) try_item(carried.id);
        for (const int carried_id : moved) send(cmd::AutoBelt{ carried_id });
    }
}

/**
 * Consume touch state into movement, attack, belt, item-sale and committed skill commands.
 * Invalidate gestures when field input loses ownership, and require a fresh attack press before
 * pickup.
 */
void Town::drive_touch() {
    if (!touch_enabled || last_device != touch::LastDevice::Touch) {
        touch_cancel_skill_chase();
        touch_stop_skill_chase(false);
        cancel_touch_item();
        touch_skill_hold.clear(); touch_get_hold.clear(); touch_input.clear(); return;
    }
    const int panel_state = (store.npc >= 0 ? store.npc + 2 : 0) + (inv_open ? 1000 : 0) + (tree_open ? 2000 : 0) +
                            (char_open ? 4000 : 0) + (game_menu.open ? 8000 : 0) + (skillbar.picking ? 16000 : 0);
    if (panel_state != touch_panel_state) {
        cancel_touch_item();
        if (!tree_open) { touch_learn_skill = -1; touch_tree_tap_skill = -1; }
        touch_panel_state = panel_state;
    }
    if (!pad_field() || view.dead || loot_chooser.open || touch_effect.modal || touch_input.menu) {
        touch_cancel_skill_chase();
        if (!view.dead) touch_stop_skill_chase(false);
        else touch_stop_chase = false;
        touch_teleport.clear(); touch_skill_hold.clear(); touch_get_hold.clear();
        const Send stop = [this](const Command& command) { send(command); };
        pad_walk(touch_player, view.player, view.dead, false, stop);
        if (touch_input.menu) { inv_open = true; tree_open = false; }
        touch_input.clear(); return;
    }
    const Send out = [this](const Command& command) { send(command); };
    if (!touch_input.move_held()) touch_input.move_x = touch_input.move_y = 0;
    const float move_len = std::hypot(touch_input.move_x, touch_input.move_y);
    touch_player.move = { touch_input.move_x, touch_input.move_y, touch_input.move_held() && move_len > 0.12f };
    if (touch_player.move.active) {
        const auto face = pad::world_step(touch_input.move_x, touch_input.move_y, kIsoW, kIsoH, 1.f);
        touch_player.face_x = face.x; touch_player.face_y = face.y;
    }
    if (touch_input.run_toggle) send(cmd::Run{ !view.running });
    if (touch_input.aiming && touch_skill_hold.release_casts()) touch_refresh_skill_aim();
    if (!touch_input.owns_skill(touch_skill_hold.finger)) {
        touch_cancel_skill_chase();
        touch_skill_hold.clear();
    }
    touch_stop_skill_chase(touch_player.move.active);
    pad_walk(touch_player, view.player, false, true, out);
    drive_touch_skill_hold();
    touch_attack_pick = pad_pick(touch_player, view.player, false);
    touch_interact_pick = pad_pick(touch_player, view.player, true, {}, false);
    touch_get_hold.update(now_ms);
    if (touch_input.belt >= 0) send(cmd::UseBelt{ touch_input.belt }); // belt_quick from the adapter; field tap uses the sheet
    if (touch_input.aiming && touch_input.preview.skill == skillpad::kShowItems) pad_show_items = true;
    for (const auto& target : std::exchange(touch_input.casts, {})) {
        if (target.valid && target.skill == skillpad::kShowItems) pad_show_items = true;
        else cast_resolved(touch_player, target);
    }

}

} // namespace d2d::client
