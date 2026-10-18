# whisper

A small control socket inside the **original game.exe (1.14d)**, so the real
game can be read and driven from another machine on your own network.

## Why

d2d's ground truth is game.exe, and the mode that matters is d2d joining a
game.exe host. Checking that live used to need a person at each keyboard:
one playing the host, one reading d2d, both describing what they saw. whisper
puts the host on a socket instead, so one session (or one script) can play
both sides:

- **See what the host sees**: `screenshot` of the game's window, next to
  d2d's own `devctl screenshot` of the same moment.
- **Read the host's truth**: `state` gives the host player's unit id,
  class, mode and subtile position straight from game.exe's memory; `peek`
  reads any address. No guessing from packets.
- **Drive the host**: `click`, `rclick`, `key` and `text` go in as ordinary
  window messages, so game.exe's client acts exactly as it would for a
  mouse; `send` puts a raw C->S packet through game.exe's own SendPacket.
- **Check our packets against game.exe's parser**: `send` refuses anything
  game.exe's own sizer (`FUN_0052bc20`) frames differently, the same check
  the host applies to d2d. That is how d2d's chat packet was caught missing
  its count byte (2026-10-17).

It turns the "netplay live checklist" from a supervised session into
something scriptable, and later a scripted Act 1-4 run against the host.

## How it loads

game.exe imports `IJL11.DLL` (Intel's JPEG library) by ordinals 2, 3 and 5,
from its own folder. whisper is built as a stand-in `ijl11.dll`: it forwards
those three to the real library (renamed `ijl11_real.dll`) and starts its
socket thread. No injector, no patched bytes; undoing it is a rename.

Everything that reads or sends runs on the game's own thread (a `WM_APP`
sent to the subclassed game window), the same "commands in, then the tick"
rule d2d follows.

## Install (on the game.exe machine)

1. Build here: `./build.sh` (needs `brew install mingw-w64`) makes `ijl11.dll`.
2. In the Diablo II folder rename `ijl11.dll` to `ijl11_real.dll`, then copy
   the built `ijl11.dll` in. With file extensions hidden in Explorer, check
   the rename didn't make `ijl11_real.dll.dll`.
3. Write `whisper.txt` beside `game.exe`: a token on line 1
   (`openssl rand -hex 12`), the port on line 2 (default 4100).
   **Without that file nothing listens**; a client must send the token first.
4. Start the game, host a TCP/IP game, and allow it through the firewall
   when Windows asks.

game.exe must be 1.14d: the addresses are its own. A different build shows
as `state` failing (check `peek 463dd0 2`: 1.14d reads `7a6a70a1 ccccc300`).
Problems are written to `whisper.log` beside game.exe.

Undo: delete `ijl11.dll`, rename `ijl11_real.dll` back.

## Use

    ./whisper.py 192.168.50.68 TOKEN state
    ./whisper.py 192.168.50.68 TOKEN click 400 300
    ./whisper.py 192.168.50.68 TOKEN send 15 01 00 68 69 00 00 00   # chat "hi"
    ./whisper.py 192.168.50.68 TOKEN screenshot /tmp/host.bmp

| Verb | Reply | Does |
|---|---|---|
| `ping` | `ok` | Liveness. |
| `state` | `id … type … class … mode … x … y … life a/b mana a/b` | The host player's unit (`DAT_007a6a70`, set by `FUN_00463d90`); x / y in subtiles; stats through `FUN_00625480`. |
| `units` | `type id class mode x y life maxlife` per unit | Every unit the client knows (its two hash tables, `FUN_00463a50`). |
| `peek ADDR [N]` | `addr value` per u32, `?` if unreadable | Read game.exe memory (up to 64 u32s), safely. |
| `click X Y` / `rclick X Y` / `move X Y` | `ok` | Mouse at window coords (800x600). |
| `realclick X Y` / `realrclick X Y` | `ok` / `err` | A real click: `SetCursorPos` + `SendInput`. Needed for an NPC's or warp's interact (posted clicks walk there, never act). The game must be the foreground window; the machine's cursor moves. |
| `key VK` | `ok` | A virtual-key press: 13 Return, 27 Esc, 0x41 'A'. |
| `text WORDS` | `ok` | Typed characters (WM_CHAR), e.g. into the chat box. |
| `send HEX…` | `ok <result>` / `err` | A raw C->S packet via `FUN_0052ae50`, refused unless `FUN_0052bc20` frames it at exactly that size. |
| `screenshot` | `ok <n>` + n BMP bytes | The game window. `whisper.py` saves it to a path. |

Several clients may connect; their commands still run one at a time on the
game thread. Keep it on your own network: anyone with the token can drive
the game.

**Posted clicks aren't a whole mouse.** Hover, moving, attacking and
talking work, but a posted click on a warp walks the player to its mouth
and stops; a real click goes in (2026-10-17). game.exe reads more than the
window messages there (the real button state, it seems), so the follower
takes warps and town portals with `send 13` (interact) once there.

## The follower

    ./whisper.py 192.168.50.68 TOKEN follow

Plays the host's character as a merc for whoever is driving the other
player (d2d): stays within a few subtiles of them, attacks the nearest
monster near both of you with the left skill (MonStats' npc / ally /
in-town / unkillable rows left alone), and drinks from belt columns 1-4
under 45% life. Warps and waypoints are the driver's: walk up to them with
`click`, or let the follower close in and click the warp yourself.
