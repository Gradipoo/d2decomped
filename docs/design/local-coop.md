# Local co-op — up to 4 pads, one screen

Status: planned 2026-10-19, no code yet. Builds on
[net-host.md](net-host.md) stages 1-2 (the N-player World and its rules) and
[controller-plan.md](controller-plan.md) (one pad today).

Goal: friends play together on one screen, from one machine, with no
game.exe running. The target is a docked Steam Deck with external pads; any
PC or Mac with pads works the same way.

## Rules

**Who plays**

- Co-op uses external pads only, at most 4. The first external pad is P1.
- The Deck's built-in controls never take a co-op slot. They still drive
  the Deck itself (Steam, the overlay).
- Handheld, the Deck's controls play as today: single player, or joining a
  game.exe host.
- Off the Deck every pad counts.

**When co-op is on**

- Co-op is on when at least one external pad is connected at game start.
  Docking itself isn't detected; an external pad is the signal.
- The mode is fixed when the game starts. A game begun on the Deck's
  controls stays single player even if a pad is plugged in later. Co-op
  starts from the title with a pad in.

**Joining and leaving**

- P1 picks the first character at character select, as today.
- A pad that comes online mid-game pops up its cursor at once. That player
  picks a character, or makes one, from a roster panel drawn over the game.
  Name entry uses Steam's on-screen keyboard (`main.cpp`, SDL_StartTextInput).
- The new player appears next to the lead, wherever the party is.
- Only characters that match the lead's difficulty (unlocked), hardcore and
  expansion flags can join; D2's own rule. A character already playing is
  greyed out.
- Saves stay local (the usual save directory). Each player's character is
  written on leaving and on quitting.
- A pad that disconnects: its character is saved and the slot is held about
  60 s. The same pad reconnecting takes it back; otherwise the player leaves.
- The lead quitting: the next player becomes lead and the game goes on. The
  difficulty stays (every player matched it on joining).

**Cursors**

- When 2 or more pads are connected, each cursor is tinted by its pad: P1
  red, P2 green, P3 blue, P4 magenta. A colour stays with its pad when
  others leave.
- The cursor is a palette-indexed DC6; the tint is a colour multiply on the
  cursor's blit only.

**One screen**

- Camera: the centre of all players, leashed. Nobody walks past the screen
  edge.
- Warps, waypoints and portals move the whole party. One screen means one
  level.
- Panels (inventory, stash, store, skill tree, roster): one player owns the
  open panel until it closes; the others keep playing.

## Fidelity

- Co-op on one screen is a d2d addition; it goes in
  [deviations.md](../research/re/deviations.md). Single player stays as it
  is.
- Player-count rules follow game.exe exactly, as traced in net-host.md
  section 2: monster HP and exp scaling (0x6e1590 / 0x6e15b4), NoDrop with
  `near` and `players`, the party exp split (FUN_0057e6c0), the gold split
  (FUN_00540900), party and area quest credit. Co-op players are one party.
- Which game type (+0x6a) co-op counts as decides the death gold rule
  (FUN_005357d0 checks type 3 only): net-host.md open question 1.

## Phases

| # | What | Size |
|---|---|---|
| 1 | The N-player World: net-host.md item 1 (`PlayerSession` / `GameSession`, tagged commands, `quests(player)`, `create_game` / `join`). Each player gets their own View; the others draw through the View's existing `others` list. | 1.5-2 weeks |
| 2 | Player-count rules: net-host.md items 4 and 5 (scaling, NoDrop, party exp, gold split, quest credit, TP per player). | 1-1.5 weeks |
| 3 | Pads become players: up to 4 open, one `pad::Mapper` each, hotplug opens a slot, the 60 s hold. Tell the Deck's built-in controller from external pads (below). | 2-3 days |
| 4 | The roster panel and tinted cursors: pick or make a character in-game, the join rules above. | 3-4 days |
| 5 | One screen: the centroid camera with its leash, party warps, panel ownership, per-player saves, the lead hand-over. | 3-4 days |

Not needed for co-op: net-host.md item 2 (room activation for players on
different levels). One screen keeps the party on one level.

## Checks

- Phase 1 keeps every existing test green: test_protocol, the per-act
  checks, smoke_d2d, and the netplay join path (`apps/d2d/netgame.cpp`).
- Two to four virtual players driven by devctl, as net-host.md stage 1.
- The built-in vs external split needs a real Deck. Steam Input shows every
  pad as a virtual Xbox pad; SDL 3 should still report the real device
  behind it (the Deck's controller is USB 0x28de / 0x1205). Phase 3 logs
  each pad's real type and ids; one run on a docked Deck with a pad settles
  it.

## What it does for networking

- Phases 1 and 2 are net-host.md stages 1 and 2. A d2d host needs them
  as they are.
- Phases 3 to 5 are local only (input, one screen). The roster and
  per-player saves carry over to a host's join.
- Still left for hosting after co-op: the D2GS codec halves, the
  `PacketEmitter`, the join handshake, room activation (net-host.md items
  2, 6-10).
