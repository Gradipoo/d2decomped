// SPDX-License-Identifier: GPL-3.0-or-later
// Act 3's quests — game.exe 1.14d, quests a3q0..a3q6.cpp (records 0xe..0x14,
// quest words 0x10..0x16; docs/research/re/quests-act3.md). Each holds one
// game's record; the player's bits are the QuestBits of their difficulty.
#pragma once

#include <monster_ids.hpp>
#include <quests.hpp>

#include <vector>

namespace d2d::rules {

// Act 3's opening (a3q0.cpp, record 0xe, word 0x10): Hratli greets (465;
// a sorceress hears 466, FUN_005b6f20: the player's class 1) with his "!"
// (FUN_005b6fa0) till bit 0; hearing it sets bit 0 and the game's bit 13
// (LAB_005b6ed0), as a join with bit 0 does (LAB_005b6fd0).
struct HratliGreeting {
    static constexpr int kQuest = 0x10, kHratli = monster_ids::kHratli, kGreeting = 465, kSorceress = 466;
    static void join(const QuestBits& quest_bits, QuestBits& game_bits) {
        if (qbit(quest_bits, kQuest, 0)) qset(game_bits, kQuest, 13);
    }
    [[nodiscard]] static std::vector<QuestMsg> talk(const QuestBits& quest_bits, int npc, int player_class) {
        if (npc != kHratli || qbit(quest_bits, kQuest, 0)) return {};
        return { { player_class == 1 ? kSorceress : kGreeting, true } };
    }
    [[nodiscard]] static bool alert(const QuestBits& quest_bits, int npc) { return npc == kHratli && !qbit(quest_bits, kQuest, 0); }
    static void said(QuestBits& quest_bits, QuestBits& game_bits, int npc, int string) {
        if (npc != kHratli || (string != kGreeting && string != kSorceress)) return;
        qset(quest_bits, kQuest, 0);
        qset(game_bits, kQuest, 13);
    }
};

// Lam Esen's Tome (a3q1.cpp, record 0xf, word 0x11): Alkor (254) gives it
// out once someone reaches Lower Kurast (79: state 0 -> 1, LAB_005b7620).
// The tome ("bbb ") lies on its stand in the Ruined Temple (object 193,
// InitFn 23 / OperateFn 28). Bits: 0 done, 1 reward due, 2 told, 3 under
// way, 13 done here, 14 done by others, 15 closed. The reward: 5 stat
// points (FUN_006272b0 stat 4) the moment Alkor takes it.
// N-player gap: one player: the party's bits on hand-in (FUN_005b74e0 /
// 005b75d0: everyone in act 3), the holders' count is this player's.
struct LamEsenQuest {
    static constexpr int kQuest = 0x11, kAlkor = monster_ids::kAlkor, kInit = 549, kSuccess = 564;
    static constexpr int kLowerKurast = 79, kDocks = 75, kStatPoints = 5;
    static constexpr const char* kTome = "bbb";
    bool open = true;              // +9
    int state = 0;                 // +0xc: 0 not yet, 1 given out, 2 told, 3 under way, 4 tome carried, 5 done
    bool first = true;             // data+0: the first hand-in pays everyone in act 3
    bool stand_taken = false;      // data+0x18 2: the stand's mode (InitFn 23)
    bool held = false;             // data+2: a tome was taken
    int holders = 0;               // data+8: tomes carried
    bool flourish = false;         // data+0xc: Alkor's thanks plays on the talk's close (LAB_005b76e0)
    bool rewarded = false;         // in the record's +0x1c list (FUN_005452c0): handed in this game

    // The player joins (FUN_005b7c10).
    void join(QuestBits& quest_bits, QuestBits& game_bits, bool has_tome) {
        if (qbit(quest_bits, kQuest, 0)) { qset(game_bits, kQuest, 13); return; }
        if (has_tome) { state = 4; ++holders; stand_taken = true; held = true; return; }
        if (qbit(quest_bits, kQuest, 3)) state = 3;
        else if (qbit(quest_bits, kQuest, 2)) state = 2;
    }
    // A level change (LAB_005b7620): into Lower Kurast opens it; out of the
    // docks, told -> under way, the told bit for those not done.
    void enter(QuestBits& quest_bits, int from, int into) {
        if (into == kLowerKurast && open && state == 0) state = 1;
        if (from == kDocks && state == 2 && !qbit(quest_bits, kQuest, 0)) { state = 3; told(quest_bits); }
    }
    // FUN_005b73a0: told, while it's told or under way.
    void told(QuestBits& quest_bits) const {
        if (!qbit(quest_bits, kQuest, 0) && !qbit(quest_bits, kQuest, 1) && (state == 2 || state == 3)) qset(quest_bits, kQuest, 2);
    }
    // The tome taken from its stand (OperateFn 28, FUN_005b7a60): none for
    // one done (the "done" flourish, unit event 0x23), else it drops there,
    // the stand to mode 2; under way unless it was. Returns whether it drops.
    bool take(const QuestBits& quest_bits) {
        if (!open || stand_taken || qbit(quest_bits, kQuest, 0)) return false;
        stand_taken = held = true;
        ++holders;
        if (state != 3) state = 3;
        return true;
    }
    // The tome picked up (FUN_005b7970) / dropped (FUN_005b7a00).
    void picked_up() { if (open) state = 4; }
    void dropped() { if (open) state = 3; }
    // What `npc` says (FUN_005b7400; blocks 0x73ecf0, state -> 0x73f188).
    struct E { int npc, string; bool greet; };
    static inline const std::vector<E> kBlocks[5] = {
        { { 254, 549, true } },
        { { 254, 550, false }, { 255, 551, false }, { 264, 552, false }, { 252, 553, false }, { 253, 554, false }, { 245, 555, false }, { 297, 556, false } },
        { { 254, 557, false }, { 255, 558, false }, { 264, 559, false }, { 252, 560, false }, { 253, 561, false }, { 245, 562, false }, { 297, 563, false } },
        { { 254, 564, true }, { 255, 565, false }, { 264, 566, false }, { 252, 567, false }, { 253, 568, false }, { 245, 569, false }, { 297, 570, false } },
        { { 245, 569, false }, { 297, 570, false } },
    };
    [[nodiscard]] std::vector<QuestMsg> talk(const QuestBits& quest_bits, int npc, bool has_tome) const {
        const bool done = qbit(quest_bits, kQuest, 0);
        if (done && !qbit(quest_bits, kQuest, 13)) return {};
        int block = -1;
        if (has_tome) block = 3;
        else if (done) block = rewarded ? 4 : -1;
        else if (open && state >= 1 && state <= 3) block = state - 1;
        std::vector<QuestMsg> out;
        if (block >= 0)
            for (const auto& entry : kBlocks[block]) if (entry.npc == npc) out.push_back({ entry.string, entry.greet });
        return out;
    }
    // Alkor's "!" (FUN_005b72b0): given out and not done or closed, or the tome carried.
    [[nodiscard]] bool alert(const QuestBits& quest_bits, int npc, bool has_tome) const {
        if (npc != kAlkor) return false;
        return (state == 1 && !qbit(quest_bits, kQuest, 0) && !qbit(quest_bits, kQuest, 15)) || has_tome;
    }
    // The player heard `string` from `npc` (FUN_005b7740). Alkor's thanks
    // with the tome: it's taken (`gives_tome`), and the first time everyone in
    // act 3 not done gets bits 13, 0, 1, the reward (bit 1 -> 5 stat points),
    // others bit 14; done here: the game's bit 13, state 5.
    struct Heard { bool takes_tome = false; int stat_points = 0; };
    Heard said(QuestBits& quest_bits, QuestBits& game_bits, int npc, int string, bool has_tome, bool in_act3) {
        if (npc != kAlkor || qbit(quest_bits, kQuest, 0)) return {};
        if (string == kInit) { state = 2; told(quest_bits); return {}; }
        if (string != kSuccess) return {};
        Heard out;
        if (has_tome) { out.takes_tome = true; flourish = true; rewarded = true; if (holders > 0) --holders; }
        if (first) {
            if (in_act3 && !qbit(quest_bits, kQuest, 1)) for (const int bit : { 13, 0, 1 }) qset(quest_bits, kQuest, bit);
            first = false;
            if (qbit(quest_bits, kQuest, 1)) { out.stat_points = kStatPoints; qset(quest_bits, kQuest, 1, false); }
            if (!qbit(quest_bits, kQuest, 0)) qset(quest_bits, kQuest, 14);
        }
        if (qbit(quest_bits, kQuest, 13) && open) { qset(game_bits, kQuest, 13); state = 5; }
        return out;
    }
    // Its log (FUN_005b7d70): hidden till Act 2's last (word 0xf bit 0);
    // done 0xb (0xd here); the tome carried 2; under way 1, lost (taken, none
    // carried) 8 in single player (9 else); found 0xc (1 in act 3 with one held).
    [[nodiscard]] int log_state(const QuestBits& quest_bits, bool has_tome, bool single_player, bool in_act3) const {
        if (!qbit(quest_bits, 0xf, 0)) return 0;
        if (qbit(quest_bits, kQuest, 0)) return qbit(quest_bits, kQuest, 13) ? 0xd : 0xb;
        if (has_tome) return 2;
        if (!open) return 0;
        if (state < 4) return held && holders == 0 ? (single_player ? 8 : 9) : 1;
        return in_act3 && holders != 0 ? 1 : 0xc;
    }
};

// The Golden Bird (a3q4.cpp, record 0x12, word 0x14). The first unique or
// superunique placed in act 3 (FUN_005a09e0 -> FUN_00544e80: not an NPC,
// MonStats +0xd & 0x40, nor class 0x197) while the record's armed (data+1,
// +0xc at init) carries the quest (FUN_005bac70); its death (FUN_005bab60)
// drops the jade figurine ("j34 ") unless its killer's done. Cain hears of it
// (527: bit 2), Meshif (264) swaps it for the golden bird ("g34 ", 529), Cain
// on the bird (531: bit 4), Alkor takes the bird (534: bit 1) and brews the
// Potion of Life ("xyz ", 538: bits 0, 13, 5; 2..11 cleared, FUN_0065c3e0):
// drunk with bit 5, +20 max life (0x55e44e: stat 7 0x1400) and bit 5 clears.
// N-player gap: one player: the carried count and the rewarded list.
struct GoldenBirdQuest {
    static constexpr int kQuest = 0x14, kCain = monster_ids::kCainAct3, kAsheara = monster_ids::kAsheara, kMeshif = monster_ids::kMeshifAct3,
                         kAlkor = monster_ids::kAlkor;
    static constexpr int kCainFigurine = 527, kMeshifSwap = 529, kCainBird = 531, kAlkorTakes = 534, kAlkorPotion = 538;
    static constexpr const char *kFigurine = "j34", *kBird = "g34", *kPotion = "xyz";
    static constexpr int kLifeBonus = 20;
    bool open = true;              // +9
    int state = 0;                 // +0xc: 1 figurine, 2 Cain told, 3 the bird, 4 Alkor has it, 5 done
    int log = 0;                   // +0xb
    bool armed = true;             // data+1: the next boss made in act 3 carries it
    bool due = true;               // data+0xc: the figurine's still to drop
    int bound = -1;                // data+4: its carrier's unit id
    bool flourished = false;       // bit 6's flourish on the figurine's first pickup
    bool rewarded = false;         // +0x1c: given the potion this game

    // The player joins (FUN_005ba870).
    void join(const QuestBits& quest_bits, bool has_bird, bool has_figurine) {
        if (qbit(quest_bits, kQuest, 0) || qbit(quest_bits, kQuest, 1)) { open = false; due = armed = false; return; }
        if (has_bird) { due = armed = false; state = 3; log = qbit(quest_bits, kQuest, 4) ? 4 : 3; return; }
        if (has_figurine) { due = armed = false; state = log = qbit(quest_bits, kQuest, 2) ? 2 : 1; }
    }
    // A boss made in act 3 (FUN_00544e80 -> FUN_005bac70).
    void made(int unit_id) {
        if (!open || !due || !armed) return;
        bound = unit_id;
        armed = false;
    }
    // Its carrier died (FUN_005bab60): the figurine drops unless the killer's done.
    bool killed(const QuestBits& quest_bits, int unit_id) {
        if (!open || !due || unit_id != bound) return false;
        bound = -1;
        if (qbit(quest_bits, kQuest, 0)) return false;
        due = false;
        if (state == 0) state = 1;
        return true;
    }
    // A quest item picked up (FUN_005ba6a0): the bird, state 3; the figurine,
    // state 1 and bit 6 (the flourish, unit event 0x23, the first time).
    // Returns whether to play the flourish.
    bool picked_up(QuestBits& quest_bits, bool bird) {
        if (qbit(quest_bits, kQuest, 0) || qbit(quest_bits, kQuest, 1)) return false;
        state = bird ? 3 : 1;
        if (bird || qbit(quest_bits, kQuest, 6)) return false;
        qset(quest_bits, kQuest, 6);
        return true;
    }
    // What `npc` says (FUN_005b9ed0; blocks 0x7401d0).
    struct E { int npc, string; bool greet; };
    static inline const std::vector<E> kBlocks[8] = {
        { { 245, 527, true }, { 252, 528, false } },
        { { 264, 529, true } },
        { { 245, 531, false }, { 254, 534, true }, { 297, 530, false }, { 253, 532, false }, { 252, 533, false } },
        { { 245, 531, true }, { 253, 532, false }, { 252, 533, false } },
        { { 254, 534, false }, { 255, 535, false }, { 253, 536, false }, { 297, 537, false } },
        { { 254, 538, true }, { 264, 539, false }, { 245, 540, false }, { 255, 541, false } },
        { { 254, 538, false }, { 264, 539, false }, { 245, 540, false }, { 255, 541, false }, { 297, 542, false } },
        { { 245, 527, false }, { 252, 528, false } },
    };
    [[nodiscard]] std::vector<QuestMsg> talk(const QuestBits& quest_bits, int npc, bool has_figurine, bool has_bird) const {
        int block = -1;
        if (qbit(quest_bits, kQuest, 0)) block = rewarded ? 6 : -1;
        else if (qbit(quest_bits, kQuest, 1)) block = 5;
        else if (has_bird) block = npc == kCain && !qbit(quest_bits, kQuest, 4) ? 3 : 2;
        else if (has_figurine) {
            const bool told = qbit(quest_bits, kQuest, 2), asks = npc == kCain || npc == kAsheara;
            block = asks ? (told ? 7 : 0) : 1;
        }
        std::vector<QuestMsg> out;
        if (block >= 0)
            for (const auto& entry : kBlocks[block]) if (entry.npc == npc) out.push_back({ entry.string, entry.greet });
        return out;
    }
    // The "!" (FUN_005b9d90).
    [[nodiscard]] static bool alert(const QuestBits& quest_bits, int npc, bool has_figurine, bool has_bird) {
        if (qbit(quest_bits, kQuest, 0) || qbit(quest_bits, kQuest, 1)) return false;
        if (npc == kAlkor) return has_bird;
        if (npc == kMeshif) return has_figurine;
        if (npc == kCain) return (has_figurine && !qbit(quest_bits, kQuest, 2)) || (has_bird && !qbit(quest_bits, kQuest, 4));
        return false;
    }
    // The player heard `string` from `npc` (FUN_005ba320): what changes hands.
    struct Heard { bool takes_figurine = false, gives_bird = false, takes_bird = false, gives_potion = false; };
    Heard said(QuestBits& quest_bits, QuestBits& game_bits, int npc, int string, bool has_figurine, bool has_bird) {
        Heard out;
        if (qbit(quest_bits, kQuest, 0)) return out;
        if (npc == kCain && string == kCainFigurine) { qset(quest_bits, kQuest, 2); if (state == 1) state = 2; }
        if (npc == kCain && string == kCainBird) qset(quest_bits, kQuest, 4);
        if (npc == kMeshif && string == kMeshifSwap && has_figurine) { out.takes_figurine = out.gives_bird = true; state = 3; }
        if (npc == kAlkor && string == kAlkorTakes && has_bird && !qbit(quest_bits, kQuest, 1)) {
            out.takes_bird = true;
            if (open && state != 4) state = 4;
            qset(quest_bits, kQuest, 1);
        }
        if (npc == kAlkor && string == kAlkorPotion && qbit(quest_bits, kQuest, 1)) {
            qset(quest_bits, kQuest, 1, false);
            qset(quest_bits, kQuest, 0);
            qset(quest_bits, kQuest, 13);
            for (int bit = 2; bit < 12; ++bit) qset(quest_bits, kQuest, bit, false);
            qset(quest_bits, kQuest, 5);
            if (open) state = 5;
            out.gives_potion = rewarded = true;
            qset(game_bits, kQuest, 13);
        }
        return out;
    }
    // The Potion of Life drunk (0x55e44e): bit 5 clears, +20 max life.
    static bool drink(QuestBits& quest_bits) {
        if (!qbit(quest_bits, kQuest, 5)) return false;
        qset(quest_bits, kQuest, 5, false);
        return true;
    }
    // Its log (FUN_005ba170).
    [[nodiscard]] int log_state(const QuestBits& quest_bits, bool has_figurine, bool has_bird, bool single_player) const {
        if (!qbit(quest_bits, 0xf, 0)) return 0;
        if (qbit(quest_bits, kQuest, 1)) return 5;
        if (qbit(quest_bits, kQuest, 0)) return qbit(quest_bits, kQuest, 13) ? 0xd : 0xb;
        if (has_bird) return qbit(quest_bits, kQuest, 4) ? 4 : 3;
        if (has_figurine) return qbit(quest_bits, kQuest, 2) ? 2 : 1;
        if (!open) return 0;
        return log > 5 ? (single_player ? 6 : 7) : log;
    }
};

}  // namespace d2d::rules
