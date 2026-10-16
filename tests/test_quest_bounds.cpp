// SPDX-License-Identifier: GPL-3.0-or-later
// Act III quest IDs must fit every quest-state representation.
#include <world.hpp>

#include <array>
#include <cassert>
#include <cstdint>

using namespace d2d::game;
using namespace d2d::rules;

static_assert(kQuestLogCount >= 41);
static_assert(kQuestLogCount > GuardianQuest::kQuest);
static_assert(kQuestFlagCount == QuestBits{}.size() / 2);
static_assert(kQuestFlagCount > kRespecQuest);

int main() {
    World world(nullptr, -1, -1);
    Level level{};
    level.id = level_ids::kKurastDocks;
    world.level = &level;

    QuestLogStates log;
    log.fill(0xff);
    world.act3_log(log, QuestBits{});

    for (const int quest : { LamEsenQuest::kQuest, GoldenBirdQuest::kQuest,
                             BladeQuest::kQuest, KhalimQuest::kQuest,
                             BlackenedTempleQuest::kQuest, GuardianQuest::kQuest })
        assert(log[std::size_t(quest)] != 0xff);
}
