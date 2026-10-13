# Act 3 quests — game.exe 1.14d

Conventions, flag helpers and the record layout are in `quests.md` /
`quests-act1.md`.

## The records

The quest init table at `0x731510` (24-byte rows `{init fn, act, 0x64,
flag, record id, quest word}`), Act 3's rows (0x11..0x17):

| record id | quest word | init fn | file |
|---|---|---|---|
| 0xe | 0x10 | `FUN_005b7010` | a3q0.cpp (the act's opening: Hratli) |
| 0xf | 0x11 | `FUN_005b7e90` | a3q1.cpp (Lam Esen's Tome: Alkor) |
| 0x10 | 0x12 | `FUN_005bda30` | a3q2.cpp (Khalim's Will: word 0x12 bit 0 breaks the orb at a join) |
| 0x11 | 0x13 | `FUN_005b9850` | a3q3.cpp (the Blade of the Old Religion: Hratli) |
| 0x12 | 0x14 | `FUN_005baa70` | a3q4.cpp (the Golden Bird: Cain, Meshif, Alkor) |
| 0x13 | 0x15 | `FUN_005bbeb0` | a3q5.cpp (the compelling orb binds it; the zealots spare its bit 0, monster-ai.md) |
| 0x14 | 0x16 | `FUN_005bcdc0` | a3q6.cpp (The Guardian: Mephisto binds it) |

Message tables: `tools/emu/quest_msgs.py <record +0xdc>` dumps them (0xc4-byte
blocks, quests.md). NPCs: 245 Cain, 252 Asheara, 253 Hratli, 254 Alkor, 255
Ormus, 264 Meshif, 297 Natalya.

## The opening (a3q0.cpp)

Hratli (253) greets with 465, a sorceress (the unit's class 1) with 466
(`FUN_005b6f20`), his "!" (`FUN_005b6fa0`) till word 0x10 bit 0. Hearing
either (`FUN_005b6ed0`) sets bit 0 and the game's bit 13; a join with bit 0
the game's (`FUN_005b6fd0`). d2d: `rules::HratliGreeting`.

## Lam Esen's Tome (a3q1.cpp)

State (+0xc, `FUN_00544350`'s EDX): 0 not yet, 1 given out (entering Lower
Kurast, 79), 2 Alkor told (549, 0x225), 3 under way (leaving the docks; the
tome dropped, 0x281; taken off its stand), 4 carried (0x26c), 5 done.
Data: +0 the first hand-in pays everyone (1 at init), +2 a tome was taken,
+8 tomes carried, +0xc Alkor's flourish on the talk's close (`LAB_005b76e0`:
unit event 0x23), +0x18 the stand's mode.

- **The stand** (objects.txt 193 `LamTome`, InitFn 23 `FUN_00544e30`: the
  record's +0x18, 2 when closed; OperateFn 28 `FUN_005b7a60`): one with bit 0
  gets the flourish; else `bbb ` drops there (`FUN_00559a30`), mode 2.
- **Alkor** (254): "!" (`FUN_005b72b0`) given out and not done or closed, or
  the tome carried. Blocks 0x73ecf0 by state (0x73f188: -1, 0, 1, 2, 3, 4);
  block 3 with the tome, 4 once rewarded (`FUN_005452c0`).
- **564 heard** (`FUN_005b7740`, 0x234) with the tome: it goes
  (`FUN_00544160`); the first time each player in act 3 not done gets bits 13,
  0, 1 (`FUN_005b74e0`), bit 1 pays 5 stat points (`FUN_006272b0` stat 4) and
  clears (`FUN_005b7580`), the rest bit 14 (`FUN_005b75d0`); done here: the
  game's bit 13, state 5, the chain (+0xf0).
- **Log** (`FUN_005b7d70`): hidden till word 0xf bit 0; done 0xb / 0xd; the
  tome carried 2; under way 1, lost 8 (9 in a multiplayer game, game +0x6a
  != 3); carried by someone 0xc (1 in act 3).

d2d: `rules::LamEsenQuest`; the pickup / drop are polled from what's
carried. `tests/quests_d2d.py` plays it.

## The Golden Bird (a3q4.cpp)

State (EDX): 1 the figurine (dropped, 0x36f; picked up, 0x26e), 2 Cain told
(527, 0x1de), 3 the bird (Meshif's 529, 0x236; picked up, 0x263), 4 Alkor
took it (534, 0x1fd), 5 done (538, 0x218). Data: +1 armed, +0xc the
figurine due (both 1 at init), +4 its carrier, +0x10 items carried.

- **The carrier.** `FUN_005a09e0` (placing a unique, superunique or preset
  special) calls `FUN_00544e80`: in act 3, record 0x12 open, the monster not
  class 0x197 and not MonStats +0xd & 0x40, → `FUN_005bac70` while armed and
  due: the quest bound to it (`FUN_005436b0(0x12)`), +0xc0 = `FUN_005bab60`.
- **Its death** (`FUN_005bab60`): unless the killer has bit 0, `j34 ` drops
  (`FUN_00559a30`), due off, state 1.
- **Talk** (`FUN_005b9ed0`, blocks 0x7401d0): done → block 6 once rewarded;
  bit 1 → 5; the bird: Cain without bit 4 → 3, else 2; the figurine: Cain or
  Asheara → 0 (7 once bit 2), the rest 1.
- **Heard** (`FUN_005ba320`): Cain 527 bit 2; Cain 531 bit 4; Meshif 529 with
  the figurine: it goes, `g34 ` made (`FUN_005466b0`); Alkor 534 with the bird:
  it goes, bit 1; Alkor 538 with bit 1: bit 1 off, bits 0, 13, bits 2..11 off
  (`FUN_0065c3e0`), bit 5, `xyz ` made, the game's bit 13.
- **"!"** (`FUN_005b9d90`): not done and no bit 1: Alkor with the bird,
  Meshif with the figurine, Cain with the figurine and no bit 2 or the bird
  and no bit 4.
- **The Potion of Life** (0x55e44e): with bit 5, bit 5 off and +20 max life
  (`FUN_006272b0` stat 7, 0x1400); else event 0x13. Picking it up wants bit 5
  (`FUN_0055cc90`).

d2d: `rules::GoldenBirdQuest`; the carrier is the first boss the World sees
on an act 3 level; pickups polled. `tests/quests_d2d.py` plays it.

## The Guardian (a3q6.cpp)

Record callbacks (`FUN_005bcdc0`): +0xc0 a monster dies `FUN_005bc8b0`,
+0xa0 `FUN_005bc270`, +0xcc `FUN_005bc5a0`, +0xac entering a level
`FUN_005bc3c0`, +0xec `FUN_005bbfe0`. Data (0x34 bytes): +0 the record's
opening sent, +1 / +4 the hellgate made / its id, +2 / +8 the bridge
made / its id, +0xc the state (2 once Mephisto's dead), +0x10 the bridge's
mode, +0x1c soulstones owed.

- **Mephisto dies** (`FUN_005bc8b0`): the killer, if word 0x16 bit 0 is
  clear, and every player in the Durance of Hate 3 (level 0x66,
  `FUN_005bc190`) without it get bits 0xd, 0 and 0xb (`FUN_005bc140`) and
  progression diff × 4 (5 in LoD) + 3 (`FUN_00538680`, EDX 3: Act 4).
  Each one that hadn't bit 0xb owes a soulstone: the dying unit's drop
  code becomes `mss ` and `FUN_00559a30` drops that many. The state goes
  to 2; the hellgate goes to mode 1, its event 1 at FrameCnt1 frames on.
- **The hellgate** (objects.txt 342, `1y`, InitFn 44 `FUN_005bcbf0`):
  takes the record's state as its mode (0 till he's dead; level 104:
  mode 2). Its parts are DC6, not DCC. OperateFn 46 leads to Act 4.
- **The bridge** (341, `xj`, InitFn 45 `FUN_005bcb90`, 11 × 5): takes
  +0x10 as its mode and, short of 2, schedules event 7 20 frames on. Event
  7 (`FUN_005449e0` for class 0x155 → `FUN_005bcac0`), every 24 frames:
  with the state 2 and a player (unit type 0) of its room within 18
  (`FUN_00641530`), mode 0 → 1 (it rises, 28 frames) and the next 1 → 2,
  its footprint lifted (`FUN_00623830`: no HasCollision2). Mode 2 stops it.

d2d: `World::mephisto_died`, `World::mephisto_bridge`; the hellgate's
OperateFn only says Act 4 isn't built. `tests/quests_d2d.py` (map seed 3).
Not traced: the record's other callbacks (the talk, the level entries).
