# Mechanism audit — Forest Temple (32 maps)

Snapshot 2026-10-31. Step 4 of `HANDOFF_FOR_NEXT_AGENT.md`. Counts below are locked by `tests/test_mechanism_inventory.cpp`
(CTest `mechanism_inventory`), which reads the maps through the project's own loader; the expected numbers were produced
independently by resolving Tiled gids through each map's tilesets in a separate script. "Port" columns describe
`src/game_world.hpp` and `src/world_render.hpp` as read on this date, not measured gameplay. Behaviour of the original is
taken from `game/js/init.js` where the notes say so; items marked *not examined* were not read in detail.

Level numbers in this document are map file numbers (`forest_NN.json`). Totals include the tutorial map
(`data/tutorials/levels/forest_01.json`), which is catalogue entry 1.

## Object inventory

Chars layer: 64 characters, 64 doors, 329 diamonds (plus one zero-size, gid-less object in `forest_05`, ignored).
Objects layer: 261 devices.

| Type | Count | Levels | Original (init.js) | Port: draws | Port: simulates | Gap |
|---|---:|---:|---|---|---|---|
| platform, translating | 86 | — | Box2D body on a prismatic joint, moves `dx`,`dy` tiles when its group is on | yes | yes, straight-line move at 180 px/s, carries a character standing on it | speed/easing and joint limits not matched |
| platform, rotating (`dtheta`) | 12 | 5 | Box2D body on a **revolute** joint, swings `dtheta` degrees (90) about a pivot offset by `offsetY` | yes (unrotated) | **no — treated as translating** | see below |
| lever | 49 | 22 | Physical stick on a revolute joint with a motor; state flips when the stick angle crosses zero in the `onis` direction; pushed by a character | yes | partial — state toggles on each *new overlap*, regardless of push direction | `onis`/`startPos` (tileset defaults, every lever) unused; no physical stick |
| pusher (pressure plate) | 47 | 20 | physical plate, group active while pressed | yes | partial — solid fixture + overlap test | not matched to joint travel |
| box | 14 | 9 | dynamic Box2D body | yes | yes — AABB body, gravity, pushed by characters | AABB, not original shape/friction |
| ball | 12 | 7 | dynamic circular Box2D body (r 12.8 px, density 2.3, friction 0.2, restitution 0.1, no damping) | yes, rolls (visual angle) | yes since step 1.35 — circle body, rolls on slopes at (2/3)·g·sinθ, keeps speed on flat ground, pushed by characters up to a walking pace | no restitution, no slipping, wind/liquid ignore it, push is a fixed acceleration; sprite sync not checked on screen |
| wind | 24 | 10 | air column, group-controlled, `length`, `windWidth`, `initialState` | yes (animated) | partial — lift force on characters only | boxes/balls unaffected; force curve is invented |
| hanging | 8 | 4 | swinging platform on joints; 6 have `fullRotation` | yes | static solid pad only | joint-based motion absent (*joint details not examined*) |
| pulley | 6 | 6 | two platforms coupled by a rope | yes | two static solid pads | counterweight motion absent (*not examined*) |
| roman (balance beam) | 3 | 2 | balance beam | yes | one static slanted pad | tilting absent (*not examined*) |

Device classes that exist in the tileset but appear in **none** of the 32 maps: `beamer`, `rotmirror`, `lightpusher`,
`boxmirror`, `boxheavy`, `infinitemirror`, `timedpusher`. `PORTING.md` calls the light-beam devices the riskiest visual
part; for this temple they are not needed, so they can be skipped until another temple is ported.

## Findings that contradict earlier documentation

1. **Balls were not simulated — fixed in step 1.35.** `README.md` and `PORTING.md` claimed collision for "the 14 movable
   box/ball objects". The 14 are the *boxes*; `GameWorld::start()` only created bodies for `type == "box"`, so the 12 balls
   were drawn but characters walked through them. They now have circle bodies (levels 07, 08, 19, 20, 27, 29, 32;
   `tests/test_ball_physics.cpp`). Related older quirk, not fixed: a *box* is pushed by any key press from a character
   standing next to it on either side, so walking away from a box drags it a few pixels; balls only react to a push towards them.
2. **Rotating platforms move the wrong way.** The loader fills platform defaults `dx=4, dy=0`, and the port always
   translates. In the original, any platform with `dtheta` is a revolute joint and swings instead. So in levels 07 and 18
   (7 platforms with no `dx`/`dy` of their own) the port slides them 4 tiles sideways; in 27, 30 and 31 (5 platforms with
   `dy` of ±3/±4) it slides them vertically. Affected levels: 07 (4), 18 (3), 27 (2), 30 (2), 31 (1).
3. **Levers are touch-toggles, not push-levers.** Every lever carries `onis`/`startPos` from its tileset variant (tile 1:
   `onis=1, startPos=-1`; tile 2: `onis=-1, startPos=1`); the port ignores both. A lever that should need a push from one
   side can be flipped from either side, and its initial state is always off.
4. **Hanging, pulley and Roman devices are static.** Hanging in 02, 12, 19, 25; pulley in 04, 05, 21, 24, 29, 30; Roman in
   14, 24. Any puzzle that depends on their motion cannot currently be solved.

None of this has been checked by playing the levels; these are differences between the source and the port's code and
data. Which levels are actually blocked needs a per-level playability check.

## Suggested order of work

1. ~~Balls~~ — done (step 1.35). Still worth doing: look at a ball on screen (no screenshot hook exists in the runtime),
   decide whether wind and liquids should act on balls and boxes, and fix the box push rule.
2. Rotating platforms: swing `dtheta` degrees about the `offsetY` pivot instead of translating; the fallback solver
   already takes convex polygons, so a rotated rectangle per frame is feasible; carrying a standing character is the hard part.
3. Levers: honour `onis`/`startPos` and the push direction.
4. Hanging / pulley / Roman: read the joint setup in `init.js` first, then decide between a small dedicated model and a
   real Box2D adapter.
5. Per-level playability check once the above exist.
