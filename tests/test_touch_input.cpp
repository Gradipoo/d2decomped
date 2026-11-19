// SPDX-License-Identifier: GPL-3.0-or-later
// test-links:
// test-includes: apps/d2d
// Actual touch adapter: independent fingers, latched page/skill, safe cancellation.
#include "skillpad.hpp"
#include "teleport_aim.hpp"
#include "touch_gestures.hpp"
#include "touch_input.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <exception>

/**
 * Exercise independent touch ownership, page latching, safe cancellation and attack press/hold
 * edges, including the single-pointer owner.
 */
int main() try {
    using namespace d2d::client::touch;
    d2d::client::TeleportAim teleport;
    const auto [half_x, half_y] = d2d::client::teleport_stick_drag(1.f, 0.f, .5f);
    const auto [full_x, full_y] = d2d::client::teleport_stick_drag(1.f, 0.f, 1.f);
    assert(half_x * 2.f == full_x && half_y == 0.f && full_y == 0.f);
    teleport.preview = { 54, -1, 5.f, 6.f, false, true };
    assert(!teleport.matches(teleport.preview));
    teleport.display();
    assert(teleport.matches(teleport.preview));
    auto changed = teleport.preview; changed.x += .2f;
    assert(!teleport.matches(changed));
    changed = teleport.preview; changed.valid = false;
    assert(!teleport.matches(changed));
    teleport.clear();
    assert(!teleport.matches(changed));
    HoldGesture hold;
    hold.begin(12, 1000);
    assert(!hold.update(1299));
    assert(hold.update(1300));
    hold.clear();
    assert(!hold.update(1500));
    hold.begin(13, 2000);
    assert(!hold.update(2050)); // a short tap remains a tap
    hold.clear();
    Input input;
    const Target fire{ 54, 7, 5, 6, false, true };
    input.event(1, Phase::Down, 126, 462);
    input.event(1, Phase::Move, 191, 462);
    assert(input.move_x == 1 && input.move_y == 0);
    input.event(2, Phase::Down, 565, 555, fire);
    input.event(2, Phase::Move, 630, 500);
    assert(input.aiming && input.dragged && input.casts.empty());
    input.event(3, Phase::Down, 765, 440);
    assert(input.alternate && input.move_x == 1 && input.preview.skill == 54);
    input.event(2, Phase::Cancel, 610, 450);
    assert(!input.aiming && input.casts.empty() && input.move_x == 1);
    input.event(2, Phase::Down, 565, 555, fire);
    input.event(2, Phase::Up, 380, 170);
    assert(input.casts.size() == 1 && input.casts[0].skill == 54); // old cancel strip still casts
    input.frame();
    input.event(2, Phase::Down, 565, 555, fire);
    input.event(2, Phase::Up, -1, 170);
    assert(input.casts.empty()); // lift off the 800x600 band
    input.event(2, Phase::Down, 565, 555, fire);
    input.event(2, Phase::Up, 380, 600);
    assert(input.casts.empty());
    input.event(2, Phase::Down, 565, 555, fire);
    input.event(1, Phase::Up, 191, 462);
    assert(input.aiming && input.casts.empty() && input.move_x == 0);
    input.event(2, Phase::Up, 565, 555);
    assert(input.casts.size() == 1 && input.casts[0].skill == 54 && input.casts[0].unit == 7);
    input.frame();
    input.event(1, Phase::Down, 126, 462);
    input.event(1, Phase::Move, 171, 462);
    assert(input.move_held());
    input.release_finger(1);
    assert(!input.move_held() && input.move_x == 0 && input.move_y == 0);
    input.event(1, Phase::Down, 126, 462);
    input.event(1, Phase::Move, 171, 462);
    input.event(2, Phase::Down, 565, 555, fire, true);
    assert(input.casts.empty()); // even self casts are cancellable until lift
    input.event(2, Phase::Cancel, 565, 555);
    assert(input.casts.empty() && input.move_x > 0);
    input.event(2, Phase::Down, 565, 555, fire);
    input.clear(); // a panel/focus change invalidates all pending gestures
    input.event(2, Phase::Up, 565, 555);
    assert(input.casts.empty() && input.move_x == 0 && !input.aiming);
    input.event(3, Phase::Down, 736, 536);
    assert(input.attack_press && input.attack_down);
    input.frame();
    assert(!input.attack_press && input.attack_down);
    input.event(3, Phase::Up, 736, 536);
    assert(!input.attack_down);
    // Single-pointer adapters have a separate valid owner, not the no-finger sentinel.
    input.clear();
    input.event(-2, Phase::Down, 736, 536);
    assert(input.attack_down && input.owns_attack(-2));
    input.event(8, Phase::Down, 736, 536);
    assert(input.owns_attack(-2) && !input.owns_attack(8));
    input.event(-2, Phase::Up, 736, 536);
    assert(!input.attack_down);
    input.clear();
    input.event(-2, Phase::Down, 126, 462);
    input.event(-2, Phase::Move, 191, 462);
    assert(input.move_held() && input.move_x == 1);
    assert(!chrome_hit(400, 300));
    assert(!chrome_hit(200, 200));
    { const auto box = layout_boxes().interact; assert(chrome_hit(box.x + 1.f, box.y + 1.f, true)); }
    // Field controls use viewport coordinates; panel coordinates remain centered.
    viewport_width = 1200.f;
    safe_inset = { 23.f, 11.f, 47.f, 19.f };
    const auto wide = layout_boxes();
    const auto separate = [](Box a, Box b) {
        return a.x + a.width <= b.x || b.x + b.width <= a.x || a.y + a.height <= b.y || b.y + b.height <= a.y;
    };
    assert(separate(wide.healing, wide.move) && separate(wide.mana, wide.move));
    assert(separate(wide.healing, wide.interact) && separate(wide.mana, wide.interact));
    for (const auto& skill : wide.skills) assert(separate(wide.healing, skill) && separate(wide.mana, skill));
    assert(wide.menu.x == 29.f);
    assert(std::abs(wide.attack.x + wide.attack.width - (1200.f - 47.f - 6.f)) < .01f);
    assert(to_legacy_x(200.f) == 0.f && to_viewport_x(800.f) == 1000.f);
    assert(translate_legacy_box(kTreeAssign).x == kTreeAssign.x + 200.f);
    const auto wide_skill = wide.skills[0];
    const float wide_x = wide_skill.x + wide_skill.width / 2.f;
    const float wide_y = wide_skill.y + wide_skill.height / 2.f;
    input.clear(); input.event(10, Phase::Down, wide_x, wide_y, fire);
    input.event(10, Phase::Up, 1199.f, wide_y);
    assert(input.casts.size() == 1);
    input.frame(); input.event(10, Phase::Down, wide_x, wide_y, fire);
    input.event(10, Phase::Up, 1200.f, wide_y);
    assert(input.casts.empty());
    input.event(10, Phase::Down, wide_x, wide_y, fire);
    input.event(10, Phase::Cancel, wide_x, wide_y);
    assert(input.casts.empty() && !input.aiming);
    viewport_width = 1001.f;
    assert(legacy_offset() == 100.f);
    assert(to_viewport_x(0.f) == 100.f && to_legacy_x(100.f) == 0.f);
    assert(to_viewport_x(800.f) == 900.f && to_legacy_x(900.f) == 800.f);
    assert(translate_legacy_box(kTreeAssign).x == kTreeAssign.x + 100.f);
    const auto odd = layout_boxes();
    assert(std::abs(odd.attack.x + odd.attack.width - (1001.f - 47.f - 6.f)) < .01f);
    viewport_width = 800.f;
    input.clear();
    // layout_boxes positions depend on safe_inset and kUiScale only, not on pad_field().
    safe_inset = {};
    const auto field = layout_boxes();
    assert(std::abs(field.skills_bar.y - field.menu.y) < 1.f);
    assert(field.skills_bar.x >= field.menu.x + field.menu.width + ui_scale(kChipGap) - 1.f);
    assert(std::abs(field.quest.y - field.bind.y) < 1.f);
    assert(field.quest.y >= field.menu.y + field.menu.height + ui_scale(kChipGap) - 1.f);
    assert(std::abs(field.bind.x - field.skills_bar.x) < 1.f);
    assert(field.stats.x >= field.skills_bar.x + field.skills_bar.width + ui_scale(kChipGap) - 1.f);
    assert(chip_hit(field.stats.x + field.stats.width / 2.f, field.stats.y + field.stats.height / 2.f));
    assert(field.menu.y < 12.f);
    assert(field.move.contains(field.move_visual.x + field.move_visual.width * .5f, field.move_visual.y + field.move_visual.height * .5f));
    assert(field.menu.x < 20.f && field.done.x + field.done.width > 740.f);
    assert(field.done.width >= ui_scale(64.f) && field.done.height >= ui_scale(64.f));
    assert(field.map.y >= field.done.y + field.done.height);
    assert(field.attack.x + field.attack.width > 790.f && field.attack.y + field.attack.height > 590.f);
    assert(bind_slot_at(field, field.skills[0].x + 2.f, field.skills[0].y + 2.f) == 0);
    assert(bind_slot_at(field, field.skills[3].x + 2.f, field.skills[3].y + 2.f) == 3);
    assert(bind_slot_at(field, field.attack.x + 8.f, field.attack.y + 8.f) == kAttackBind);
    assert(bind_slot_at(field, 400.f, 300.f) == -1);
    const auto overlap = [](Box left, Box right) {
        return left.x < right.x + right.width && right.x < left.x + left.width && left.y < right.y + right.height && right.y < left.y + left.height;
    };
    const auto separated_by = [](Box left, Box right, float gap) {
        return left.x + left.width + gap <= right.x || right.x + right.width + gap <= left.x
            || left.y + left.height + gap <= right.y || right.y + right.height + gap <= left.y;
    };
    for (const float width : { 800.f, 1067.f, 1300.f }) for (const SafeInset inset : { SafeInset{}, SafeInset{ 23.f, 11.f, 47.f, 19.f } }) {
        viewport_width = width; safe_inset = inset;
        const auto layout = layout_boxes();
        const auto wheel = bind_wheel_layout(layout);
        assert(wheel.previous.width == 64.f && wheel.previous.height == 64.f);
        assert(wheel.next.width == 64.f && wheel.next.height == 64.f);
        assert(!overlap(wheel.previous, wheel.indicator) && !overlap(wheel.next, wheel.indicator));
        for (const auto box : { wheel.previous, wheel.next }) {
            const float x = box.x + box.width / 2, y = box.y + box.height / 2;
            assert(bind_slot_at(layout, x, y) == -1);
            assert(!overlap(box, layout.page) && !overlap(box, layout.attack));
            for (const auto skill : layout.skills) assert(!overlap(box, skill));
        }
        const std::array<Box, 16> controls = { layout.done, layout.map, layout.belt_more,
            layout.belts[0], layout.belts[1], layout.belts[2], layout.belts[3],
            layout.attack, layout.page, layout.skills[0], layout.skills[1], layout.skills[2], layout.skills[3],
            layout.interact, layout.healing, layout.mana };
        for (std::size_t i = 0; i < controls.size(); ++i) {
            const auto box = controls[i];
            assert(box.x >= inset.left && box.y >= inset.top);
            assert(box.x + box.width <= width - inset.right && box.y + box.height <= 600.f - inset.bottom);
            assert(!overlap(box, layout.life_orb) && !overlap(box, layout.mana_orb));
            for (std::size_t j = i + 1; j < controls.size(); ++j) assert(!overlap(box, controls[j]));
        }
        assert(!overlap(layout.life_orb, layout.mana_orb));
        assert(layout.move.width >= 210.f + (width - 800.f) * .45f - inset.left - inset.right);
        assert(layout.healing.y > layout.mana.y);
        const float attack_cx = layout.attack.x + layout.attack.width / 2.f;
        for (const Box skill : layout.skills) assert(skill.width >= 80.f && skill.height >= 80.f);
        for (std::size_t i = 0; i < layout.skills.size(); ++i)
            for (std::size_t j = i + 1; j < layout.skills.size(); ++j)
                assert(separated_by(layout.skills[i], layout.skills[j], 8.f));
        assert(layout.healing.width >= 64.f && layout.mana.width >= 64.f);
        assert(layout.mana.x + layout.mana.width + 8.f <= layout.skills[1].x);
        assert(layout.healing.x + layout.healing.width + 8.f <= layout.skills[0].x);
        assert(layout.interact.y + layout.interact.height + 16.f <= layout.mana.y);
        assert(layout.healing.x + layout.healing.width / 2.f < attack_cx);
        assert(layout.skills[0].x + layout.skills[0].width / 2.f < attack_cx);
        const std::array<Box, 8> thumb_controls = { layout.attack, layout.skills[0], layout.skills[1],
            layout.skills[2], layout.skills[3], layout.interact, layout.mana, layout.healing };
        for (std::size_t i = 0; i < thumb_controls.size(); ++i)
            for (std::size_t j = i + 1; j < thumb_controls.size(); ++j)
                assert(separated_by(thumb_controls[i], thumb_controls[j], 8.f));
    }
    viewport_width = 800.f; safe_inset = {};
    PotionPress press;
    const float heal_x = field.healing.x + 8.f, heal_y = field.healing.y + 8.f;
    assert(press.event(20, Phase::Down, heal_x, heal_y, field) == PotionButton::None);
    assert(press.event(21, Phase::Up, heal_x, heal_y, field) == PotionButton::None);
    assert(press.event(20, Phase::Up, heal_x, heal_y, field) == PotionButton::Healing);
    assert(press.event(20, Phase::Up, heal_x, heal_y, field) == PotionButton::None);
    press.event(20, Phase::Down, heal_x, heal_y, field);
    press.event(20, Phase::Move, 0.f, 0.f, field);
    assert(press.event(20, Phase::Up, heal_x, heal_y, field) == PotionButton::None);
    press.event(20, Phase::Down, heal_x, heal_y, field);
    assert(press.event(20, Phase::Cancel, heal_x, heal_y, field) == PotionButton::None);
    assert(press.event(20, Phase::Up, heal_x, heal_y, field) == PotionButton::None);
    assert(press.event(20, Phase::Down, 0.f, 0.f, field) == PotionButton::None);
    assert(press.event(20, Phase::Up, heal_x, heal_y, field) == PotionButton::None);
    assert(!overlap(field.map, field.belts[1]));
    assert(!overlap(field.map, field.belts[3]));
    assert(chip_hit(field.menu.x + 2.f, field.menu.y + 2.f));
    assert(chip_hit(field.bind.x + 2.f, field.bind.y + 2.f));
    assert(!chip_hit(field.move.x + 8.f, field.move.y + 8.f));
    assert(!chrome_hit(400, 80, false, false));
    assert(touch_interface_visible(true, LastDevice::Touch));
    assert(!touch_interface_visible(true, LastDevice::Gamepad));
    assert(touch_hud_visible(true, LastDevice::Touch, false));
    assert(!touch_hud_visible(true, LastDevice::Gamepad, false));
    assert(!touch_hud_visible(true, LastDevice::MouseKeyboard, false));
    assert(!touch_hud_visible(true, LastDevice::Touch, true));
    assert(touch_owns_locomotion(LastDevice::Touch, 1, false));
    assert(touch_owns_locomotion(LastDevice::Touch, 0, true));
    assert(!touch_owns_locomotion(LastDevice::Gamepad, 1, true));
    const auto icon_at = centred_blit_origin({ 10.f, 20.f, 100.f, 80.f }, 40.f, 20.f);
    assert(std::abs(icon_at.first - 40.f) < 0.01f && std::abs(icon_at.second - 50.f) < 0.01f);
    const float attack_scale = fit_icon_scale(96.f, 96.f, 48.f, 48.f, 0.10f);
    assert(attack_scale > 1.5f && attack_scale < 1.7f);
    {
        const SnapGrid grid{ 400.f, 100.f, 29.f, 29.f, 10, 4, 2, 4 };
        int col = -8, row = -8;
        assert(snap_grid_cell(grid, 395.f, 160.f, col, row));
        assert(col == 0 && row >= 0 && row < 4);
        const auto at = snap_grid_point(grid, col, row);
        int col2 = -8, row2 = -8;
        assert(snap_grid_cell(grid, at.first, at.second, col2, row2) && col2 == col && row2 == row);
        assert(snap_grid_cell(grid, 680.f, 160.f, col, row) && col == grid.cols - grid.item_w);
        assert(!snap_grid_cell(grid, 200.f, 160.f, col, row));
    }
    assert(!item_dragged(10.f, 10.f, 12.f, 10.f));
    assert(item_dragged(10.f, 10.f, 10.f + kItemDragSlop + 1.f, 10.f));
    const auto sheet = make_item_sheet(true, false, false);
    assert(sheet.open && sheet.show_use && !sheet.show_sell && !sheet.show_buy);
    assert(sheet.hit(sheet.use.x + 1.f, sheet.use.y + 1.f) == ItemAction::Use);
    assert(sheet.hit(sheet.confirm.x + 1.f, sheet.confirm.y + 1.f) == ItemAction::Confirm);
    assert(sheet.hit(sheet.cancel.x + 1.f, sheet.cancel.y + 1.f) == ItemAction::Cancel);
    const auto fill_sheet = make_item_sheet(true, false, false, true);
    assert(fill_sheet.show_fill && fill_sheet.show_use);
    assert(fill_sheet.hit(fill_sheet.fill.x + 1.f, fill_sheet.fill.y + 1.f) == ItemAction::Fill);
    const auto identify = make_confirm_sheet();
    assert(identify.open && identify.identifying_target && !identify.show_use);
    assert(identify.hit(identify.confirm.x + 1.f, identify.confirm.y + 1.f) == ItemAction::Confirm);
    assert(identify.hit(identify.use.x + 1.f, identify.use.y + 1.f) == ItemAction::None);
    input.clear();
    input.belt_expanded = true;
    input.event(4, Phase::Down, field.belts[0].x + 2.f, field.belts[0].y + 2.f, {}, false, false, 1000);
    assert(input.belt < 0 && !input.belt_quick && input.belt_armed == 0);
    input.event(4, Phase::Up, field.belts[0].x + 2.f, field.belts[0].y + 2.f, {}, false, false, 1010);
    input.event(5, Phase::Down, field.belts[0].x + 2.f, field.belts[0].y + 2.f, {}, false, false, 1200);
    assert(input.belt == 0 && input.belt_quick);
    input.frame();
    input.event(6, Phase::Down, field.belts[1].x + 2.f, field.belts[1].y + 2.f, {}, false, false, 2000);
    assert(input.belt < 0 && !input.belt_quick && input.belt_armed == 1);
    input.clear();
    input.event(9, Phase::Down, field.done.x + 2.f, field.done.y + 2.f);
    assert(input.done);
    input.frame();
    input.alternate = false;
    input.event(8, Phase::Down, field.page.x + 2.f, field.page.y + 2.f);
    assert(input.alternate);
    assert(deliberate_double_tap(10.f, 10.f, 12.f, 11.f, 1000, 1250));
    assert(!deliberate_double_tap(10.f, 10.f, 12.f, 11.f, 1000, 1000 + kMoveDoubleTapMs + 1));
    assert(!deliberate_double_tap(10.f, 10.f, 10.f + kMoveTapSlop + 4.f, 10.f, 1000, 1200));
    assert(kTreeLearnConfirm.contains(20.f, 500.f) && kTreeLearnCancel.contains(140.f, 500.f));
    assert(!kTreeLearnConfirm.contains(140.f, 500.f));
    assert(tree_preview_chrome_hit(20.f, 500.f, true, false));
    assert(tree_preview_chrome_hit(140.f, 500.f, true, false));
    assert(tree_preview_chrome_hit(20.f, 560.f, false, true));
    assert(!tree_preview_chrome_hit(400.f, 300.f, true, true));
    input.clear();
    const float mx = field.move.x + 40.f, my = field.move.y + 40.f;
    input.event(1, Phase::Down, mx, my, {}, false, false, 3000);
    input.event(1, Phase::Up, mx, my, {}, false, false, 3050);
    assert(!input.run_toggle);
    input.event(1, Phase::Down, mx + 2.f, my + 1.f, {}, false, false, 3200);
    assert(input.run_toggle && input.move_held());
    input.clear();
    input.event(1, Phase::Down, mx, my, {}, false, false, 4000);
    input.event(1, Phase::Move, mx + 60.f, my, {}, false, false, 4100);
    input.event(1, Phase::Up, mx + 60.f, my, {}, false, false, 4200);
    input.event(1, Phase::Down, mx + 60.f, my, {}, false, false, 4300);
    assert(!input.run_toggle);
    input.clear();
    input.event(1, Phase::Down, mx, my, {}, false, false, 6000);
    input.event(1, Phase::Move, mx + 60.f, my, {}, false, false, 6050);
    input.event(1, Phase::Up, mx, my, {}, false, false, 6100);
    input.event(1, Phase::Down, mx, my, {}, false, false, 6200);
    assert(!input.run_toggle);
    input.clear();
    input.event(1, Phase::Down, mx, my, {}, false, false, 5000);
    input.event(1, Phase::Up, mx, my, {}, false, false, 5050);
    input.event(1, Phase::Down, mx, my, {}, false, false, 5050 + kMoveDoubleTapMs + 40);
    assert(!input.run_toggle);
    input.clear();
    input.event(7, Phase::Down, field.attack.x + 8.f, field.attack.y + 8.f, {}, false, true);
    assert(input.destination == kAttackBind);
    input.frame();
    const Target attack_fire{ 54, 7, 5, 6, false, true };
    input.event(7, Phase::Down, field.attack.x + 8.f, field.attack.y + 8.f, attack_fire);
    assert(input.attack_down && input.aiming && !input.dragged);
    input.event(7, Phase::Move, field.attack.x - 40.f, field.attack.y - 40.f, attack_fire);
    assert(input.aiming && input.dragged);
    input.event(7, Phase::Up, field.attack.x - 40.f, field.attack.y - 40.f, attack_fire);
    assert(input.casts.size() == 1 && input.casts[0].skill == 54 && !input.attack_down);
    assert(kBarBlockH >= 64.f);
    assert(field.quest_track.y + 1.f >= field.bind.y + field.bind.height + bar_gap() + kBarBlockH);
    {
        const Box stamina = stamina_bar_box(field);
        assert(stamina.x == field.menu.x && stamina.width < (field.bind.x + field.bind.width - field.menu.x));
        assert(stamina.y >= field.bind.y + field.bind.height);
        assert(stamina.y + stamina.height < field.quest_track.y);
        assert(stamina.y + stamina.height > field.bind.y + field.bind.height + bar_gap());
        StaminaPress stamina_press;
        assert(stamina_press.event(20, Phase::Down, stamina.x + 3.f, stamina.y + 3.f, stamina));
        assert(stamina_press.owns(20) && !stamina_press.owns(21));
        assert(!stamina_press.event(21, Phase::Up, stamina.x + 3.f, stamina.y + 3.f, stamina));
        assert(stamina_press.event(20, Phase::Up, stamina.x + 3.f, stamina.y + 3.f, stamina));
        assert(!stamina_press.owns(20));
        Input movement;
        movement.event(61, Phase::Down, field.move.x + 10.f, field.move.y + 10.f);
        assert(movement.move_held());
        assert(stamina_press.event(62, Phase::Down, stamina.x + 3.f, stamina.y + 3.f, stamina));
        assert(stamina_press.event(62, Phase::Up, stamina.x + 3.f, stamina.y + 3.f, stamina));
        assert(movement.move_held()); // The tap remains independent of the movement finger.
        movement.event(61, Phase::Cancel, field.move.x + 10.f, field.move.y + 10.f);
        assert(stamina_press.event(22, Phase::Down, stamina.x + 3.f, stamina.y + 3.f, stamina));
        assert(stamina_press.event(22, Phase::Cancel, stamina.x + 3.f, stamina.y + 3.f, stamina));
        assert(!stamina_press.owns(22));
        assert(stamina_press.event(23, Phase::Down, stamina.x + 3.f, stamina.y + 3.f, stamina));
        assert(stamina_press.event(23, Phase::Move, stamina.x + stamina.width + 2.f, stamina.y + 3.f, stamina));
        assert(!stamina_press.event(23, Phase::Up, stamina.x + 3.f, stamina.y + 3.f, stamina));
        Input interrupted;
        assert(interrupted.stamina_press.event(-2, Phase::Down, stamina.x + 3.f, stamina.y + 3.f, stamina));
        interrupted.release_finger(99);
        assert(interrupted.stamina_press.owns(-2));
        interrupted.release_finger(-2); // An early panel route must drop the lifted finger.
        assert(!interrupted.stamina_press.owns(-2));
        assert(!interrupted.stamina_press.event(-2, Phase::Up, stamina.x + 3.f, stamina.y + 3.f, stamina));
        assert(interrupted.stamina_press.event(-2, Phase::Down, stamina.x + 3.f, stamina.y + 3.f, stamina));
        interrupted.clear(); // Focus loss/panel changes cannot complete a stale tap.
        assert(!interrupted.stamina_press.event(-2, Phase::Up, stamina.x + 3.f, stamina.y + 3.f, stamina));
        const Box xp{ stamina.x + stamina.width + 4.f, stamina.y, stamina.width, stamina.height };
        assert(!stamina.contains(xp.x + 2.f, xp.y + 2.f));
    }
    assert(!swipe_aims(false, true, false));
    assert(swipe_aims(true, true, true));
    assert(swipe_aims(true, false, false));
    assert(!swipe_aims(true, false, true));
    {
        using namespace d2d::client::skillpad;
        const std::array ids{ 0, 6, 7, 8, 9, 54, 36, 42, 61 };
        assert(bind_wheel_page_count(int(ids.size())) == 3);
        const auto first = bind_wheel_page(int(ids.size()), 0);
        const auto second = bind_wheel_page(int(ids.size()), 1);
        const auto actions = bind_wheel_page(int(ids.size()), 2);
        assert(first.start == 0 && first.shown == 8);
        assert(second.start == 8 && second.shown == 1);
        assert(actions.start == 9 && actions.shown == int(kActions.size()));
        assert(bind_wheel_page_count(0) == 1 && bind_wheel_page(0, 0).shown == int(kActions.size()));
        assert(bind_wheel_pick(0.f, 0.f, first, true) == -1);  // The hub itself cannot assign Attack.
        assert(bind_wheel_pick(40.f, 0.f, first) == -1);
        assert(bind_wheel_pick(40.f, 0.f, first, true) >= 0);
        const auto pick_icon = [](WheelPage page, int icon) {
            const float angle = wedge_center(icon, page.shown);
            return bind_wheel_pick(std::cos(angle) * kBindWheelRx, std::sin(angle) * kBindWheelRy, page);
        };
        assert(pick_icon(first, 5) == 5 && ids[5] == 54);
        assert(pick_icon(second, 0) == 8 && ids[8] == 61);
        assert(kActions[std::size_t(pick_icon(actions, 0) - actions.start)] == kEmpty);
        assert(kActions[std::size_t(pick_icon(actions, 2) - actions.start)] == kShowItems);
    }
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "test_touch_input: %s\n", error.what());
    return 1;
}
