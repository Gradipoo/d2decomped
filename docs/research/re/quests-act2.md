# Act 2 quests — game.exe 1.14d

Started 2026-10-10 with what Lut Gholein itself needs. Conventions, flag
helpers and the record layout are in `quests.md` / `quests-act1.md`.

## The records

The quest init table at `0x731510` (24-byte records `{init fn, act, 0x64,
flag, record id, quest word}`), Act 2's rows:

| record id | quest word | init fn | file |
|---|---|---|---|
| 7 | 8 | `FUN_00598810` | act 2's opening (Jerhyn's greeting) |
| 8 | 9 | `FUN_00599340` | a2q1 Radament's Lair |
| 9 | 10 | `FUN_0059eb00` | a2q2 The Horadric Staff |
| 10 | 11 | `FUN_0059eea0` | a2q3 Tainted Sun |
| 11 | 12 | `FUN_0059f2a0` | a2q4 Arcane Sanctuary |
| 12 | 13 | `FUN_0059c280` | a2q5 The Summoner |
| 13 | 14 | `FUN_0059d5b0` | a2q6 The Seven Tombs |
| 0x1a / 0x1b | 0x1e / 0x1f | `FUN_0059e330` / `FUN_0059e530` | (later) |

`FUN_00543640(game, id)` finds a record by id (the list at game
+0x10f4); `FUN_00544760(game, word, bit)` reads the game's own quest
flags (game +0x10f4 +0xc). Record +9 is open in this game, +0xc its state.

## Jerhyn and the palace gate (a2q4.cpp)

Lut Gholein's DS1 holds two invisible objects named "jerhyn" (objects.txt
121 and 122, token `ss`). Their InitFns (table 0x731bc0) run as their
rooms come up; each finds the quest record of type 0xb in the game's list
and calls a2q4's hook:

- **Object 121** (InitFn 18 `FUN_005448b0` → `FUN_0059f380`): unless
  record +0xd (Jerhyn made) is set, when word 8 and word 12 lack bit 13
  and record 13 (the Seven Tombs) is open (+9) with state under 2:
  `FUN_00545340(2, 0x100, spot, 10, 100)` finds room by the object, then
  Jerhyn (MonStats 201) is made there (`FUN_005b2f20(x, y, 0xc9, 1, -1,
  0)`); +0xc = 1, +0x3c his unit id.
- **Object 122** (InitFn 19 → `FUN_0059f440`): unless word 14 has bit 13,
  the palace guard (331, act2guard2, MonAI JarJar) one subtile east
  (`FUN_005b3090(x + 1, y, 0x14b, 1, 0)`); event 7. Then, unless Jerhyn's
  made, he stands in the palace (`FUN_0059ef70`: x − 10 while record 13
  is open and under 2, else x + 15; y − 3) — except while words 8 and 9
  lack bit 13 and record 13 holds (he's outside then).
- `FUN_0059f0c0` (a2q4's level change, +0xac) moves him in on entering
  level 40 once record +0x11 is set (not ported).

d2d: `rules::palace_spots` (quests.hpp, tests/test_quests.cpp); Lut
Gholein builds every candidate (`Npc::palace` 1..4) and
`World::show_palace` shows them as the quest bits stand.
Ponytails: the game's flags are the player's (one player), and record 13's
"under 2" is quest 14 having none of bits 1..11 (the Seven Tombs record
isn't ported); FUN_00545340's spot search is the nearest free spot.
