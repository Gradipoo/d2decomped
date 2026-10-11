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

## Act 2's opening (a2q0.cpp, record 7, quest 8)

`FUN_00598810`: +0xa = 1, state 0, no log (+0xd 0), blocks `0x738fc8`
(block 0: Jerhyn 201 string 253, kind 0; block 1: Cain 244 string 125).

- +0xa0 `FUN_005986b0`: Jerhyn without quest 8 bit 0 → block 0; Cain with
  quest 4 bit 14 (missed the rescue), not yet told → block 1.
- +0xcc `LAB_00598640`: 253 heard from Jerhyn → bit 0 and the game's word 8
  bit 13 (`FUN_00544720`): that's what moves Jerhyn into the palace
  (a2q4's objects read the game's flags). 125 from Cain → told.
- +0xd4 `LAB_005987d0`: a join with bit 0 sets the game's bit 13.
- +0xec `FUN_00598780`: Jerhyn's "!" while bit 0 is clear.

d2d: `rules::ActTwoOpening`; the game's flags are `World::game_bits`.
Ponytail: Cain's 125 (and `FUN_005940a0`) isn't here.

## Radament's Lair (a2q1.cpp, record 8, quest 9)

`FUN_00599340`: +0xa = 1, **state 1** (given out from the start), +0xd 4,
+0x10 = 13 (the Seven Tombs' record is next on the chain), blocks
`0x739218`, state → block `0x7398fc` (−1, 0, 1, 2, 3, 4). NPCs: Atma 176,
Warriv 175, Drognan 177, Fara 178, Greiz 198, Elzix 199, Geglash 200,
Lysander 202, Meshif 210, Cain 244. Radament is MonStats 229.

Bits: 0 done, 1 reward due, 2 Atma told, 3 / 4 under way (log 1 / 2),
5 the Book of Skill due, 13 took part, 14 killed by someone else, 15 closed.

| field | function | when |
|---|---|---|
| +0xa0 | `FUN_00598c40` | messages: bit 1 → block 3; rewarded → 4; else without bit 0, state ≠ 0 and (state < 4 or bit 13) → state − 1 |
| +0xa8 | `LAB_00598990` | Atma's talk closes after 304: log 1, one shot |
| +0xac | `LAB_00599130` | leaving level 40: off the rewarded list; state 2 (no bit 0 / 1) → 3, mark |
| +0xc0 | `FUN_00599020` | Radament dies (below) |
| +0xcc | `FUN_00598a70` | heard from Atma: 304 → data+8, state 2, mark; 334 with bit 1 → done (below) |
| +0xd4 | `FUN_00599230` | join (below) |
| +0xec | `FUN_00598910` | Atma's "!": no bit 0, and (open, state 1, no bit 15) or bit 1 |
| +0xf0 | `FUN_005991c0` | chain: stops while open and not done, else record 13's +0xf0 |

Mark (`LAB_005989e0`): no bit 0 / 1; state 2 → bit 2, state 3 → bit 3 if
log 1, else bit 4.

**Radament dies** (`FUN_00599020`, open): state 4, the game's word 9 bit 13,
+0xa8 = 0. Per player: `FUN_00598e20` (the player's room is the killer's
or next to it, no bit 0 / 1 → bits 13, 1 and 5; the party too,
`FUN_00598d40`); `LAB_00598dc0` (no bit 0 / 1 → bit 14, the notice);
`LAB_00598fa0` (bit 5 and no "ass" carried → a book to drop; bit 13 →
voice 0x32, the class's act2_complete_radament). A 12-tick timer (once,
data+10) → `LAB_00598f70`: state 4 → log 3. The books drop at him
(`FUN_00559a30`, code "ass" at +0xb8).

**Atma's 334 with bit 1:** bit 13 → (state ≠ 5: log 0xd, state 5, the
chain; closed: the game's bit 13); without it, a closed-with-reward-due
join's data+0x10 runs record 13's chain. Then bit 0 set, bit 1 cleared,
onto the rewarded list. No reward here: the book was it.

**Join** (`FUN_00599230`): bit 0 → if bit 5 and no book carried, bits 0, 1,
15, 5 clear (it's to be done again in a later game); the game's bit 13,
closed, state 0. Bit 15 → data+0x10 = bit 1, closed. Else bit 4 → log 2,
state 3; bit 3 → state 3, log 1; bit 2 → state 2, log 1.

**The Book of Skill** (item use, `0x55cd31` can-use and `0x55e3d0` use):
only with quest 9 bit 5; it clears bit 5, adds a skill point (stat 5,
`FUN_006272b0`) and the book is used up.

**The log** (client, `FUN_004a1950`): Act 2's records are 6..11 of
`0x7237a4` (quests 9..14); only the Seven Tombs (record 11, `0x723a64`)
has its own case.

d2d: `rules::RadamentQuest`, `World::radament_died`, the book in
`World::apply` (UseItem). Checked live (devctl, 2026-10-11): Atma 304 →
state 2 / log 1 / bit 2; out of town → state 3; the kill → bits
1 2 3 5 13, log 3, the book dropped; read → +1 skill point, bit 5 clear;
Atma 334 → state 5, log 0xd; the log's Act 2 tab says "Quest completed".
Ponytails: one player (the lists are bools, no party), "near" is the
player in Sewers level 3, the chain on to the Seven Tombs waits for its
record.
