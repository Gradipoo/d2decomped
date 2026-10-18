// SPDX-License-Identifier: GPL-3.0-or-later
// Gamepad -> named actions, with no SDL in it (gamepad.hpp feeds it): the
// sticks, triggers and buttons of one frame in, what the game does out. The
// default layout uses SDL's positional buttons (south/east/west/north), so an
// Xbox, PlayStation, Switch or Deck pad lands the same. See
// docs/design/controller-plan.md.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace d2d::client::pad {

enum class Button { South, East, West, North, Back, Start, LeftShoulder, RightShoulder, LeftStick, RightStick,
                    DpadUp, DpadDown, DpadLeft, DpadRight, Count };

// What the screen the pad plays is: the title (B is not Esc there: that quits),
// the other menus, or the game.
enum class Context { Title, Menu, Game };

// Keys the pad presses on the game's behalf: the adapter turns them into SDL
// key events, so every screen's own key handling runs as for the keyboard.
enum class Key { Escape, Return, Up, Down, Left, Right, Inventory, Character, SkillTree, QuestLog, Automap, ToggleRun,
                 Belt1, Belt2, Belt3, Belt4 };

struct Sample {                       // one frame of a pad: sticks -1..1, triggers 0..1
    float lx = 0, ly = 0, rx = 0, ry = 0, left_trigger = 0, right_trigger = 0;
    std::array<bool, std::size_t(Button::Count)> held{};
    [[nodiscard]] bool operator[](Button which) const { return held[std::size_t(which)]; }
};

struct Config {
    float deadzone = 0.25f;           // radial, both sticks
    float cursor_speed = 650.f;       // game pixels per second at full tilt
    float run_above = 0.85f;          // stick tilt (past the dead zone, 0..1): at or above it the hero runs...
    float walk_below = 0.6f;          // ...and under this walks again
    float trigger_press = 0.5f, trigger_release = 0.3f;
    float walk_reach = 6.f;           // tiles ahead a held stick walks towards
};

struct Actions {
    float cursor_dx = 0, cursor_dy = 0;            // pixels to move the cursor this frame
    bool  click_down = false, click_press = false, click_release = false;   // the left button
    bool  rclick_press = false;
    bool  recentre_cursor = false;
    bool  move_active = false;                     // the left stick walks the hero...
    float move_x = 0, move_y = 0;                  // ...towards this screen direction (unit length)
    bool  run = false;                             // at full tilt
    std::vector<Key> keys;
};

// A screen direction as a world one: the inverse of the isometric projection
// the cursor's click uses (Town::input), normalised, times `reach` tiles.
struct WorldStep { float x = 0, y = 0; };
[[nodiscard]] inline WorldStep world_step(float screen_x, float screen_y, int iso_w, int iso_h, float reach) {
    const float iso_u = screen_x / (float(iso_w) / 2), iso_v = screen_y / (float(iso_h) / 2);
    const float x = (iso_u + iso_v) / 2, y = (iso_v - iso_u) / 2;
    const float length = std::hypot(x, y);
    return length > 0 ? WorldStep{ x / length * reach, y / length * reach } : WorldStep{};
}

class Mapper {
public:
    explicit Mapper(Config config = {}) : config_(config) {}

    // `dt` seconds since the last frame. A pad that went away is a default
    // Sample: every button lets go, so nothing stays held.
    Actions step(const Sample& in, Context context, float dt) {
        Actions out;
        left_trigger_ = hysteresis(in.left_trigger, left_trigger_);
        right_trigger_ = hysteresis(in.right_trigger, right_trigger_);
        auto held = in.held;
        const auto pressed = [&](Button which) { return held[std::size_t(which)] && !prev_[std::size_t(which)]; };
        const bool game = context == Context::Game;

        // The left button: south or the right trigger, anywhere.
        out.click_down = in[Button::South] || right_trigger_;
        out.click_press = out.click_down && !click_prev_;
        out.click_release = !out.click_down && click_prev_;
        click_prev_ = out.click_down;
        if (game) {
            const bool secondary = in[Button::East] || left_trigger_;
            out.rclick_press = secondary && !secondary_prev_;
            secondary_prev_ = secondary;
        } else {
            secondary_prev_ = false;
        }

        // Right stick: the cursor. In the menus the left one moves it too.
        float cursor_x = in.rx, cursor_y = in.ry;
        if (!game) { cursor_x += in.lx; cursor_y += in.ly; }
        const auto [tilt_x, tilt_y, tilt] = dead(cursor_x, cursor_y);
        const float speed = config_.cursor_speed * tilt * tilt;   // gentle near the centre, quick at the edge
        out.cursor_dx = tilt_x * speed * dt;
        out.cursor_dy = tilt_y * speed * dt;
        out.recentre_cursor = pressed(Button::RightStick);

        // Left stick: walking, in the game.
        if (game) {
            const auto [dir_x, dir_y, tilt_move] = dead(in.lx, in.ly);
            out.move_active = tilt_move > 0;
            out.move_x = dir_x; out.move_y = dir_y;
            if (out.move_active) running_ = running_ ? tilt_move >= config_.walk_below : tilt_move >= config_.run_above;
            else running_ = false;
            out.run = running_;
        } else {
            running_ = false;
        }

        const auto key = [&](Button which, Key press) { if (pressed(which)) out.keys.push_back(press); };
        if (game) {
            key(Button::Start, Key::Escape);
            key(Button::North, Key::Inventory);
            key(Button::West, Key::Character);
            key(Button::LeftShoulder, Key::SkillTree);
            key(Button::Back, Key::QuestLog);
            key(Button::RightShoulder, Key::Automap);
            key(Button::LeftStick, Key::ToggleRun);
            key(Button::DpadUp, Key::Belt1);
            key(Button::DpadDown, Key::Belt2);
            key(Button::DpadLeft, Key::Belt3);
            key(Button::DpadRight, Key::Belt4);
        } else {
            key(Button::Start, Key::Return);
            if (context == Context::Menu) key(Button::East, Key::Escape);
            key(Button::DpadUp, Key::Up);
            key(Button::DpadDown, Key::Down);
            key(Button::DpadLeft, Key::Left);
            key(Button::DpadRight, Key::Right);
        }
        prev_ = held;
        return out;
    }

private:
    struct Tilt { float x, y, magnitude; };
    // The stick past the radial dead zone: its direction and a 0..1 magnitude.
    Tilt dead(float x, float y) const {
        const float length = std::hypot(x, y);
        if (length <= config_.deadzone) return {};
        const float scaled = std::min((length - config_.deadzone) / (1.f - config_.deadzone), 1.f);
        return { x / length, y / length, scaled };
    }
    bool hysteresis(float value, bool down) const { return down ? value > config_.trigger_release : value >= config_.trigger_press; }

    Config config_;
    std::array<bool, std::size_t(Button::Count)> prev_{};
    bool click_prev_ = false, secondary_prev_ = false, left_trigger_ = false, right_trigger_ = false, running_ = false;
};

}  // namespace d2d::client::pad
