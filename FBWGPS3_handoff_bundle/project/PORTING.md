## Progress tracker

| Step | Status | Verification / notes |
|---|---|---|
| 0.1 — Restore/check required assets | PARTIAL | The supplied `game/assets` tree contains four font PNGs and 23 MP3 files. The original shared BMFont descriptor `font.fnt` is absent; the runtime asset-reference audit and ten additional sound references listed in the recovery notes still need reconciliation. |
| 0.2 — Inventory `[PS3-PORT]` markers | DOCUMENTED; count discrepancy remains | `NOTES.md` records the 34 annotated comments and source lines. The 35th raw occurrence is the header description of the marker syntax. |
| 0.3 — Generate level table | COMPLETE | `LEVELS.md` generated from the supplied `game/data/forest/temple.json`; exactly 32 entries. |
| 1.4 — CMake/C++17 core scaffold | COMPLETE (engine-neutral; Linux Release compiled) | Fresh GNU 14.2.0 Release build succeeds; CTest covers input, maps, assets, rendering and character helpers. SDL2/RSX display and Box2D runtime are not wired in yet. |
| 1.5 — Load level data into plain structs | COMPLETE (initial implementation) | `test_level` loaded all 32 Tiled maps using each map's own tileset `firstgid` values. |
| 1.6 — TexturePacker atlas metadata loader | COMPLETE | `src/atlas.hpp` parses frame rectangles, trim/source sizes, rotation flags, image name and dimensions; frame names are indexed for lookup. |
| 1.7 — Atlas integrity checks | COMPLETE | `test_atlas` validates all 8 atlas JSON files, every frame rectangle, duplicate names, PNG signatures and PNG dimensions: 984 frames validated. |
| 1.8 — BMFont descriptor parser | PARTIAL | `src/bitmap_font.hpp` parses BMFont text metrics, pages, glyph rectangles and kerning; fixture test passes. Real `font.fnt` is missing from the supplied archive, so validation against game metrics and all four PNGs remains blocked. |
| 1.9 — SDL2/Box2D development dependencies | BLOCKED BY SDK HEADERS | SDL2 and Box2D runtime shared libraries are present, but their development headers/pkg-config metadata are absent; SDL2_image is also absent. The C++ core and PNG CPU renderer therefore build, but no native SDL2/Box2D adapter can yet be compiled. |
| 1.10 — Engine-neutral ground render commands | COMPLETE (render plan only) | `src/render_queue.hpp` converts every Ground `DrawOp` into a texture/source/destination command, preserving the original canvas placement formula. Verified across all 32 levels: 12,243 commands. Type-0 tile variants use a coordinate hash for stable output rather than the browser's per-draw random choice. |
| 1.11 — Liquid surface render commands | COMPLETE (render plan only) | Builds left/middle/right WaterBox, FireBox and GreenBox sprites; selects the original 15-frame, 30 fps animation and supports frozen IceBox surfaces (including initial type 55). Verified on every liquid run in all 32 levels: 740 sprite commands. Dynamic freeze/melt state is not connected to gameplay yet. |
| 1.12 — Generic sprite transforms and layer ordering | COMPLETE (engine-neutral) | `appendFrame` handles atlas trim offsets, original untrimmed dimensions, scale, pivot, rotation, and horizontal/vertical flips. Rotated-atlas destination dimensions use the unrotated source size. Render layers are stable-sorted: background, ground, liquids, devices, characters, effects, UI. |
| 1.13 — CPU RGBA renderer and PNG output | COMPLETE (software preview) | `src/software_renderer.*` decodes atlas PNGs through libpng, samples TexturePacker regions, applies alpha blending/rotation/flips, composites the tiled temple background with the ground alpha mask, and writes PNG output. Pixel-level tests verify sampling, alpha, flip and rotation; all 32 level maps export to PNG previews. This is an offline CPU renderer, not an interactive window or GPU backend. |
| 1.14 — Static device and character sprite placement | COMPLETE (initial states) | `src/world_render.hpp` maps all object types present in all 32 maps. Verification: 64 character objects, 64 doors, 329 diamonds, 261 devices, zero unsupported object types. Dynamic transforms and mechanism states remain unimplemented. |
| 1.15 — Character animation state and frame selection | COMPLETE (engine-neutral) | `src/character_animation.hpp` mirrors the state-selection thresholds in `init.js` for idle/jumping/rising/falling, left/right facing, head rotation, running legs, and ice overlay cadence. Fireboy/Watergirl animation frame selection is atlas-validated and unit-tested. It consumes a movement snapshot; live physics is not connected. |
| 1.16 — Engine-neutral character movement controller | COMPLETE (Box2D command adapter pending) | `src/character_controller.hpp` mirrors the run-force ramp, air control, jump impulse/velocity, slope jump, jump hold lock, per-character ice friction/speed and absolute speed clamp. It emits ordered `StepCommand` values rather than calling Box2D; tests cover normal ground, slopes, air, ice and input validation. |
| 1.17 — SDL2 desktop window / PS3 RSX graphics backend | NOT STARTED / SDK BLOCKED | The CPU renderer can write preview PNGs, but there is no interactive game window, SDL texture upload, RSX GPU submission, camera loop, or controller-driven live rendering. SDL2 runtime is present, but dev headers/metadata are missing. |
| 1.18 — Fallback terrain physics prototype | PARTIAL (engine-neutral approximation) | `src/physics_world.hpp` adds deterministic gravity, acceleration, jump, horizontal/vertical terrain resolution and substeps against the existing convex ground polygons. It is explicitly an AABB-based fallback, not Box2D parity; it is not yet wired to character animation, liquid/diamond/door sensors, or an interactive renderer. Box2D development headers remain unavailable in this environment. |
| 1.19 — Dynamic device/mechanism simulation | NOT STARTED | Static device art is mapped; switches, pushers, mirrors, platforms, pulleys, portals, and their connected state changes still need runtime logic. |
| 1.20 — Menus, audio and level-type effects | NOT STARTED | UI navigation, sound playback, dark-level lighting, and runtime speed timer remain future work. |
| 1.21 — Level result / star calculation | COMPLETE (engine-neutral) | `src/progression.hpp` mirrors the original result rules for normal/speed, puzzle and dark levels, including the single-player/mobile timer limit. Unit tests cover 1/2/3-star cases and puzzle silver-diamond requirement. |
| 1.22 — Best-result merge and tree unlock states | COMPLETE (engine-neutral) | Keeps only positive-star results as best, compares stars then diamonds then elapsed time, tracks played levels, totals stars, and unlocks adjacent Forest Temple nodes from completed nodes. Menu-node `_id` values are kept separate from public level IDs. Verified against the supplied catalogue and its 31 graph edges. |
| 1.23 — Render audit for full catalogue | COMPLETE (visual audit) | Software-rendered previews for all 32 levels succeeded. Contact sheet: `docs/forest_temple_contact_sheet.jpg`. This verifies static composition only, not dynamic mechanism states or gameplay. |
| 1.24 — Progress save/load | COMPLETE (engine-neutral file format) | `src/progress_save.hpp` stores version, game ID, played flags, and best run per public level ID. Loading rejects mismatched game IDs, duplicate IDs, unsupported versions and invalid statistics. Save writes a temporary file and replaces the destination; repeat-save and round-trip tests pass. Game startup/shutdown integration and platform-specific save location remain pending. |
| 1.25 — Contact event routing and timer accuracy | COMPLETE (engine-neutral adapter contract) | `src/contact_router.hpp` routes physics-backend begin/end events for liquid death sensors, ice surfaces, diamonds and doors into `GameplayRuntime`, returning explicit errors for invalid targets. The session clock now accumulates the full frame delta for scoring while animation updates remain capped at 40 ms. Regression tests cover event dispatch and a 250 ms frame. This is an event adapter, not collision detection or a physics simulation. |
| 1.26 — Integrated game-world simulation | PARTIAL (first end-to-end simulation layer) | `src/game_world.hpp` starts a catalogue level, creates Fireboy/Watergirl bodies, steps fallback physics, detects approximate diamond/door/liquid overlaps, advances death timers, and updates level outcome/progression. Unfrozen liquid fixtures are tagged as sensors and excluded from solid collision; type 55 frozen water remains solid. `test_game_world` verifies body creation, movement and timer integration. Not yet rendered live; sensors and physics remain approximate. |

# Porting brief: Fireboy & Watergirl - Forest Temple -> PS3

Personal, offline port of a web game (Phaser CE + Box2D, AMD/RequireJS).
Original game: Oslo Albet / Agame. Keep the credits; do not redistribute.
All online hooks (igroutka loader/handshake, GameDistribution SDK, ads, parent `postMessage`) are already removed.
Annotated source: `game/js/init.js` - search for `[PS3-PORT]` (35 markers).

## 0. Reality check
There is no automatic JS -> PS3 "recompile". This is a **re-implementation** in C/C++ that reuses the original
assets and level data unchanged. Recommended order:
1. Desktop prototype (C++17, SDL2 + Box2D 2.3/2.4 + stb_image + cJSON/nlohmann) - verify gameplay parity.
2. Port the platform layer to PSL1GHT / ps3toolchain (RSX or software blit, ioPad, cellAudio, file IO).
Do NOT embed a JS engine + Phaser shim: Phaser needs Canvas/WebGL/DOM, not worth it on Cell.

## 1. Files (no conversion needed except audio)
| Path | Format | Native handling |
|---|---|---|
| `game/game.json` | JSON | id, temples:"forest" |
| `game/data/forest/temple.json` | JSON | 32 levels: id, x,y (0..1 menu pos), filename, time, mobileTime, required, type(speed/dark/puzzle) |
| `game/data/forest/levels/*.json`, `data/tutorials/levels/forest_01.json` | Tiled 1.1 JSON | orthogonal, 32x32 tiles; layers `Ground` (tilelayer), `Chars`, `Objects` (objectgroup) |
| `game/assets/atlasses/*.json+png` | TexturePacker JSON-ARRAY | frame lookup by name |
| `game/assets/atlasses/Temples/forest/TempleAssets.*` | same | level-specific tiles/sprites |
| `game/assets/fonts/font*.png` + `font.fnt` | BMFont (shared .fnt) | 4 fonts, same metrics, different PNG |
| `game/assets/audio/*.mp3` | MP3 | convert to OGG (stb_vorbis) or 16-bit PCM at build time |

## 2. Architecture map (see markers in init.js)
- `main` -> `States/Boot` -> `States/Load` -> `States/Menu/Menu` -> `LevelMenu` -> `States/Level/Level`.
- `Level`: loads Tiled map, builds `Ground` (static b2PolygonShape per tile + baked visuals), characters (layer `Chars`),
  devices (layer `Objects`, class chosen by `object.type` through `Devices/_define`), `MechManager` links devices.
- Physics: Box2D, PTM 32 px/m, `gravity.y = 1000` (set in Level.loadComplete). Step once per frame, dt = frame time.
- `States/Level/character`: Fireboy `fb`, Watergirl `wg`; custom run-force ramp, jump impulse, ice friction, liquid float.
  Port `update()` verbatim - it is the feel of the game.
- Level types: normal, `speed` (timer), `dark` (light mask around chars), `puzzle`.
- Save: `ProgressModel` (localStorage `progress_forest`) + settings (`settings`) -> native `progress_forest.json` under the game save directory; the versioned progress serializer is implemented, but platform path/startup/shutdown wiring remains pending.

## 3. Two-controller support (required)
Gameplay only reads `cursors.left/right/up.isDown`. Original desktop layout: **wg = W/A/D, fb = Arrow keys** (both
characters active simultaneously, i.e. the game is natively 2-player).
- Pad 1 -> one character, Pad 2 -> the other (let the player pick; default pad1=fb, pad2=wg).
  D-pad or left stick (deadzone ~0.35, hysteresis) = left/right; Cross (and Up) = jump. START = pause (`P` key).
- Hot-plug: poll `ioPadGetInfo2` every frame; if pad 2 is absent, fall back to **single-pad mode**: the game already has
  `controls="single"` with `CursorToggle` (switch active character) - bind to L1/R1.
- Menus are **pointer-only** (every `Button` uses inputEnabled/onInputDown). Add a focus-navigation layer
  (D-pad moves a highlight among visible Buttons, Cross = activate, Circle = Back) or a virtual cursor.
- Settings: hide `controls`, add `players: 1|2`.

## 4. Drop list
Ads/*, GDProvider, TouchManager/hammer, States/Buy/*, cordova/store code, WebGL filters (cosmetic - skip or pre-bake),
`debug-fps`, Backbone/Underscore/jQuery (replace with plain structs).

## 5. Platform constraints (PS3)
256 MB system RAM / 256 MB VRAM: all atlases (~2.4 MB PNG, decode to RGBA ~15-20 MB) fit comfortably.
Target 1280x720, 60 fps, fixed logical scale 1.0 for 32 px tiles. Use `cellAudio` mixer for ~10 simultaneous SFX + 1 music.
Pads: `libio` (ioPad). Save: `/dev_hdd0/game/<TITLEID>/USRDIR/`. Build with ps3toolchain + PSL1GHT, sign with `make_self`
for CFW/homebrew.

## 6. Known gaps / verify first
- Run `fetch_missing.ps1` once (font.fnt + 10 sounds were not in the original download).
- Dark-level light mask and light-beam devices (Beam, mirrors, Window, LightPusher) are the riskiest visual parts.
- Verify 2-player feel against the browser build side by side before touching PS3 code.

## 7. Engine-neutral rendering and animation work (current milestone)

`src/render_queue.hpp` is a platform-independent render command buffer. Each command records the atlas/image, source rectangle, destination rectangle, pivot, rotation, flip flags, layer and stable sequence number. `ordered()` provides stable layer ordering for a future SDL2 or PS3 RSX backend.

- `appendGround()` maps `GroundResult::draws` to `GroundAssets` frames and follows the original `Ground.drawTile` destination offset. The original randomly chooses one of four variants for type 0; this port currently chooses a deterministic coordinate hash so the same level renders consistently.
- `appendLiquids()` creates the left, middle and right sprites for each liquid run. It uses the original 30 fps / 15-frame animation and supports frozen `IceBox` sprites. Type 55 is treated as initially frozen, matching the source constructor. Runtime freeze/melt events are not implemented.
- `appendFrame()` is the common sprite transform path and accounts for TexturePacker trim offsets, source size, scale, pivot, rotation and flips.

The CPU renderer now consumes the draw commands and produces RGBA pixels/PNG previews. It is not an interactive graphics backend: there is still no SDL2 window, RSX submission, camera loop, or GPU texture upload. The command queue is verified on all 32 maps: 12,243 ground commands and 740 liquid sprite commands.


### CPU renderer and character animation

- `src/software_renderer.*` is a deterministic CPU RGBA renderer. It decodes PNG atlases using libpng, applies source sampling and alpha compositing, supports trim/rotation/flip transforms from the queue, composes the tiled `BackGround` with the `ForeGround` through the ground mask, and can save preview PNGs.
- `src/world_render.hpp` maps every Tiled character, door, diamond and device object type observed in the supplied 32 Forest Temple maps to atlas sprites. This is static art placement only; it does not simulate the devices.
- `src/character_animation.hpp` turns a physics/input snapshot into the original Fireboy/Watergirl animation selection. It covers idle, jumping, rising, falling, running legs, ice overlay, facing and head rotation. The helper is tested with the real `CharAssets` atlas. The snapshot is not yet supplied by a live physics world.
- `src/character_controller.hpp` mirrors the controller decisions in `Character.update()`: running-force ramp along contact normals, airborne float force, jump gating, impulse plus jump-velocity override, slope jumps, ice handling, and speed clamps. It returns engine-neutral `StepCommand` data for a future Box2D adapter.
- Current tests: 15/15 CTest cases pass, including pixel-level CPU renderer tests, character animation and movement, progression/save, gameplay contact rules, contact-event routing, level-session integration and a long-frame timer regression. `tests/check_spec.py` reports `ALL OK` against the annotated source.

Do not confuse the CPU PNG preview or engine-neutral movement commands with a playable build. SDL2 desktop rendering, PSL1GHT/RSX, Box2D runtime integration, actual collision generation, and dynamic device simulation are still pending. `src/contact_router.hpp` is only the callback contract between a future physics backend and the tested gameplay rules.


### Level results and progression

`src/progression.hpp` ports the original `ProgressModel.update()` scoring rules without introducing platform dependencies. Normal and speed levels award one base star, one for all diamonds, and one for meeting the configured time; puzzle levels award three stars only when at least one silver diamond is collected; dark levels award two stars, or three within the time limit. Single-player mode uses the alternate time limit when supplied. Best-result tie-breaking follows stars, then diamonds, then lower elapsed time. Tree progression states and adjacent-node unlocks are modelled, and `src/progression_catalog.hpp` derives scoring metadata from all 32 maps and preserves the catalogue’s separate menu-node IDs. `src/progress_save.hpp` implements validated versioned JSON save/load; wiring it into menu boot/exit and a platform-specific user-data path remains pending.

### Fallback physics prototype (step 1.26)

`src/physics_world.hpp` now supplies a minimal standalone simulation using screen-pixel coordinates, convex terrain polygons from `buildGround()`, a character AABB, gravity, run/air acceleration, jump, drag, speed caps and small movement substeps to reduce tunnelling. `tests/test_physics_world.cpp` verifies falling onto a platform, jumping and landing. This is a useful testable bridge while Box2D headers are unavailable, but it is deliberately marked as an approximation: it does not yet reproduce the source character’s compound fixtures, slopes accurately, device collisions, or sensors, and is not yet connected to the renderer/gameplay runtime.

### SDL2 desktop runtime prototype (step 1.27)

`src/game_main.cpp` adds an optional SDL2 interactive runtime when libpng and the SDL2 runtime library are found. It builds a textured static level image from the supplied atlases, opens a window, follows the active character with a camera, renders animated Fireboy/Watergirl head and leg atlas frames, feeds keyboard input into `GameWorld`, autosaves progress after an outcome, restarts the current level with `R`, advances to the next catalogue level with `Enter` after an outcome, and pauses with `Escape` (Enter resumes). Controls: Fireboy = arrows + Space; Watergirl = A/D/W (S is also accepted as jump in this prototype). This is a desktop integration milestone only: it is not PS3-targeted and does not yet implement full device mechanisms, menu UI, audio, correct single-player switching, or original Box2D parity. The fallback physics and overlap tests remain approximations.

The desktop runtime milestone supersedes the earlier renderer section's statement that no SDL2 window exists: `src/game_main.cpp` now provides a basic SDL2 window and keyboard/game-controller input, and `desktop_runtime_startup` plus `desktop_runtime_all_levels` validate startup/render-queue composition for the initial level and all 32 catalogue levels. This does not supersede the listed gameplay gaps; it is still a prototype rather than a complete game or PS3 build.

### Runtime interaction pass (step 1.28)

The SDL2 prototype now draws the level's object queue per frame instead of baking all objects into the static background. This lets platform and box positions update visually. The live world includes an initial moving-platform adapter: platform objects share activation groups with lever objects, touching a lever toggles its group, and linked platforms travel between their configured start/end offsets. Pushable boxes receive gravity and can be moved horizontally by either character; boxes and platform surfaces are added to the fallback collision fixtures. Wind areas apply a basic upward force, frozen-water contacts set the ice state, Watergirl gets the lower ice speed cap, and liquid surface frames animate at 30 fps. These mechanics are partial approximations and still require parity tests against the original game.

The fallback collision solver now resolves overlap along the minimum separating axis (including convex polygon edge normals), which improves slope contact versus axis-only rollback. Regression coverage includes flat-platform landing, jump/landing, a sloped fixture and non-solid unfrozen liquid.

The latest runtime pass also adds approximate collision fixtures for the 14 movable boxes (correction, 2026-10-31: the 12 balls have no body or collision; see `docs/MECHANISM_AUDIT.md`) and the hanging/pulley platforms and Roman balance beams. Balls currently use an AABB approximation, hanging/pulley platforms are static, and wind-effect sprites animate when their group is active. This closes several common traversal gaps but is not a substitute for the original joint-based simulation.

### Controller usability pass (step 1.29)

The SDL2 runtime now supports gamepad-only navigation for the main level menu and solo mode selection: D-pad up/down selects levels, A starts an unlocked level, X toggles solo/two-player mode, B exits from the menu or returns from gameplay to the menu, Start pauses/resumes, and Y switches the active hero in solo mode. The menu text reflects the additional gamepad controls. Returning to the menu through B writes progress immediately. This remains SDL2 desktop input; a PSL1GHT `ioPad` adapter still needs to map the same actions for PS3.

Validation after this change: 19/19 CTest cases pass, both desktop runtime validation modes pass for all 32 maps, and `tests/check_spec.py` reports `ALL OK`. This confirms loading and regression checks, not full gameplay parity or PS3 execution.


### Solo-camera correction (step 1.30)

The SDL2 camera now follows the selected hero in solo mode instead of averaging Fireboy and Watergirl positions. This matters when switching characters across a large map: the active character remains the camera focus. Two-player mode still centers between both characters. This is a camera usability fix, not a change to the physics or proof of single-player gameplay parity.

Validation: Release build succeeds, 19/19 CTest cases pass, all 32 levels pass desktop runtime validation, and `tests/check_spec.py` reports `ALL OK`. PS3 toolchain and target-hardware validation remain outstanding.


### Collision regression coverage (step 1.31)

Extended `tests/test_physics_world.cpp` with dedicated vertical-wall and ceiling cases. These fixtures disable gravity where necessary so the test isolates the collision axis instead of accidentally allowing a falling character to pass below a finite wall. The test asserts that sustained horizontal input cannot cross the wall, that horizontal velocity is cancelled, and that upward movement cannot pass through the ceiling. The complete Release build passes 19/19 CTest cases; `tests/check_spec.py` reports `ALL OK`. These tests strengthen the fallback solver's regression coverage but do not establish full Box2D parity.


### Collision edge-case regression pass (step 1.32)

Expanded `tests/test_physics_world.cpp` to cover crossing the seam between adjacent floor fixtures and vertical sliding while a character remains blocked by a wall. The tests assert that a character can traverse a joined floor without catching on the shared edge, and that horizontal collision does not cancel gravity-driven downward motion. Release build passes 19/19 CTest tests and `tests/check_spec.py` reports `ALL OK`. This remains fallback-physics regression coverage; the solver is still an AABB approximation and the PS3 build is not yet available.


### PS3 feasibility gate and pad conversion layer (step 1.33)

**Toolchain gate result (2026-10-31, cloud Linux workspace): BLOCKED.** No PS3 cross-compiler or SDK is present (`PS3DEV`/`PSL1GHT` unset, no `powerpc64-ps3-elf-gcc`). It also cannot be obtained there: `ps3toolchain` downloads its sources from gnu.org/sourceware, which the workspace network refuses, and PSL1GHT on GitHub returned HTTP 403. Docker registries are unreachable too. A PS3 binary must therefore be built on a machine with a working ps3toolchain/PSL1GHT install (the user's Windows PC would need WSL/Docker/MSYS2 for it) — no PS3 build exists and none was attempted past this gate.

Baseline rebuilt on GNU 13.3.0: Release build OK, CTest passed, `tests/check_spec.py` = ALL OK.

Added `src/ps3_pad.hpp`: converts a raw PS3 pad sample (digital1/digital2 bytes + 0..255 left-stick axes) into `fw::PadState` for the existing `fw::Input` layer; a disconnected pad yields a neutral state. `tests/test_ps3_pad.cpp` (CTest `ps3_pad_conversion`) covers the bit mapping, axis scaling, two-pad play, single-pad L1 character toggle, stick hysteresis, START/Cross/Circle menu events. **Caveat:** the digital bit constants were written from memory without the SDK headers; the tests prove the conversion logic, not that the constants match PSL1GHT. Check them against `io/pad.h` on the first real toolchain build. The pad glue itself (`ioPadInit`, `ioPadGetInfo2`, `ioPadGetData`) is not written.

Validation: 20/20 CTest cases pass, `tests/check_spec.py` = ALL OK. Neither shows that a PS3 build exists or runs.

### Mechanism audit (step 1.34)

`docs/MECHANISM_AUDIT.md` inventories all 261 devices by type and compares each with the port. New CTest `mechanism_inventory` (`tests/test_mechanism_inventory.cpp`) fixes the per-type counts. Main findings: balls (12, in 7 levels) are not simulated; the 12 rotating platforms (5 levels) are translated instead of swung; levers ignore `onis`/`startPos` and toggle on touch; hanging/pulley/Roman devices are static; seven tileset device classes are never used by these maps. These are code-versus-source differences, not results of playing the levels. 21/21 CTest cases pass and `tests/check_spec.py` = ALL OK.

### Rolling balls (step 1.35)

`PhysicsBody::radius > 0` turns a body into a circle in `src/physics_world.hpp`: circle-vs-convex-polygon resolution (edge normals plus the nearest-vertex axis), contacts remove only the velocity component into the surface, and gravity is scaled by 2/3 while the body rests on a surface (solid disc rolling without slipping). `GameWorld` now creates one such body per `ball` object (radius 12.8 px, spawn at x+16, y−11.2 as in the render plan), gives other bodies a tangent 12-gon to collide with, lets a character push it at up to 200 px/s only when pressing towards it, applies no drag (Box2D default), and tracks a roll angle that `game_main.cpp` feeds to the sprite. `tests/test_ball_physics.cpp` (CTest `rolling_balls`) covers floor rest, constant-speed rolling, wall stop, slope acceleration against (2/3)·g·sinθ·cosθ (3 % tolerance), 12 ball bodies across the catalogue staying inside their maps, and pushing with no-input and walk-away controls. A first version of the slope check compared against the along-slope acceleration instead of its horizontal projection and passed only through a loose tolerance; it was corrected. Not covered: restitution, slipping, wind and liquid on balls, and how the sprite looks on screen. 22/22 CTest cases pass and `tests/check_spec.py` = ALL OK.

### CPU frame composer, app state machine and PS3 platform layer (steps 1.36-1.38)

**1.36 `src/frame_composer.hpp`** builds one RGBA frame of the live `GameWorld` entirely in software (camera crop of the baked level, devices at live positions, doors, wind, animated liquids, heroes, dark-level mask, HUD, pause/result banner), using the new `SoftwareRenderer::drawQueueOnto()` and `RenderQueue::push()`. It replaces what the SDL runtime does through SDL textures, so a PS3 layer needs no GPU texture code. `tests/test_frame_composer.cpp` (CTest `frame_composer_all_levels`) composes all 32 levels at 1280x720 and checks each frame is not flat/empty; levels 1, 7, 15 were also written as PNG and looked at (`docs/frame_level_*.png`). All 32 levels compose in about 1.2 s total on the x86 development machine (35 ms/level including level bake and 30 simulated frames) - a PS3 PPU is unmeasured and may be much slower; the rotated per-pixel blit is not optimised.

**1.37 `src/app.hpp`** is the platform-neutral state machine (menu / playing / paused / result) driven only by `fw::Input`: D-pad Up/Down + Cross in the menu (locked levels refuse), two pads = both heroes, one pad = L1/R1/Square hero switch, START pause, Down = restart from pause, Circle = menu (saves), Cross on the result screen = next unlocked level / retry. It reuses the existing progression, session and save code. `tests/test_app_flow.cpp` (CTest `app_flow_pads`) drives it exclusively through simulated `ps3::RawPad` samples: menu navigation, locked-level refusal, start, active-hero-only movement in one-pad mode, L1 switch, pause freezing the world, resume, save file written on return to the menu, two-pad play and a 1280x720 render. It does not play any level to completion.

**1.38 `ps3/ps3_main.cpp` + `ps3/Makefile`** are the PSL1GHT glue: `ioPad` -> `PadState` -> `fw::Input`, `App::render()` frame -> XRGB buffer -> RSX flip, `sysUtil` exit callback, save path under USRDIR. **They have never been compiled or run.** There was no PS3 SDK and no console. Every PSL1GHT call was written from memory and is tagged `VERIFY`; expect compile fixes on the first build (rsxInit/gcm calls, `padData` field names, `padInfo.status`, sfo/pkg tool invocation, whether the toolchain GCC accepts `-std=gnu++17`). Plan: build with ps3toolchain + PSL1GHT, run first in RPCS3, only then on hardware.

Not done on the PS3 side: audio (no cellAudio code, MP3 -> OGG/PCM conversion not done), real fonts (still the 5x7 debug font; `font.fnt` exists in the repository-root `assets/fonts/` but is not wired in), PKG packaging verification, frame-rate measurement on the Cell, and any on-target test. Re-checking `ps3_pad.hpp` bit constants against `io/pad.h` is still pending (the glue avoids them by reading named `padData` fields, but that is also unverified).

Validation (g++ 13.3 built directly, cmake is not installed in this workspace so CTest itself was not run): all 18 original core tests, `test_software_renderer`, `render_level_preview`, `test_frame_composer`, `test_app_flow` and `tests/check_spec.py` (ALL OK) pass; `game_main.cpp` still compiles. The SDL runtime tests (`desktop_runtime_*`) were not executed.

### Progress estimate (2026-10-31)

These are engineering estimates, not measured completion metrics. They separate the desktop remake from the requested PS3 port:

- Original assets/maps/catalogue ingestion and preview rendering: **about 75%** (the supplied 32 maps and atlases load and render, but font/audio completeness and visual parity are not finished).
- Core gameplay/progression: **about 50%** (movement, basic contacts, doors, diamonds, liquids, saves and partial mechanisms exist; physics parity, puzzle/device behavior and full level validation remain).
- Desktop playable integration: **about 45%** (an SDL2 runtime exists with keyboard/gamepad input, camera, menu, pause and level transitions, but known mechanics/audio/UI gaps remain).
- PS3-specific port and release packaging: **about 5%** (portable core is intended to support it, but PSL1GHT/RSX/ioPad integration and a verified PS3 build are absent).
- Overall remake-to-PS3 deliverable: **estimated 35% complete**. This weighted estimate is intentionally conservative: the target deliverable is a tested, playable PS3 build, not just source scaffolding or desktop previews.

Validation snapshot: Release build succeeds, **19/19 CTest tests pass**, all 32 maps pass desktop validation, and `tests/check_spec.py` reports `ALL OK`. These checks do not demonstrate that all levels are completable or that the game runs on PS3 hardware.
