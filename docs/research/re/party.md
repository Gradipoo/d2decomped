# Players and parties — counts, experience, gold (1.14d game.exe)

Our port: `components/rules/monsters.hpp` (`player_life_pct`,
`player_exp_pct`, `monster_stats`), `components/rules/combat.hpp`
(`party_shares`), `components/rules/drops.hpp` (`roll_drops`' `nearby`),
`components/game/fight.cpp` (`Fight::killed`, `drop_counts`),
`components/game/loot.hpp` (`Loot::take`'s gold), `World::credit_party`.
Tests: `tests/test_game.cpp` (two players). The first survey is in
docs/design/net-host.md section 2; this file has the traced detail.

## The player count — FUN_00535790

Players in the game (FUN_005538d0 over the player hash, game +0x1120,
skipping units with FUN_00639df0(unit, 7) set). `/players` (DAT_00883d70,
FUN_00535780) wins only when larger and only for game types (+0x6a) 1..3.
d2d counts the World's players; there is no `/players`.

## Monster life and experience — FUN_00573930, FUN_00573cb0

FUN_00573930 (monster unit, type 1, its data at unit +0x14): unless the
data's byte +0x4c is set, n = FUN_00535790, life % = FUN_005738f0(n),
experience % = FUN_00573910(n); with +0x4c, n = 1 and no bonus.

- FUN_005738f0: n < 9 → table 0x6e1590, else (n - 2) x 50.
- FUN_00573910: n < 9 → table 0x6e15b4, else (n x 5 + 130) x 2.
- Both tables (read from game.exe's memory): 0, 0, 50, 100, 150, 200,
  250, 300, 350.

FUN_00573cb0 (0x573f83..0x573fdb): life = rand(max - min + 1) + min on
the unit seed (FUN_0045c3e0), then life + FUN_00483360(life, life %, 100),
at most 0x7fffff, x 256 into stats 7 and 6. Experience (stat 13) =
exp + FUN_00483360(exp, experience %, 100). Stat 100 is set to n.

FUN_00483360 is MulDiv truncated (plain integer division; 64-bit only
when the product would overflow).

The four other callers of FUN_00573930 (FUN_005e8810, FUN_005e9750,
FUN_005f0b00, FUN_005fc450) are AI thinks reading n; they don't rescale.
A monster keeps its stats when players join or leave.

Open: what sets monster data +0x4c. d2d's pets never take the bonus.

## NoDrop — FUN_0055a6d0, FUN_005408e0

n = nearby + (players - nearby) / 2 (drops.md "Rolling"): players is
FUN_00535790's; nearby is FUN_005408e0's count of the killer's party alive
on its level (FUN_005404f0), 1 without a party.

## Experience for a kill — FUN_0057e990

1. The killer resolved (FUN_0057e7b0): a pet's owner, a player as is.
2. The merc (FUN_00574ec0(7)) gets its own share (FUN_0057e860: 86/256 of
   the penalised award when the killer isn't the merc). d2d: not kept.
3. No party (FUN_00554630 == 0xffff): the killer gets FUN_0057e480's
   award (the level penalty, + stat 0x55 item_addexperience), added by
   FUN_0057e510.
4. A party: FUN_0057e6c0.

### FUN_0057e6c0

FUN_005405a0 walks the killer's party members whose room is on the
killer's level, calling FUN_0057e5a0 on each:

- skipped when dead (FUN_005541b0: a player in mode 0 or 0x11);
- skipped when dx² + dy² > 0x1900 (unsigned), dx / dy the monster's
  subtile less the member's;
- an 8th member is a fatal error (message 0xf25).

It records each member and its character level and sums the levels.

- No member: nobody gets anything.
- One member: the killer gets the whole award (even when the member found
  isn't the killer).
- More: total = exp + ((n - 1) x exp x 0x59) / 256 (32-bit IMUL, the
  shift rounding toward zero); share = (float)(total / Σ levels) (FILD,
  FIDIV, FSTP float); each member gets _ftol(level x share) through
  FUN_0057e480 (its own level against the monster's) and FUN_0057e510.

## Gold picked up — FUN_00540900

Called from FUN_0055c850 when the gold has no player owner. Members =
the picker's party alive on its level (FUN_005404f0).

- Fewer than 2: the picker takes what their purse holds (FUN_00622e70:
  level x 10000); the rest stays on the ground.
- Otherwise share = gold / members; every other member alive on the level
  (mode not 0, not 0x11) gets min(share, room) (FUN_00540650), the rest
  coming back to the picker, who takes what fits; what doesn't stays.

d2d: a player's own dropped gold is shared too (no owner is kept).
