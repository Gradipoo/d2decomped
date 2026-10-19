# The weapon switch — game.exe 1.14d

An expansion character's second set of hands (W, "Swap Weapons"). d2d:
`cmd::SwapWeapons` (protocol.hpp), `World::apply`, the inventory's set II
art in `draw_inventory` (panels.cpp), `tests/smoke_d2d.py`.

## The packets

- **C→S 0x60** (1 byte) → `FUN_0054ce70`: only on an expansion game (client
  +0x70); `FUN_00620250` then the mode check `FUN_005541b0` (a player in
  mode 0 death or 0x11 dead can't), then `FUN_005616a0` does it.
- **S→C 0x97** (1 byte): the client toggles `DAT_007bcc4c`, the set its
  inventory shows. Reset to 0 when the panel's art loads (`FUN_004868b0`).
- **S→C 0x9d action 0x17** (`FUN_004c3b00`): an item's new place after the
  switch.

## The switch (`FUN_005616a0`, ItemMode.cpp)

The items trade places; nothing tells "the set in use" apart from where
they are:

| from body slot | to |
|---|---|
| 4 right arm | 0xb |
| 5 left arm | 0xc |
| 0xb | 4 |
| 0xc | 5 |

Each is lifted from the body (`FUN_0063bde0` / `FUN_0063ad90`) and put back at
its new slot (`FUN_0063bdb0`, `FUN_0063b210`, `FUN_00627d70`); the stats are
redone (`FUN_0055dbc0`).

The skills: the left / right ones the other set kept (player data +0x84 /
+0x8c, read and cleared by `FUN_006231a0` / `FUN_00623120`) come back, and
the ones in use are kept in their place (`FUN_00622f80` / `FUN_00622ff0`).
A kept skill that's no longer usable (`FUN_00647960` says 2 or 7: not
usable, or not in the skill list) gives way to the skill list's own
(`FUN_00620190` / `FUN_006201d0` → `FUN_00643930` / `FUN_00643960`,
untraced); d2d gives Attack. The choice goes out as a select skill
(`FUN_00571c60`).

The save: header +0x10 is the set showing (0 I, 1 II); items are saved where
they are, so the hands in body slots 4 / 5 are always the set in use (a
real save on II: the bow in 4, the set I spear and shield in 0xb / 0xc).
The skill pair at +0x80 / +0x84 is what the other set kept.

## The inventory (`FUN_0048edf0`, `FUN_004845a0`)

With set II showing on an expansion game, `Panel\invchar6Tab` (loaded by
`FUN_004868b0` into `DAT_007bce84`) draws frame 0 over the left arm's box
and frame 1 over the right arm's, each at the box's left − 5 and bottom + 4
(`0x722100..0x72210c`: −5, 4, −5, 4). The two hands' items draw 4 right
and 4 up. Set I draws nothing extra (the panel's own art has the I tabs).

ponytail: a click on the I / II tabs isn't traced (W and the pad switch);
another player's switch (0x9d action 0x17) isn't followed.
