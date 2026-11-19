// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gamepad_map.hpp"
#include "touch_input.hpp"

#include <cmath>
#include <cstdint>
#include <span>

namespace d2d::client::touch {

// A held gesture activates only after a short tap has had time to finish.
struct HoldGesture {
    std::int64_t finger = -1;
    std::uint32_t started = 0;
    bool activated = false;

    void begin(std::int64_t owner, std::uint32_t now) { finger = owner; started = now; activated = false; }
    bool update(std::uint32_t now, std::uint32_t delay = 300) {
        if (finger != -1 && now - started >= delay) activated = true;
        return activated;
    }
    void clear() { finger = -1; activated = false; }
};

// Screen-space limits of the two-to-one drag projection. Allow a little
// overshoot at its edge before interpreting a drag as cancellation.
inline constexpr float kSkillAimX = 370.f, kSkillAimUp = 250.f, kSkillAimDown = 210.f;
inline bool beyond_skill_aim(float dx, float dy) {
    constexpr float slop = 16.f;
    return std::abs(dx) > kSkillAimX / 2 + slop
        || dy < -kSkillAimUp / 2 - slop || dy > kSkillAimDown / 2 + slop;
}

struct SkillHold : HoldGesture {
    int target = -1;
    float aim_x = 0, aim_y = 0;
    bool cancelled = false, chases = false, at_caster = false, sent = false;
    int sent_to = -1;
    std::uint32_t sent_at = 0;

    void begin(std::int64_t owner, std::uint32_t now, int unit, float direction_x, float direction_y, bool centered = false) {
        clear();
        HoldGesture::begin(owner, now);
        target = unit;
        steer(direction_x, direction_y);
        at_caster = centered;
    }
    void steer(float direction_x, float direction_y) {
        aim_x = direction_x;
        aim_y = direction_y;
    }
    void cancel() { cancelled = true; }
    [[nodiscard]] bool release_casts() const { return !activated && !cancelled; }
    bool active(std::uint32_t now) { return !cancelled && update(now); }
    void resolve(Target& preview, float x, float y, std::span<const pad::Candidate> foes, const pad::Aim& aim) {
        if (at_caster) {
            preview = caster_target(preview.skill, x, y);
            target = -1;
            return;
        }
        const int index = pad::pick_target(x, y, aim_x, aim_y, foes, target, aim);
        target = index < 0 ? -1 : foes[std::size_t(index)].id;
        preview.unit = target;
        preview.x = index < 0 ? x + aim_x : foes[std::size_t(index)].x;
        preview.y = index < 0 ? y + aim_y : foes[std::size_t(index)].y;
        preview.valid = true;
    }
    bool dispatch(std::uint32_t now, bool ready, std::uint32_t interval) {
        if (!active(now) || !ready || (sent && now - sent_at < interval)) return false;
        sent = true;
        sent_to = target;
        sent_at = now;
        return true;
    }
    void clear() { *this = {}; }
};

} // namespace d2d::client::touch
