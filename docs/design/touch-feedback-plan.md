# Touch and gamepad feedback plan

Prepared 2026-11-17. Implementation is now present in the working tree;
the sections below preserve the design and its acceptance criteria.

## Implementation status

- Integrated: full-width safe-area touch layout, circular spell art and menu
  glyphs, hidden idle movement stick, aggregated healing/mana belt buttons,
  expanded belt access, bottom-mounted original orbs with angel/demon housing,
  teleport landing previews,
  modest input-only acquisition tuning, gamepad loot radial and touch loot list.
- Integrated effect authority: stable instances, remaining durations,
  conservative positive-effect dismissal and LAN protocol version 8. Host
  dismissal is opt-in via `--toggle dismiss_effects=on`; stock D2GS hosts
  cannot receive this command. Touch hold inspection, paged effect lists,
  instance-specific confirmation and transient visual button feedback are
  integrated.
- Device-feedback revisions: compact spell spacing, potion sprites/counts,
  separate Map/Belt hit boxes, a larger widescreen movement region, raised
  stamina/XP bars, contextual hand/dialogue glyphs, hold-to-repeat melee skills,
  and retail ground labels during the touch loot list or a held Get gesture.
  War Cry touch drags stay centered; opening a loot chooser cancels field gestures.
  Ownership checks also preserve the mouse-backed touch pointer and prevent
  another finger from stealing an active cast.
- Howl and other caster-centered skills now refresh the caster position
  and clear any creature target at dispatch. The real touch adapter to world
  regression passes with local 1.14d data, including no queued approach.
  The original phone report still needs device confirmation; nova ring
  emission is unchanged.
- The native `d2d` application built and linked successfully with these
  revisions. All 29 selected CMake/CTest regressions pass with local 1.14d
  data configured and no skips, including vendor transactions, quest review,
  position casts, mana/readiness and movement casts. Source
  ordering and working-tree whitespace checks pass. Physical-device validation
  and broader game-data-backed scenarios remain outstanding. The LAN listener test cannot open its
  localhost socket in this environment; handshake execution is unverified.
- The selected-test results above describe the pre-integration revision.
  Upstream integration and its validation are reported separately in the PR.

## Thumb-fan revision (2026-11-18)

The annotated phone screenshot supersedes the earlier compact column layout.
Four 80-unit skill targets follow a tighter arc above/left of the main attack.
The lower centers are pulled inward and the potion/Use group is 15 units
closer to the attack button; hit targets remain separated by at least 8 units.
Two 64-unit potion targets sit on its inner-left side, health below mana;
the contextual Use/Get/Talk target sits above with a 16-unit safety gap.
All primary thumb targets have at least 8 units of separation, verified at
800, 1067 and 1300 logical widths with zero and asymmetric safe insets.
Physical thumb comfort still requires device testing.

Button faces now use deterministic dark stone texture and aged-bronze bevels,
with original icon art inside the rim and pressed/disabled shading. No new
art assets or dependencies are added. Rendering checks cover clipping,
contrast, bevel preservation and state shading.

Held offensive skills now activate after a 300 ms hold, keep the selected
hostile target while valid, and reacquire in the held aiming direction.
Commands respect local animation/cooldown state, with bounded skill-delay
retries for remote hosts. Release, a deliberate drag beyond the aim area,
focus/device loss, or a modal/panel ends the hold; queued melee approaches
are cancelled while separately held movement remains available. A latched
red “release to cancel” cue identifies cancellation. Taps cast once;
auras, buffs, summons, corpse skills and unsupported casts retain their
single-use behavior. Damaging Nova/War Cry and ordinary offensive missiles
can repeat; non-damaging Howl does not repeat.

Thirteen focused checks pass: ten existing CTest regressions plus strict
standalone builds/runs of the current input, hold and theme tests. Native
application build and link pass. Physical-device and live LAN
combat validation remain outstanding.

## Point indicators and binding revision (2026-11-18)

Skills receives a gold rim and plus badge while skill points are available.
A new Stats chip beside the top-left controls opens the character panel and
indicates unspent stat points. Both are derived from current character stats,
so spending the points clears the indication. Inventory, Skills and Stats
use the established panel toggles rather than forcing panels open.

Back/Done has a larger circular curved-arrow target, with Map positioned
below it. Touch XP uses the native HUD palette entry; stamina uses its
normal, depleted and blue-effect palette rules.

Touch binding now shares the existing gamepad skill discovery and action
choices, with explicit 64-unit circular Prev/Next controls and a page count.
Paging retains the selected destination; the final page contains actions. Town Portal sources include inventory tomes and loose scrolls;
these are separate from the Sorceress Teleport skill. Self-centered shouts
stay at the caster without requiring or approaching an enemy. Non-damaging
Howl and buff shouts cast once on release; damaging Nova and War Cry can
repeat at the moving caster while held, even without a nearby enemy.

The native application builds and links. Updated wheel, panel policy,
geometry, gesture, glyph and theme checks pass, as does the 1.14d
touch-to-world Howl regression. On-device binding, loose-scroll use and
physical Back/Stats reach still need validation.

## Touch action feedback revision (2026-11-18)

Touch-only local selection clicks reuse the existing frontend button sound
and volume path for accepted Inventory, Skills, Stats, Quest, Map, Bind,
Belt and Back/More transitions, plus binding destination/page selection and
successful assignment saves. Deferred Back requests are resolved by the
existing Escape handler and cleared on focus loss/background entry. Repeated
identical action/destination cues are throttled for 100 ms; distinct steps
remain audible. Mouse and gamepad sound paths are unchanged.

A failed assignment save leaves the previous sheet intact, keeps the picker
open, displays failure text, and produces rejection feedback rather than a
success click. Existing item/spell gameplay audio is retained; no extra
selection click is layered onto potion use or spell casts. Nested loot/effect
menus remain outside this pass.

Seventeen independent visual channels now cover field controls, menus, Back,
and binding pages. Pulses render over panels as well as the field, saved
assignments flash their destination, and unavailable panel actions give a
muted rejection pulse. The old mapping of extra channels to the mana button
is removed. Skill artwork and rings scale with the enlarged buttons.

Seventeen focused checks pass (nine strict standalone input/UI tests and
eight existing CTest regressions). The native application rebuilt and linked
successfully.
Audible feedback, mute behavior and thumb comfort still need device checks.

## Notification, free-cast and vendor revision (2026-11-18)

Skills and Stats use red sampled from the native point-notification artwork
while points remain. Quest uses the same red for native quest updates not
yet reviewed. A separate touch review tracker survives opening or changing
tabs in the native log, and acknowledges only the selected entry when its
content is displayed. Reviewing one update does not clear other quests.
The native quest button's existing lifecycle is unchanged.

Without an eligible creature, touch taps cast in the facing direction and
swipes/holds cast toward the finger. A held melee attack continues swinging
in place after its target dies; it does not walk to an empty aiming spot.
The World/Fight position-cast path also supports existing Shift-click
commands. Charge and Leap Attack accept position destinations using their
existing movement/collision sequences, with a guard separating host swing
display from real position movement. Howl and other radial/self casts remain
at the caster. Resource, passive, cooldown, corpse and teleport restrictions
still apply. These changes are regression-tested, not a new game.exe oracle
or a claim of verified retail parity for movement abilities.

Vendor rendering and picking share one stock-grid origin, fixing the shifted
hit boxes that made Misc supplies difficult to select. Corner touch chips
are hidden while trading so they cannot cover vendor tabs or stock; Back
and Map remain available. Buy and Sell sheets have only their transaction
and Cancel actions, with no extra Confirm; other Use/Fill sheets retain their
existing behavior. Inventory items can be dragged onto eligible vendor stock
areas to sell; vendor items can be dragged into the inventory to buy. Valid
drops show green with a release label; outside or cancelled drops do nothing.
Stable vendor-stock identities and a transaction gate protect against stock
shifts, stale selections and repeated releases. Existing transaction checks
remain in the authoritative command path.

The native application and selected test targets build successfully. All 29
selected regressions pass with local game data configured; no tests skipped.
The isolated headless vendor flow is registered but cannot be exercised via
the unavailable devctl channel in this environment; physical touch trading
and notification appearance still need device validation.

## Stamina and Belt controls (2026-11-18)

The visible touch stamina bar toggles Run/Walk on an owned, completed tap.
Its 56-unit-high target extends into the clear space below the bar without
covering XP or the quest card. Rendering and hit testing share the bar's
geometry. Movement fingers remain independent; dragging out, cancellation,
panel interruption and focus loss cannot complete an old tap. The action
uses the existing Run command and touch selection-sound path.

Belt now has a cached belt/buckle glyph, retaining its Belt/Close caption and
expanded state. The native application builds and all five selected CMake
regressions pass (input, glyph, feedback, UI sound and vendor). Physical tap
comfort and appearance still need a phone check.

## Baseline and delivery

- Fork `Gradipoo/d2decomped` main was fast-forwarded from `6fadc09c` to
  upstream `895f8a67` (1,093 commits, zero fork-only commits). GitHub's branch
  head was checked after the update. Terminal Git metadata writes are denied
  in this session, so local main and remote-tracking refs were not updated.
- The inspected checkout is clean at `8cac5872` on
  `cursor/ios-simulator-rebase-1314`. Implementation must reconcile it with
  current main first; syncing main does not rebase a feature branch.
- Continue the existing [fork PR #3](https://github.com/Gradipoo/d2decomped/pull/3)
  for overlapping touch files, in individually reviewable commits with tests.
  Its first 100 changed-file results already include the touch/input/drawing
  files. Do not create another PR touching those files while it is open.
- [Upstream #456](https://github.com/psi29a/d2decomped/pull/456), also yours,
  overlaps iOS platform/input/view files. Keep its platform scope coordinated.
  [#598](https://github.com/psi29a/d2decomped/pull/598) touches targeting and
  commands; [#583](https://github.com/psi29a/d2decomped/pull/583) touches missile
  targets. Recheck their status before editing those functions and coordinate
  or wait rather than duplicating their fixes. Recheck all open PR file lists
  before implementation; this is not a complete conflict audit.
- Leave `docs/PLAN.md` alone. No dependency or platform framework additions
  are proposed. Publishing, replies and subsequent pushes remain separate
  outward actions.

## What the code currently shows

| Area | Observed implementation | Consequence |
|---|---|---|
| Layout | `apps/d2d/touch_input.hpp` anchors boxes to 800×600. | Screen-edge support needs a viewport-aware layout. |
| Rendering/input | `Town::draw` in `town/draw.cpp` draws touch into the centered 800-wide UI buffer; `main.cpp` subtracts the world viewport offset from finger positions. | Updating drawing alone would leave misplaced hit targets. |
| Spell art | `town/touch.cpp` blits skill sprites inside circular rings without a circular image mask. | Mask the image as well as drawing a round button. |
| Belt | Four field buttons show belt columns; a field tap opens the existing item sheet. | Aggregated healing/mana actions must preserve belt consumption rules. |
| Movement | The movement rings draw whenever field controls draw. | Hide their visuals when no movement finger owns the pad. |
| Targeting | `Town::pad_aim` uses ranged acquisition/sticky distances of 12/14 tiles; `pad::Aim` defaults to 2.5/4. | Tune acquisition separately from actual attack reach. |
| Teleport | `Town::resolve_skill` already applies drag distance, room restrictions and free-spot checks. | Extract/reuse that resolution for both devices and the marker. |
| Self-centered skills | This checkout returns early for Nova; War Cry's early return is conditional on `!manual`. | Dragging can take a different path; reproduce before assigning a cause. |
| Effects | `world/view.cpp` exposes buff skill IDs and shrine code, without remaining duration in those fields. | Accurate timers and dismissal need authoritative state support. |

These are source observations, not new traces against game.exe. Exact
Diablo Immortal acquisition distances have not been established.

## Intended interactions

### 1. Full-width layout and circular icons

- Give the touch overlay its own safe viewport: full drawable logical width,
  height, scale and platform safe insets. Keep legacy panel coordinates
  separate and convert between spaces at explicit boundaries.
- Draw and hit-test field controls using the same layout. Account for notches,
  home indicators, rotation, resizing and display scaling. Keep the middle
  of the world clear; fill side gutters with usable controls rather than
  stretching sprites or changing gameplay distances.
- Clip spell sprites to a circular interior with enough inset to preserve the
  gold rim and recognizable spell art. Use the same renderer for Attack,
  assigned skills and assignment previews. Avoid rebuilding masks per frame.
- Add clear pictograms in circular buttons for Inventory, Skills, Quest,
  Bind, Map and More (`…`). Preserve labels in tooltips/help and unambiguous
  active, pressed and disabled states. Use locally loaded game art where
  suitable or project-drawn glyphs; do not add bundled Blizzard assets.
- Hide the movement stick after its finger releases or cancels, with a short
  fade. Its activation area stays available. It appears immediately on press,
  and never remains stuck after focus loss, panels or a device handoff.

### 2. Potion pair and compact original orbs

- Replace four field belt buttons with Healing and Mana, each with the total
  remaining count of that type in the belt, across all rows and columns.
  Inventory reserves do not contribute. Rejuvenation remains a distinct type
  accessible through More → expanded belt; neither action silently uses it.
- Place both just left of the leftmost spell button. Put Healing at the lower,
  nearer part of the right thumb's natural arc, with Mana above it. Keep a
  visible gap from the spell arc and the bottom system-gesture region.
  On narrow screens, rearrange the cluster rather than overlap movement.
- Healing has a slightly larger target; use a deliberate press-and-release
  inside the button. A press that starts on a spell, a drag across Healing,
  or an OS Cancel cannot drink a potion. One completed tap consumes at most
  one potion; no hold repeat. A fast emergency tap needs no confirmation.
- Choose deterministically among usable matching belt slots: strongest
  available top potion, then stable slot order. Use existing belt commands
  and shifting rules; do not consume an inaccessible deep item directly.
  Revalidate at dispatch and update the count from authoritative state.
  Show zero as disabled, without switching potion type.
- Restore the original fluid/glass orbs as a small **paired orb locket** below
  the top-left menu cluster: two complete miniature globes in a shared thin
  frame, without the large statues or bottom panel. Start around 44–48 logical
  units per globe, then tune on hardware. Keep them read-only, away from
  potion taps. Put effects below them and reserve space for portraits/quests.
- Preserve fluid fill, poison/status treatment and readable optional numeric
  values. A brief low-resource rim pulse should remain readable without audio
  or color alone. Reuse locally loaded art and existing HUD state semantics.

### 3. Explicit selection among nearby loot

- Use a shared eligible-item candidate list with stable item IDs and names.
  One nearby item retains quick pickup. Two or more open the chooser on the
  dedicated pickup/interact action; ordinary held attacks never open it.
- Gamepad: open a radial with no sector selected initially. Flick the aiming
  stick into a sector and return/release the stick to neutral to commit once.
  Use an outer selection threshold, inner neutral threshold and angular
  hysteresis so stick noise cannot choose a neighbor. Center without a prior
  selection, Back, focus loss or device handoff cancels.
- Freeze sector order while open. Revalidate the chosen ID on commit; an item
  disappearing must never select the replacement occupying its old index.
  Show item name and quality with a clear focused sector. Page dense piles
  rather than make sectors too small; make paging explicit and distinct from
  returning the stick to neutral to commit.
- Touch: show a well-spaced vertical list, not a radial. Use comfortable rows
  (start at 48 logical units), full names, quality cues and scrolling for long
  piles. A row press/release picks that item; a scroll cannot pick it. Put the
  list above the thumb, keep it inside safe bounds, and provide Close.
- Chooser input captures aiming/action input until closed and cannot leak
  into a cast or ground tap. Do not pause multiplayer simulation. Handle
  movement, death, inventory opening, full inventory and out-of-range items
  through existing command rules with clear failure feedback.

### 4. Shared teleport aiming and target assistance

- Share a resolved teleport destination between preview and command creation.
  Touch drag direction and distance move the landing point; gamepad right
  stick direction and magnitude do the same while the teleport action is
  held. Preserve partial stick magnitude rather than normalizing it away.
- Show a small ground-projected ring at the resolved landing tile. Indicate
  invalid positions using a different shape plus color. If collision moves
  the landing point, the marker follows the actual resolved point.
- Release the spell action to cast once at the displayed destination.
  Cancel, focus loss, death and panel/device changes clear the preview.
  Keep the existing tap behavior with a predictable default distance.
  Recheck destination validity on release; reject or refresh a stale preview
  instead of silently teleporting somewhere unmarked.
- Keep game.exe teleport restrictions, mana and skill/charge rules intact.
  An original remote host may resolve against newer state; describe the ring
  as a prediction there, not a guaranteed server outcome.
- Increase touch/gamepad enemy acquisition slightly. Start playtests with
  about 10–15% more acquisition range, not as a claimed Immortal measurement.
  Tune melee, ranged, corpse and interaction policies separately and preserve
  directional preference and stable target retention.
- Longer acquisition must not increase weapon reach or spell damage radius.
  Melee still uses normal approach/attack behavior. Preserve target cues,
  skill-specific eligibility and authoritative obstruction checks. Avoid
  selecting targets beyond the visible gameplay area just because the display
  is wide. Keep mouse targeting unchanged.

### 5. Howl and other self-centered casts

- Reproduce the report with mouse, gamepad, touch tap and touch drag, both
  with and without an enemy selected. Check current main and the feature
  branch before deciding whether this is resolver routing, missile direction,
  repeated dispatch, or several issues.
- Classify skills by targeting policy: Self, Enemy, Corpse, Ground or
  Direction. Self-centered skills must ignore selected enemy and manual aim;
  a drag cannot turn a radial cast into a targeted volley.
- Check both cast routing and projectile creation. Howl intentionally emits
  multiple radial missiles; the regression is their placement/direction or
  extra casts, not merely the existence of multiple missiles.
- Existing research notes describe Howl as do-22 nova behavior and discuss
  war cries under `docs/research/re/skills.md`. Treat those as leads until
  rechecked locally against 1.14d. Include a reproducible oracle with any
  original-behavior claim, and coordinate with upstream #583 before touching
  the missile implementation.

### 6. Buff column and action feedback

- Display active buffs, auras and shrine effects down the left safe edge,
  below menu/orbs and clear of movement. Keep stable ordering and scroll or
  compact overflow instead of covering the field. Hold an icon to show its
  name, description and authoritative remaining time when available.
- A double tap on a removable beneficial effect opens a confirmation naming
  it. Confirmation removes that exact effect once. Single tap/hold, an expired
  icon, list movement or cancellation never dismisses anything.
- Audit existing dismissal commands first. Where removal is new gameplay,
  document it as an intentional deviation behind a toggle and route it through
  the authoritative command layer with effect ID/instance validation. Derive
  stats and expiry normally; do not just hide the icon. Curses and effects
  owned by another source must not become freely removable by accident.
  Unsupported remote-host removal is shown as unavailable, with an explanation.
- Add visual pressed, aiming, accepted, cancelled and rejected states to
  spells and buttons. Confirmed execution is distinct from merely pressing.
  Show reasons such as insufficient mana, empty belt and invalid target.
- Use existing suitable UI/game sounds and short optional haptic pulses where
  the platform supports them. Separate visual, sound and haptic preferences;
  unsupported haptics are a silent no-op. Rate-limit repeat feedback so held
  actions do not buzz or play clicks each frame. No audio/haptic-only message.

## Implementation order and checks

| Phase | Deliverable | Required verification |
|---|---|---|
| 0 | Reconcile the existing feature branch with synced main; recheck overlapping PRs; capture reproductions. | Baseline tests on the reconciled branch and screenshots on phone/tablet. |
| 1 | Shared full-width layout, coordinate conversions, circular art/glyphs, hidden idle stick. | Hit-test/render alignment at 4:3, 16:9, tall-phone landscape and ultrawide; safe insets, rotation, high DPI, panels and device switching. |
| 2 | Aggregated potion buttons and paired orb locket. | Mixed belt rows/tiers, counts, empty belt, shifting, multiplayer updates, disabled state and rapid taps; no accidental drink during spell drags. |
| 3 | Skill targeting policies and diagnosed Howl fix. | Touch tap/drag, gamepad and mouse; single dispatch, self-centered radial directions, charged/item skills; oracle for traced behavior. |
| 4 | Shared teleport ring and calibrated acquisition assistance. | Partial/full drag and stick, walls, restricted areas, blocked/changed destinations, mana/charges, cancellation, moving camera and host joins. |
| 5 | Gamepad loot radial and touch loot list. | One/many/dense drops, stick noise, selection release, paging, scroll cancellation, item disappearance, full inventory and no combat fallthrough. |
| 6 | Buff inspection/dismissal and integrated feedback. | Hold versus double tap, confirmation, effect expiry/replacement, stat recalculation, unsupported hosts, disabled haptics and feedback throttling. |

Extend existing `tests/test_touch_input.cpp`, `tests/test_gamepad.cpp` and
skillpad tests only alongside the behavior they check. Add dedicated
`test_*.cpp` files with `// test-links:` for new layout, belt aggregation,
loot chooser, spell policy, teleport preview and effect interactions as
needed. Favor state-transition and command-output tests over screenshots
alone. Keep new touch helpers in their subsystem and split responsibilities
out of `town/touch.cpp` before it grows further.

Run focused tests after each implementation commit, then
`python3 tools/pr_guard.py` and `python3 tools/presubmit.py --quick` before
publishing; run the full suite when a build exists. Device tests must include
simultaneous movement/casting, emergency Healing reach, drag-across-potion,
focus/background cancellation, handedness/reach assessment and sustained
combat feedback. Simulators do not validate thumb comfort or physical haptics.
No build, download, emulator run or game.exe trace was performed for this plan.

## Android reference audit

Read the [project README](https://github.com/N1v0k/D2Android-Releases),
[changelog](https://github.com/N1v0k/D2Android-Releases/blob/main/CHANGELOG.md)
and selected public source. Its architecture runs 1.13d through an Android
runtime/mod; d2d's native SDL client should use its own commands and rules.

| Reference under `source/winlator/app/src/main/java/com/winlator/d2/` | Useful idea | Adaptation here |
|---|---|---|
| `D2TeleportAim.java`, `D2TeleportMarker.java` | Valid/corrected/invalid destination and preview freshness. | Reuse native resolver results; do not add its mod packet bridge. |
| `D2Effects.java` | Stable effect ordering, shrine art and effect descriptions. | Use d2d authoritative state and safe left-edge layout. |
| `D2Hud.java` | Reuse local game globe/glass artwork and potion sprites. | Two small read-only original orbs rather than a full bottom panel. |
| `D2SkillTargets.java` | Distinguish aimable skills from passive/aura/corpse skills. | Use native skill metadata and verified policies; its 1.13d offsets are not d2d evidence. |
| `D2PadTarget.java` | Directional candidate ranking and target retention. | Tune shared acquisition without altering combat reach. |
| README/changelog pickup descriptions | Explicit selection among nearby targets. | User-requested stick-release radial on gamepad and spacious list on touch; its documented controller selection uses a different confirmation. |

The source README identifies the project's own app/mod code as MIT and
separates Winlator/third-party licenses. This plan copies no implementation
or assets. If implementation later reuses code, retain its applicable notices
and verify the individual file's license. Nothing from this reference is
evidence of stock Diablo II 1.14d behavior.
