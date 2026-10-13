# Act 3 quests — game.exe 1.14d

Conventions, flag helpers and the record layout are in `quests.md` /
`quests-act1.md`.

## The records

The quest init table at `0x731510` (24-byte rows `{init fn, act, 0x64,
flag, record id, quest word}`), Act 3's rows (0x11..0x17):

| record id | quest word | init fn | file |
|---|---|---|---|
| 0xe | 0x10 | `FUN_005b7010` | a3q0.cpp (the act's opening) |
| 0xf | 0x11 | `FUN_005b7e90` | a3q1.cpp |
| 0x10 | 0x12 | `FUN_005bda30` | a3q2.cpp (Khalim's Will: word 0x12 bit 0 breaks the orb at a join) |
| 0x11 | 0x13 | `FUN_005b9850` | a3q3.cpp |
| 0x12 | 0x14 | `FUN_005baa70` | a3q4.cpp |
| 0x13 | 0x15 | `FUN_005bbeb0` | a3q5.cpp (the compelling orb binds it; the zealots spare its bit 0, monster-ai.md) |
| 0x14 | 0x16 | `FUN_005bcdc0` | a3q6.cpp (The Guardian: Mephisto binds it) |

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
