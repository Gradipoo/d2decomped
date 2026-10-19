#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Local co-op over virtual pads (docs/design/local-coop.md, phase 3).

Usage: coop_d2d.py <path-to-d2d>

A headless d2d with three synthetic saves and devctl's virtual pads: a pad
and the Steam Deck's own controls are in as the game begins (co-op; the
Deck's never takes a seat), a second pad comes and picks a save on its
roster, each pad walks its own player, the Deck's walks nobody, a pad
unplugged and back takes its seat again, and a third pad makes a new
character (class, typed name). Skips without the MPQs.
"""
import atexit, os, platform, socket, struct, subprocess, sys, tempfile, time

d2d = sys.argv[1]
data = os.environ.get("D2_MPQ_DIR") or os.path.expanduser("~/Workspace/private/diablo2")
if not os.path.exists(os.path.join(data, "d2data.mpq")):
    print(f"SKIP: {data}/d2data.mpq not found")
    sys.exit(0)

home = tempfile.mkdtemp(prefix="d2d_coop_")
user = os.path.join(home, "Library/Preferences/d2d" if platform.system() == "Darwin" else ".config/d2d")
saves = os.path.join(user, "save")
os.makedirs(saves)
for index, name in enumerate(["Alpha", "Beta", "Gamma"]):   # expansion softcore, newest first: Alpha
    b = bytearray(0x2FD)
    struct.pack_into("<III", b, 0, 0xAA55AA55, 96, len(b))
    b[0x14:0x14 + len(name)] = name.encode()
    b[0x24], b[0x28], b[0x2B] = 0x20, index + 1, 10
    struct.pack_into("<I", b, 0x30, 1_000_000 - index)   # last played
    with open(os.path.join(saves, name + ".d2s"), "wb") as f:
        f.write(b)
patch = os.environ.get("D2_PATCH_INSTALLER")
if patch and os.path.exists(patch):
    with open(os.path.join(user, "d2d.cfg"), "w") as f:
        f.write(f"patch = {patch}\n")

sock_path = os.path.join(tempfile.gettempdir(), f"d2d_coop_{os.getpid()}.sock")
proc = subprocess.Popen([d2d, "--headless", "--no-save", "--seed", "3", "--data", data, "--devctl", sock_path, "--start-screen", "charselect"],
                        env=dict(os.environ, HOME=home))
atexit.register(lambda: proc.poll() is None and proc.kill())

def cmd(line):
    with socket.socket(socket.AF_UNIX) as s:
        s.settimeout(10)
        s.connect(sock_path)
        s.sendall((line + "\n").encode())
        out = ""
        while not out.endswith("\n") or not out.rsplit("\n", 2)[-2].startswith(("ok", "err")):
            chunk = s.recv(65536)
            assert chunk, f"{line!r}: connection closed after {out!r}"
            out += chunk.decode()
    assert not out.rsplit("\n", 2)[-2].startswith("err"), f"{line!r} -> {out!r}"
    return out

def frames(n):
    frame = lambda: int(cmd("info").split("frame=")[1].split()[0])
    want, deadline = frame() + n, time.time() + 20 + n / 10
    while frame() < want:
        assert time.time() < deadline, f"stuck waiting for frame {want}"
        time.sleep(0.005)

def players():
    rows = [line.split("\t") for line in cmd("debug players").splitlines() if "\t" in line]
    return {row[1]: (float(row[2]), float(row[3]), int(row[4])) for row in rows}

def walk(pad, frames_held=40):
    cmd(f"pad {pad} axis leftx 1"); frames(frames_held)
    cmd(f"pad {pad} axis leftx 0"); frames(10)

for _ in range(400):
    if os.path.exists(sock_path):
        break
    assert proc.poll() is None, "d2d exited during startup"
    time.sleep(0.1)
frames(1)
cmd("pad 0 plug")
cmd("pad 1 plug deck")
frames(5)
cmd("key Return")                  # Alpha, the newest save
frames(40)
start = players()
assert list(start) == ["Alpha"] and start["Alpha"][2] == -1, start   # the Deck's pad took no seat

cmd("pad 2 plug")                  # a second pad: its roster, Alpha greyed (playing), the cursor on Beta
frames(10)
assert list(players()) == ["Alpha"]
def roster():
    rows = [line.split("\t") for line in cmd("debug roster").splitlines() if "\t" in line]
    return {int(row[0]): (int(row[1]), int(row[2]), row[3].rstrip(",").split(","), row[4]) for row in rows}
assert roster() == {1: (0, 1, ["*Alpha", "Beta", "Gamma", "New character"], "")}, roster()
cmd("debug day 90"); frames(5)
cmd("screenshot " + os.environ.get("COOP_SHOT", os.path.join(home, "roster.png")))   # COOP_SHOT: where to keep it
def press(pad, button):
    cmd(f"pad {pad} {button} down"); frames(3)
    cmd(f"pad {pad} {button} up"); frames(3)
press(2, "a")                      # Beta joins at seat 1
joined = players()
assert set(joined) == {"Alpha", "Beta"} and joined["Beta"][2] == 1 and roster() == {}, (joined, roster())

walk(2)                            # the second pad walks Beta, not Alpha
moved = players()
assert moved["Alpha"][:2] == joined["Alpha"][:2], (joined, moved)
assert abs(moved["Beta"][0] - joined["Beta"][0]) + abs(moved["Beta"][1] - joined["Beta"][1]) > 1, (joined, moved)

walk(1)                            # the Deck's own controls walk nobody in co-op
still = players()
assert still["Alpha"][:2] == moved["Alpha"][:2] and still["Beta"][:2] == moved["Beta"][:2], (moved, still)

walk(0)                            # the first pad walks Alpha
alpha = players()
assert abs(alpha["Alpha"][0] - still["Alpha"][0]) + abs(alpha["Alpha"][1] - still["Alpha"][1]) > 1, (still, alpha)

cmd("pad 2 unplug")                # held, then back: the same seat, no one new
frames(10)
assert set(players()) == {"Alpha", "Beta"}
cmd("pad 2 plug")
frames(10)
back = players()
assert set(back) == {"Alpha", "Beta"} and back["Beta"][2] == 1, back

cmd("pad 3 plug")                  # a third pad makes a new Necromancer, Zed
frames(10)
assert roster()[2][2] == ["*Alpha", "*Beta", "Gamma", "New character"], roster()
for _ in range(3): press(3, "dpdown")              # past the end: stays on New character
press(3, "a")
assert roster()[2][:2] == (1, 0), roster()          # the classes
press(3, "dpdown"); press(3, "dpdown")              # Amazon, Sorceress, Necromancer
press(3, "a")
assert roster()[2][0] == 2, roster()                # the name
cmd("text Zed"); frames(5)
assert roster()[2][3] == "Zed", roster()
press(3, "start")
assert set(players()) == {"Alpha", "Beta", "Zed"} and players()["Zed"][2] == 2 and roster() == {}, (players(), roster())

cmd("quit")
proc.wait(timeout=20)
print("OK: co-op pads: seats, the roster (a save, a new character), each pad its own player, the Deck's left out, a replug")
