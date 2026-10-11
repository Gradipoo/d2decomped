// S->C decoders that are more than a field read (net-packets.md).
#pragma once

#include "wire.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace d2d::net::d2gs::s2c {

// 0x27 NPC info, mode 1: the NPC's quest lines for this player
// (FUN_004a1600 -> FUN_00661510): +2 u32 the NPC's id, +6 u8 count (< 8),
// from +8 four bytes a line, u8 kind (0 greets, 2 a topic) and u16 message
// at +10. A lone kind 3 is a number over the NPC: not quest lines.
struct NpcLine { std::uint16_t message = 0; bool greet = false; };
struct NpcLines { std::uint32_t id = 0; std::vector<NpcLine> lines; };
inline auto npc_lines(std::span<const std::uint8_t> packet) -> std::optional<NpcLines> {
    if (packet.size() < 40 || packet[0] != 0x27 || packet[1] != 1 || packet[6] >= 8 || (packet[6] == 1 && packet[8] == 3)) return std::nullopt;
    NpcLines out{ .id = read_u32(packet, 2) };
    for (std::size_t line = 0; line < packet[6]; ++line) out.lines.push_back({ read_u16(packet, 10 + 4 * line), packet[8 + 4 * line] == 0 });
    return out;
}

// The quest log's row (= quest word) of a quest record id, as 0x5d's +1
// names it (FUN_004a1910: the table at 0x723f30, +8 the record, 41 rows;
// 37 is "none"). -1 when no row has it.
inline auto quest_word_of_record(int record) -> int {
    static constexpr std::array<int, 41> kRecordOfWord = { 37, 1, 2, 3, 4, 5, 6, 37, 37, 8, 9, 10, 11, 12, 13, 37, 37, 15, 16, 17, 18,
                                                           19, 20, 37, 37, 22, 23, 24, 37, 37, 37, 37, 37, 37, 30, 31, 32, 33, 34, 35, 36 };
    const auto found = std::ranges::find(kRecordOfWord, record);
    return found == kRecordOfWord.end() ? -1 : int(found - kRecordOfWord.begin());
}

}  // namespace d2d::net::d2gs::s2c
