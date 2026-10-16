# Act 4 quests — game.exe 1.14d

Conventions, flag helpers and the record layout are in `quests.md` /
`quests-act1.md` (callbacks: +0xa0 messages, +0xa8 talk closes, +0xac level
change, +0xb0 item event, +0xc0 a bound unit dies, +0xc4 quest item on
leave, +0xc8 leaves, +0xcc heard, +0xd4 joins, +0xd8, +0xec "!", +0xf0
open / chain). Traced 2026-10-14 from the decompile; not yet ported or
emulated.

## The records

Quest init table `0x731510` (24-byte rows), Act 4's (act 3):

| row | record id | quest word | init fn | file |
|---|---|---|---|---|
| 0x19 | 0x15 | 0x18 | `FUN_005b3780` | (a4q0, no asserts: the act's opening, Tyrael) |
| 0x1a | 0x16 | 0x19 | `FUN_005b42b0` | a4q1.cpp, The Fallen Angel (Izual) |
| 0x1b | 0x17 | 0x1a | `FUN_005b54c0` | a4q2.cpp, Terror's End (Diablo, the seals) |
| 0x1c | 0x18 | 0x1b | `FUN_005b6610` | a4q3.cpp, The Hellforge |

Message tables (+0xdc): 0x73d580, 0x73d708, 0x73dbb8, 0x73e140
(`tools/emu/quest_msgs.py`). Chain (+0x10, record ids): a4q1 → 0x18 (the
Hellforge) → 0x17 (Terror's End). Units (MonStats rows): 0x16f Tyrael (tyrael2), 0xf6
Cain (cain4), 0x196 Izual's ghost, 0x100 Izual, 0xf3 Diablo, 0x199
Hephasto.

**Binding** (`+0xc0` fires only for units bound with `FUN_005436b0`):
`FUN_005b1cf0` by MonStats class — Diablo 0xf3 → 0x17 (unless his MonStats
+.. is 0x2c1), Izual 0x100 → 0x16 (unless 0x2c2), Hephasto 0x199 → 0x18;
`FUN_005a49b0` by superunique — 0x24 Infector of Souls, 0x25 Lord De
Seis, 0x26 Grand Vizier of Chaos → 0x17.

## The opening (a4q0)

Tyrael (0x16f) has 664 (0x298) and "!" (`FUN_005b3750`) till word 0x18
bit 0 (`FUN_005b36d0`: block 0). Hearing 664 (`LAB_005b36a0`) sets bit 0.

## The Fallen Angel (a4q1.cpp, word 0x19)

Data (0x1c bytes): +4 / +8 Izual's spot at death, +0xc told-by-ghost, +0xd
the timer set, +0x10 the ghost due, +0x11 Tyrael's word heard, +0x12 the
ghost spoke, +0x14 the killer.

- **"!"** (`FUN_005b37f0`): Tyrael while not done and (bit 1, or state 1);
  the ghost (0x196) till bit 5.
- **Messages** (`FUN_005b3bb0`): the ghost without bit 5 → block 3; bit 1
  (reward due) → 3; `FUN_005452c0` → 4; else by state from 0x73dba0.
- **Heard** (`FUN_005b39a0`): Tyrael 670 (0x29e): +0x11, state (0xd3),
  unit callback `LAB_005b3860` (bit 2 at state 2; at 3 bit 4 when the ghost
  told, else bit 3). Tyrael 676 (0x2a4) with bit 1: if bit 13 and state ≠ 5:
  state 5, the chain (+0xf0), every player's log (`FUN_005b3940`), game
  quest flag 0xd; then bit 0, bit 1 cleared, bits 2..11 cleared, **2 skill
  points** (`FUN_006272b0(player, 5, 2)`). The ghost 675 (0x2a3): +0x12,
  +0xf, bit 5; the log refreshed.
- **Entering** the fortress (+0xac, `LAB_005b4140`, level 0x67) at state 2,
  not done: state (0x23b) and the bit callbacks.
- **Izual dies** (+0xc0, `FUN_005b4020`): state (0x201), +0xc0 off, the
  killer; in this game: the killer's bits 13 + 1 if not done (and cleared
  2..11), +0x10, his spot (`FUN_00620870`), every player in his room or its
  neighbours (`FUN_00619790`) bits 13 + 1 (`FUN_005b3f20`), the "returns"
  callbacks (`LAB_005b3de0`: bit 13 → sound event `LAB_005b3d70`;
  `LAB_005b3cd0`: not done → bit 14, message 0). Once: the 3-tick timer
  `FUN_005b3e60`: log 4 to everyone; the ghost (0x196) at his spot, mode 8,
  within −1, then 3, then 5 (`FUN_005b2f20`).
- **Izual's first think** (`FUN_005b4390`, from his AI): every player's
  log (`FUN_005b3940`) when +9 and state < 2.
- **Join** (`LAB_005b4220`): not done or closed: bit 4 → state 3, log 2,
  data +0xc; bit 3 → 3 / 1; bit 2 → 2 / 1.

## Terror's End (a4q2.cpp, word 0x1a)

Data (0x4c bytes): +0 Tyrael's word heard, +1 the Diablo timer armed, +2
Diablo due, +3 Diablo dead, +4 / +5 the end steps, +6 the start point found,
+8 its object id, +0xc..+0x10 seals 392..396 opened, +0x13 the sanctuary
cleared, +0x14 Diablo dead (this game), +0x15 the 75 s step done, +0x18
ticks / the death tick (`GetTickCount`), +0x1c the kill count, +0x20 the
player, +0x24 / +0x2c / +0x34 the three seal bosses' spots, +0x3c the
room he died in, +0x44 / +0x45 Tyrael's portal due / made, +0x48 bound
deaths.

- **"!"** (`FUN_005b4450`): Tyrael while not done and state 1; classic
  (game +0x70 = 0) with bits 0 and 7; LoD with bit 0 and not bit 9. Cain
  (0xf6): classic bits 0 + 6; LoD bit 0 and not bit 8.
- **Heard** (`FUN_005b4670`): Tyrael 681 (0x2a9): +0, state (0xe1), bit
  callbacks `LAB_005b4560`; 684 (0x2ac): bit 7 cleared; 20000 (LoD): bit 9
  (`FUN_00543520(0x1a, 9)`) and, once, Tyrael's portal (`FUN_005b45e0`).
  Cain 685 (0x2ad): bit 6 cleared; 20001: bit 8.
- **Talk closes** (`FUN_005b4f90`): Tyrael with +0 → logs; LoD, +0x44 and
  not +0x45 → the portal.
- **The seals** (objects 392..396, InitFn 56 nothing; OperateFn 52 for 393
  and 395 `FUN_005b5630`; 54 / 55 / 56 for 392 / 394 / 396): closed (mode
  0) → mode 1 (event 1 at its anim end), its flag +0xc..+0x10 by id
  (0x188..0x18c). 54 / 55 / 56 first keep a spot (`FUN_00620870` + (−12,
  −52) / (−39, +33) / (+32, +16) into +0x24 / +0x2c / +0x34) and make object
  131 (the flash, `FUN_00555230` type 2) at a free spot (`FUN_00545340`
  mask 0x3f11, sizes 13 / 15 / 14) — its event 7 (`FUN_005449e0` → in
  level 108 `FUN_005b5750`) makes the seal's superunique there
  (`FUN_00545c30`, ids [0x744304]+0xb28.. = 0x24 / 0x25 / 0x26), retried
  10 frames on when it can't.
- **All five open and three bound deaths** (seal op, the start point's
  InitFn 55 `FUN_005b5590`, or a death `FUN_005b52e0`): `FUN_005b5230`
  once (+0x13): every monster in level 108 but Diablo, not dead, not a
  pet, set to mode 0 (dies: `FUN_005a7e60` / `FUN_005a7c20`); and if not
  armed: +2, +1, the 1-tick timer `FUN_005b4be0`.
- **The timer** (`FUN_005b4be0`): with +2, after 10 ticks and the start
  point's object (+8, object 255 "diablo start point"): Diablo (0xf3) at its
  spot, mode 1, within −1 / 5 / 10 (`FUN_005b4b60` → `FUN_005b2f20`), flags
  0x3000000, +0x11; done. With +3 (dead): 75 s (wall clock) → once
  `FUN_0052e2a0`; 90 s → +5: classic every player `FUN_005b4a80` (saved,
  start town 0x67, sound 0x1b8, the end `FUN_0053d710(0x1175d)`); 95 s →
  reset, classic `FUN_00530590` (the game ends).
- **Diablo dies** (+0xc0 `FUN_005b52e0`, class 0xf3): (classic: `FUN_00545760` first)
  +0x14, his room (+0x3c); with a killer, once: +2 0, +3 / +4 / +5 1, +1,
  the tick; the timer. Every player: logs; unit callbacks `FUN_005b5140`
  (in his room or a neighbour, not done: `FUN_005b4d20` — bits 13 and 0,
  bits 2..11 cleared; classic bits 6 / 7, sound 0x250 and the next
  difficulty opened `FUN_00538680(diff)`; +0x1c count), `LAB_005b4df0`,
  `LAB_005b4970` (not done: bit 14), `FUN_005b49c0` (bit 13: sound 0x1a4).
  Rewarded players' counts → `FUN_00545990(0x40)` each.
- **Tyrael's portal** (LoD, `FUN_005b45e0`): object at Tyrael + (5, 0)
  (`FUN_00545340(2, 0x400, .., 12, 100)`, then `FUN_00555230` type 2),
  flags 0x3000000, +0x44 0, +0x45 1. Its operate `FUN_005b5880`: LoD, bit
  0 or 13: in act 4 sets word 0x1c bits 0 and 13, sound 0x55a and the
  waypoint (`FUN_00554190`), then to level 0x6d (109, Harrogath,
  `FUN_0054b830`). d2d has no Act 5: it stops there.
- **Entering** (`LAB_005b4ea0`): the fortress at state 2 → state; the
  Chaos Sanctuary (0x6c) → state, logs.
- **Join** (`FUN_005b5080`): LoD bit 9 without word 0x1c bit 0 → bit 9
  cleared (again on `LAB_005b5030`); bits 4 / 3 / 2 → states as a4q1.

## The Hellforge (a4q3.cpp, word 0x1b)

Data (0x20 bytes): +0 the forge's mode (0 / 2 / 3), +2 Cain's word heard,
+3 / +4 the gem drop on / its mode, +8 rounds left, +0xc hammer hits, +0x10
hammers about, +0x14 gems per round, +0x1c the soulstone given.

- **"!"** (`FUN_005b5ee0`): Cain at state 1, not done or due, +0x1c 0.
- **Messages** (`FUN_005b62d0`): due → 2; `FUN_005452c0` → 3; state 1: 0
  when the player has `mss ` (`FUN_00558110`), else 1 unless +0x1c or log
  > 2; later states 1 likewise.
- **Heard** (`FUN_005b6100`): Cain 678 (0x2a6): +2, state; 679 (0x2a7):
  gives the soulstone (`FUN_005466b0("mss ", 0, 2, 1)`), +2, +0x1c, state
  (< 2); 680 (0x2a8) with bit 1: state 5 and the chain (if bit 13), bit 0,
  bit 1 cleared.
- **Hephasto dies** (+0xc0 `FUN_005b65d0`): drops `hfh ` (the Hellforge
  Hammer, `FUN_00559a30(7)`), +0x10.
- **The forge** (objects 376: InitFn 48 `FUN_005b5a20` mode = +0 (3 with no
  record); OperateFn 49 `FUN_005b5c10`): not done / due: mode 0 with `mss `
  carried → mode 1 (event at its anim end), +0 = 2, the logs, `mss `
  taken; without it the player's unit event; mode 2 with `hfh ` in hand
  (`FUN_0063bef0` on the player's +0x60, item code +0x80): +0xc += 1; the
  third → mode 3, +0 = 3, `hfh ` taken, bits 13 + 1, +4 / +3 = 1, +8 = 4,
  +0x14 = 1, every player in act 4 (`FUN_005b5b00`: not done: `hfh ` /
  `mss ` taken, bits 13 + 1, +0x14 count), state (0x125), event 7 at its
  anim end, the player's `LAB_005b5bb0` (bit 14). 
- **The gems** (event 7, `FUN_005449e0` object 0x178 → `FUN_005b6710`):
  mode 4 once; each round +0x14 drops at the forge (`FUN_00559a30(2)`),
  each `rand(7)` on the quest seed (`FUN_005438b0`) from round +8: 4
  0x73e56c (perfect: gpv gpr gpb gpy gpg gpw skz), 3 / 2 0x73e588
  (flawless), 1 0x73e5a4 (plain); a dropped round → +8 − 1 and the next 20
  frames on; the last, LoD: a rune `rand(11)` from 0x73e114 (r01–r11) /
  0x73e514 (nightmare r12–r22) / 0x73e540 (hell r15–r25).
- **Item event** (+0xb0 `LAB_005b5a90`): `mss ` picked up → state 1;
  +0xc4 / +0xd8 count hammers about.
- **Join** (`FUN_005b64e0`): `mss ` carried → state 1; bit 5 → state 2,
  log 4; bits 3 / 2 → states as a4q1.

## Act 4's other objects

| object | InitFn / OperateFn | does |
|---|---|---|
| 342 hellgate (Durance 3) | 44 / 46 `FUN_00584750` | word 0x16 bit 13 or 0: the warp (`FUN_00546ac0`); from level 0x66 the fortress waypoint (0x67) given and act 4 opened (`FUN_0053aec0`) |
| 345..347 hellfire | 22 `FUN_0054fb40` | mode 2 unless the objects.txt flags +0x141 / +0x13f say; event 0 at +25 frames |
| 259, 373 skeleton spawns | 26 `FUN_005500e0` | mode 1 (client fn 7 plays it) |
| 358, 359 hell brazier | 34 `FUN_005500f0` | an **objrgn** step: odd → mode 1 |
| 380 burning soul | 51 `FUN_005501f0` / 48 `FUN_005869f0` | init: mode 1 / 2 → event 9 at +35; operate: rand(100) < 90 → a chest drop (`FUN_00585b90`) and mode 5; else mode 1, events 1 and 9 (+20) |
| 383, 384 souls | 2 / 3 | the usual urn / chest |
| 255 diablo start point | 55 `FUN_005b5590` | a4q2's +6 / +8 (and the all-seals check) |
| 131 the flash | 59 | in level 108 its event 7 makes the seal's boss |
| 369 trapped soul placeholder | 46 `FUN_005506d0` | the trapped souls (monsters.md "Act 4's population") |
| 399 fissure | 58 `FUN_0054fdb0` | an **objrgn** step: its first eruption (event 8) at that % 250 + 25 frames |
