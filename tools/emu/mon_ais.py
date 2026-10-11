# SPDX-License-Identifier: GPL-3.0-or-later
"""Act 2's monster thinks (docs/research/re/monster-ai.md) run natively.

    uv run python mon_ais.py <ai|all> [cases] [seed] [--dump tests/mon_ais_cases.inc]

Each MonAI think runs in game.exe on random scenes: the params block (target,
AI distance, melee), the AI control's +0x14 / +0x18 / +0x1c, MonStats aip1..8,
Skill1..5, Velocity / Run, the unit seed, the frame. Every helper it calls is
hooked at its entry: the acts (FUN_005ddf90 a mode, FUN_005de080 stand,
FUN_005de0f0 wait, FUN_005de130 event, FUN_005dead0 skill, FUN_005de000 its
sequence, FUN_005dec80 walk, FUN_005ded40 run, FUN_005def80 approach,
FUN_005defe0 back off, FUN_005df7d0 circle, FUN_005de200 wander,
FUN_005df530 about the target, FUN_005de190 pace), the effects (states,
overlays, stats set, the leader's command) and the answers the scene gives
(got hit, life %, stats, the second target FUN_005ddc30, the searches
FUN_005dd0b0 by callback, a skill usable FUN_005fd470, the leader). The
draws FUN_005df7d0 / FUN_005de200 / FUN_005df530 make on the unit seed are
made here as they make them.

Each run comes to one act in d2d's terms (rules::Think: act, n, x, y, the
unit it's at, wait, event), the pace, the effects, the AI control and seed
after. --dump writes them as tests/mon_ais_cases.inc lines for
tests/test_monsters.cpp, which runs rules::mon_think on each. Prints `ok`.
"""
import random
import struct
import sys

import emu
from merc import Seed

MASK = 0xffffffff
S32 = lambda v: v - (1 << 32) if v & 0x80000000 else v
UNITS = ("mon", "foe", "foe2", "kin", "aid")

# MonAI think addresses (the MonAI table 0x73ca18, row + 8).
AIS = {
    "Duriel": 0x5f67b0, "Swarm": 0x5f2460, "PinHead": 0x5f6340, "Mummy": 0x5f2850, "SandLeaper": 0x5f20a0,
    "ClawViper": 0x5f1b60, "PantherWoman": 0x5f22b0, "PantherJavelin": 0x5e1080, "Scarab": 0x5f2540,
    "BatDemon": 0x5f5040, "Baboon": 0x5f0cd0, "SandRaider": 0x5f0700, "GreaterMummy": 0x5f2b10,
    "SandMaggot": 0x5f1800, "Summoner": 0x5f85c0,
}
ORDER = list(AIS)
# Think acts (rules::MonAct) by name; units (rules::ThinkUnit).
ACTS = ["idle", "a1", "a2", "s2", "skill", "walk", "run", "approach", "keep", "circle", "wander", "none", "die", "around",
        "home", "point", "untraced", "s3", "s4", "seq"]
UNIT = {"foe": 0, None: 1, "foe2": 2, "kin": 3, "aid": 4, "mon": 5}
MODE_ACT = {4: "a1", 5: "a2", 9: "s2", 10: "s3", 11: "s4"}


def setup(rng, ai):
    s = {"ai": ai, "diff": rng.randint(0, 2), "frame": rng.randint(100, 5000)}
    s["seed"] = (rng.getrandbits(32), rng.getrandbits(32))
    s["target"] = True                                                     # search types 1 / 4: none found, no think (FUN_005b1650)
    s["spot"] = (rng.randint(4000, 6000), rng.randint(4000, 6000))
    s["dist"] = rng.choice((rng.randint(0, 6), rng.randint(0, 20), rng.randint(0, 60)))
    s["melee"] = rng.random() < 0.4
    s["got_hit"] = rng.random() < 0.3
    s["life"] = rng.choice((rng.randint(0, 100), rng.randint(0, 30), 100))
    word = lambda: rng.choice((0, 0, 1, 2, 3, 4, 5, -1, rng.randint(0, 12), s["frame"] + rng.randint(-60, 60)))
    s["ctrl"] = [word(), word(), word()]
    s["aip"] = [rng.choice((0, 1, 2, rng.randint(0, 100), rng.randint(0, 20), 100)) for _ in range(8)]
    ids = rng.sample(range(400), 5)                                        # distinct: the act names its slot
    s["skills"] = [rng.choice((-1, ids[k])) for k in range(5)]
    s["modes"] = [rng.randint(0, 15) for _ in range(5)]
    s["velocity"], s["run"] = rng.choice((0, 3, 5, 6, 8)), rng.choice((0, 5, 6, 9, 12))
    s["aidel"] = rng.randint(0, 20)
    s["stat"] = {(u, k): rng.choice((0, rng.randint(0, 100), rng.randint(-50, 300))) for u in ("mon", "foe") for k in (0x4a, 0x27, 0x2b)}
    s["foe2"] = rng.random() < 0.6
    s["foe2_dist"] = rng.choice((rng.randint(0, 10), rng.randint(0, 40)))
    s["kin"] = rng.random() < 0.6
    s["kin_d"] = rng.choice((rng.randint(0, 50), rng.randint(0, 2000)))   # FUN_005b0bd0: squared
    s["corpse"], s["aid"] = rng.random() < 0.5, rng.random() < 0.5
    s["counts"] = (rng.randint(0, 4), rng.randint(0, 4))
    s["usable"] = rng.random() < 0.7
    s["command"] = rng.choice((None, 1, 2))                                # FUN_0058ee80: no command / its kind
    s["leader"] = rng.random() < 0.5                                       # FUN_0058f0d0 gives itself
    s["aura"] = rng.random() < 0.5                                         # FUN_006201d0: its right skill (an aura) is on
    s["fails"] = rng.random()
    return s


CUR = {}


def install(e):
    cur = CUR
    def out(*item): cur["out"].append(item); return 0
    def name(addr): return cur["names"].get(addr)
    def seed():
        u = cur["units"]["mon"]
        return Seed(e.r32(u + 0x20), e.r32(u + 0x24))
    def put(sd):
        u = cur["units"]["mon"]
        e.w32(u + 0x20, sd.lo); e.w32(u + 0x24, sd.hi)
    def moved():
        return int(cur["moves"].random() >= cur["s"]["fails"])
    # The acts.
    e.hook(0x5ddf90, lambda e: out("mode", e.arg(0), name(e.arg(1))), 2)
    e.hook(0x5de080, lambda e: out("stand", S32(e.arg(0))), 1)
    e.hook(0x5de0f0, lambda e: out("wait", S32(e.arg(0))), 1)
    e.hook(0x5de130, lambda e: out("event", S32(e.arg(0))), 1)
    e.hook(0x5dead0, lambda e: out("skill", e.arg(0) & 0xff, S32(e.arg(1)), name(e.arg(2)), S32(e.arg(3)), S32(e.arg(4))) or 1, 5)
    e.hook(0x5de000, lambda e: out("seq", S32(e.arg(0)), name(e.arg(1))) or 1, 4)
    def walk(kind):                                                    # set off (ai.cpp handles one that can't)
        def go(e):
            out(kind, name(e.arg(0)), S32(e.arg(1)), 1)
            return 1
        return go
    e.hook(0x5dec80, walk("walk"), 2)
    e.hook(0x5ded40, walk("run"), 2)
    e.hook(0x5def80, walk("approach"), 2)
    def away(e):
        ok = moved()
        out("away", name(e.arg(0)), S32(e.arg(1)), e.arg(2), ok)
        return ok
    e.hook(0x5defe0, away, 3)
    def circle(e):                                                     # FUN_005df7d0: a step, then at the target
        sd = seed(); v = sd.step(); put(sd)
        out("circle", name(e.arg(0)), S32(e.arg(1)), e.arg(2), int((v & 0xff) >= 0x80))
        return 1
    e.hook(0x5df7d0, circle, 3)
    def wander_draws(n):                                               # FUN_005de200 / FUN_005df530's three draws
        sd = seed()
        if sd.step() & 1 == 0: x, y = sd.rand(n), n
        else: x, y = n, sd.rand(n)
        if sd.step() & 1: x = -x
        if sd.step() & 1: y = -y
        put(sd)
        return x, y
    def wander(e):
        n = e.arg(0) & 0xff
        out("wander", n, *wander_draws(n))
        return 1
    e.hook(0x5de200, wander, 1)
    def around(e):
        unit, n = e.arg(0), e.arg(1) & 0xff
        if not unit: return wander_n(2)
        out("around", name(unit), n, *wander_draws(n))
        return 1
    def wander_n(n):
        out("wander", n, *wander_draws(n)); return 1
    e.hook(0x5df530, around, 2)
    def pace(e):
        out("pace", S32(e.arg(0)), S32(e.arg(1)))
        return 0
    e.hook(0x5de190, pace, 2)
    # The effects.
    e.hook(0x639db0, lambda e: out("state", e.arg(1), e.arg(2)), 3)
    e.hook(0x621e40, lambda e: out("overlay", e.arg(1)), 3)
    def setstat(e):
        out("stat", e.arg(1), S32(e.arg(2)))
        return 0
    e.hook(0x627260, setstat, 4)
    e.hook(0x58f730, lambda e: out("rally"), 1)
    e.hook(0x58ef40, lambda e: 0, 1)
    e.hook(0x58ed10, lambda e: out("command_done"), 0)
    e.hook(0x59c330, lambda e: out("summoner_open"), 0)
    e.hook(0x56deb0, lambda e: 0, 2)
    e.hook(0x5701b0, lambda e: out("aura", S32(e.arg(0))), 2)
    e.hook(0x620c10, lambda e: 0, 2)                                   # face the unit
    # The answers.
    e.hook(0x5dd2b0, lambda e: int(cur["s"]["got_hit"]), 0)
    e.hook(0x621f20, lambda e: cur["s"]["life"], 1)
    e.hook(0x5dd280, lambda e: cur["s"]["life"], 0)
    e.hook(0x625480, lambda e: cur["s"]["stat"].get((name(e.arg(0)), e.arg(1)), 0) & MASK, 3)
    def second(e):
        s = cur["s"]
        e.w32(e.arg(0), s["foe2_dist"] if s["foe2"] else 0x7fffffff)
        if not s["foe2"]: return 0
        return cur["units"]["foe2"]
    e.hook(0x5ddc30, second, 2)
    def search(e):
        s, ctx, cb = cur["s"], e.arg(0), e.arg(1)
        if cb in (0x5b0d00, 0x5f06b0):                                     # the nearest kin / other monster
            if s["kin"] and S32(s["kin_d"]) < S32(e.r32(ctx + 4)):
                e.w32(ctx, cur["units"]["kin"]); e.w32(ctx + 4, s["kin_d"])
        elif cb == 0x5f2a00:                                               # a corpse, a hurt one, their counts
            if s["corpse"]: e.w32(ctx, cur["units"]["kin"])
            if s["aid"]: e.w32(ctx + 4, cur["units"]["aid"])
            e.w32(ctx + 8, s["counts"][0]); e.w32(ctx + 12, s["counts"][1])
        else:
            raise RuntimeError(f"search callback {cb:#x}")
        return 0
    e.hook(0x5dd0b0, search, 3)
    e.hook(0x5fd470, lambda e: int(cur["s"]["usable"]), 4)
    def command(e):
        c = cur["s"]["command"]
        if c is None: return 0
        e.w32(cur["cmd"] + 8, c)
        return cur["cmd"]
    e.hook(0x58ee80, command, 0)
    e.hook(0x58f0d0, lambda e: cur["units"]["mon"] if cur["s"]["leader"] else cur["units"]["kin"], 0)
    e.hook(0x6201d0, lambda e: int(cur["s"]["aura"]), 1)
    e.hook(0x6488c0, lambda e: cur["s"]["spot"][0], 1)                 # the target's path spot
    e.hook(0x648900, lambda e: cur["s"]["spot"][1], 1)
    e.hook(0x599420, lambda e: 0, 0)                                   # Radament's: FUN_00599420 (the quest's)


def run_game(e, s, addr, rng_moves):
    def alloc(n): a = e.alloc(n); e.mu.mem_write(a, bytes(n)); return a
    game, params, ctrl, row, cmd = (alloc(n) for n in (0x2000, 0x40, 0x40, 0x200, 0x40))
    units, names = {}, {}
    for k, n in enumerate(UNITS):
        u = alloc(0x200)
        e.w32(u, 1); e.w32(u + 4, 0x100 + k); e.w32(u + 0xc, k + 1); e.w32(u + 0x2c, alloc(0x80))
        units[n], names[u] = u, n
    mon = units["mon"]
    e.w32(mon + 0x20, s["seed"][0]); e.w32(mon + 0x24, s["seed"][1])
    e.mu.mem_write(game + 0x6d, bytes([s["diff"]])); e.w32(game + 0xa8, s["frame"])
    for k, v in enumerate(s["aip"]): e.mu.mem_write(row + 0x56 + 6 * k + 2 * s["diff"], struct.pack("<h", v))
    e.mu.mem_write(row + 0x32, struct.pack("<hh", s["velocity"], s["run"]))
    e.mu.mem_write(row + 0x170, struct.pack("<5h", *s["skills"]))
    e.mu.mem_write(row + 0x180, bytes(s["modes"]))
    e.mu.mem_write(row + 0x4f + s["diff"], bytes([s["aidel"]]))
    for k, v in enumerate(s["ctrl"]): e.w32(ctrl + 0x14 + 4 * k, v & MASK)
    e.w32(params, ctrl); e.w32(params + 7 * 4, row)
    if s["target"]: e.w32(params + 8, units["foe"])
    e.w32(params + 20, s["dist"]); e.w32(params + 24, int(s["melee"]))
    CUR.update(s=s, out=[], units=units, names=names, moves=rng_moves, cmd=cmd)
    e.call(addr, params, ecx=game, edx=mon)
    return CUR["out"], [S32(e.r32(ctrl + 0x14 + 4 * k)) for k in range(3)], (e.r32(mon + 0x20), e.r32(mon + 0x24))


def outcome(s, out):
    """The native calls in rules::Think's terms: (act, n, x, y, unit, wait, event), the
    pace (pct, steps), the effects, the back-offs and moves asked (n, ok) in order."""
    act, pace, effects, asks = ["none", 0, 0, 0, 0, 0, 0], [0, 0], [], []
    def to(name, n=0, x=0, y=0, unit="foe"):
        act[:] = [name, n, x, y, UNIT[unit], act[5], act[6]]
    for item in out:
        k = item[0]
        if k == "mode": to(MODE_ACT[item[1]], unit=item[2])
        elif k == "stand": to("idle", item[1])
        elif k == "wait": act[5] = item[1]
        elif k == "event": act[6] = item[1]
        elif k == "skill":
            slot = s["skills"].index(item[2])
            spot = (item[4], item[5]) == s["spot"]                         # at the target's spot (the Summoner's)
            to("skill", slot, 0 if spot else item[4], 1 if spot else item[5], item[3])
        elif k == "seq": to("seq", s["skills"].index(item[1]), unit=item[2])
        elif k in ("walk", "run", "approach"): to(k, item[2], unit=item[1])
        elif k == "away":
            asks.append((item[2], item[4]))
            if item[4]: to("none")
        elif k == "circle": to("circle", item[2], item[4], item[3], item[1])
        elif k == "wander": to("wander", item[1], item[2], item[3])
        elif k == "around": to("around", item[2], item[3], item[4], item[1])
        elif k == "pace":
            if item[1]: pace[0] = item[1]
            if item[2]: pace[1] = item[2]
        elif k == "aura": effects.append(("aura", s["skills"].index(item[1])))
        else: effects.append(item)
    return act, pace, effects, asks


def dump_case(s, act, pace, effects, asks, ctrl, seed):
    """A tests/mon_ais_cases.inc line: the scene | the moves asked | what it comes to."""
    nums = [ORDER.index(s["ai"]), s["diff"], s["frame"], int(s["target"]), s["dist"], int(s["melee"]), int(s["got_hit"]), s["life"]]
    nums += s["ctrl"] + s["aip"] + [int(k >= 0) for k in s["skills"]]
    nums += [s["velocity"], s["run"], s["aidel"]]
    nums += [s["stat"][(u, k)] for u in ("mon", "foe") for k in (0x4a, 0x27, 0x2b)]
    nums += [int(s["foe2"]), s["foe2_dist"], int(s["kin"]), s["kin_d"], int(s["corpse"]), int(s["aid"]), *s["counts"]]
    nums += [int(s["usable"]), s["command"] or 0, int(s["leader"]), int(s["aura"])]
    nums += list(s["seed"])
    kinds = {"state": 0, "overlay": 1, "stat": 2, "rally": 3, "command_done": 4, "summoner_open": 5, "aura": 6}
    eff = [f"{kinds[x[0]]} {x[1] if len(x) > 1 else 0} {x[2] if len(x) > 2 else 0}" for x in effects]
    tail = [ACTS.index(act[0])] + act[1:] + pace + ctrl + list(seed)
    return " ".join(map(str, nums)) + " | " + " ; ".join(f"{n} {ok}" for n, ok in asks) + " | " + " ; ".join(eff) + " | " + " ".join(map(str, tail))


def check(e, ai, cases, rng, dumped):
    mark, kinds = e.brk, {}
    for _ in range(cases):
        e.brk = mark
        s = setup(rng, ai)
        out, ctrl, seed = run_game(e, s, AIS[ai], random.Random(rng.getrandbits(32)))
        act, pace, effects, asks = outcome(s, out)
        kinds[act[0]] = kinds.get(act[0], 0) + 1
        if dumped is not None:
            key = (act[0], tuple(x[0] for x in out), len(asks))
            rows = dumped.setdefault(ai, {})
            if len(rows) < 40 or key not in rows: rows.setdefault(key, []).append(dump_case(s, act, pace, effects, asks, ctrl, seed))
    print(f"{ai} ok: {cases} cases, {kinds}")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--") and not a.endswith(".inc")]
    which = args[0] if args else "all"
    cases = int(args[1]) if len(args) > 1 else 2000
    rng = random.Random(int(args[2]) if len(args) > 2 else 1)
    e = emu.Emu()
    install(e)
    dumped = {} if "--dump" in sys.argv else None
    for ai in (AIS if which == "all" else which.split(",")): check(e, ai, cases, rng, dumped)
    if dumped is not None:
        with open(sys.argv[sys.argv.index("--dump") + 1], "w") as f:
            f.write("// tools/emu/mon_ais.py --dump: game.exe's own monster thinks (see the script), a line each.\n")
            for rows in dumped.values():
                for lines in rows.values(): f.write("".join(f'"{line}",\n' for line in lines[:6]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
