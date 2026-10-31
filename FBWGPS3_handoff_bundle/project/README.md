# Fireboy & Watergirl — Forest Temple native port base

This repository is an early C++17 re-implementation scaffold for an offline PS3 port. It uses the supplied Tiled level data and original art assets; it does not embed Phaser or a JavaScript runtime.

## Current state

- Loads the Forest Temple catalogue and all 32 Tiled maps.
- Builds ground collision fixtures and engine-neutral ground draw operations.
- Parses all eight supplied TexturePacker JSON atlases and validates all 984 frames against PNG dimensions.
- Parses BMFont descriptors. The parser is tested with a fixture because the original `font.fnt` descriptor is missing from the supplied archive.
- Builds an engine-neutral render queue for ground, animated liquid surfaces, sprite transforms, devices, characters, doors and diamonds.
- Includes a CPU RGBA renderer that decodes PNG atlases, applies alpha blending/rotation/flips, composites the ground mask over the temple background and writes preview PNGs.
- Includes engine-neutral Fireboy/Watergirl animation selection for idle, jumping, rising, falling, directional facing and ice overlays.
- Includes an engine-neutral character movement controller that outputs Box2D operations for the run-force ramp, air movement, jump impulse/velocity, slopes, ice friction and speed limits.
- Implements original level-result scoring for normal/speed, puzzle and dark levels, best-result comparison, total stars, played flags, Forest Temple tree unlock states, and validated versioned JSON save/load.
- Software-rendered previews have been generated and visually audited for all 32 levels; see `docs/forest_temple_contact_sheet.jpg`.
- Provides two-controller input-state logic and physics constants cross-checked against `game/js/init.js`.
- Adds a deterministic fallback terrain-physics prototype with gravity, running, jumping, platform collision and substeps. Unfrozen liquid collision fixtures are treated as sensors rather than solid platforms. Regression tests also cover vertical walls, ceilings, adjacent-floor seams, and sliding down a wall under gravity.
- Adds `src/game_world.hpp`, which links level startup, two character bodies, approximate physics, diamond and door overlap checks, liquid hazards, death timing and the level session in one simulation update. This is a first integration layer, not a finished playable game.

The current map/object rendering audit covers all 32 maps: 12,243 ground commands, 740 liquid-surface sprites, 64 character objects, 64 doors, 329 diamonds and 261 device objects. No object type present in those maps is currently left unmapped in the static object-render pass.

**This is not yet a playable game.** There is no interactive SDL2 window or PS3 RSX backend, no Box2D runtime world or integrated live character physics, and no dynamic device logic, menus, menu-integrated progression/save handling, or audio playback. Character animation and movement calculations are engine-neutral helpers: they consume contact/velocity snapshots and emit physics commands, but are not yet connected to a simulated character.

## Rendering

`src/render_queue.hpp` contains the backend-neutral draw list. `src/world_render.hpp` adds the static art for Tiled objects. `src/character_animation.hpp` resolves character animation state and atlas frames. `src/character_controller.hpp` produces engine-neutral physics commands based on the original controller logic. `src/progression_catalog.hpp` loads the complete 32-level catalogue, derives diamond totals from maps, and maps menu graph node IDs correctly; `src/progress_save.hpp` handles versioned progress JSON and save/load validation. `src/software_renderer.*` can consume the queue and export a PNG preview using libpng; it is useful for visual validation but is not a replacement for the future SDL2 or PS3 RSX runtime renderer.

## Build and test

The core is C++17. CMake and a C++17 compiler are required; libpng development files are additionally required to build the CPU preview renderer (CMake keeps that target optional when PNG is unavailable). SDL2 and Box2D runtime libraries exist in the current build environment, but their development headers are not installed, and SDL2_image is absent.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 tests/check_spec.py
```

`compiled/linux-x86_64/` contains Linux x86-64 Release tools from this development snapshot, including progression scoring, catalogue, and save/load tests. They are not Windows or PS3 executables.

## Attribution and scope

Original game by Oslo Albet / Agame. Preserve the original credits. This project is intended as a personal offline porting effort; do not redistribute the original game assets.

## Level session integration

`src/level_session.hpp` joins the level catalogue, Tiled loader, ground builder, engine-neutral gameplay state, timer, star calculation, best-result merge, and save-file writer. It is an integration layer only: until a physics backend feeds sensor contacts, it cannot run a playable character simulation by itself.

### Gameplay adapter progress

`src/contact_router.hpp` defines the engine-neutral event boundary for a future physics backend: liquid-death, ice, diamond, and door contacts are routed to the tested gameplay state layer. Level scoring time now counts the full frame delta even when animation updates clamp to 40 ms; regression tests cover a 250 ms frame. This still does not create physical contacts or make the port playable.

### Integrated game-world prototype

`src/physics_world.hpp` provides an engine-neutral, approximate AABB character body against convex ground fixtures. `src/game_world.hpp` now connects that simulation to level startup and gameplay runtime state: movement, approximate diamond/door overlaps, liquid hazards, death timing and session outcomes. Unfrozen liquid fixtures are excluded from solid collision while type 55 frozen water remains solid. This remains a prototype: slopes, compound character fixtures, devices and visual/input runtime are not at original-game parity.

## Interactive desktop prototype (new)

If CMake detects libpng and the SDL2 runtime library, it also builds `build/fbw_game`. Run it from the project root so the `game/` assets resolve:

```sh
./build/fbw_game game
```

Controls: menu Up/Down + Enter; Fireboy uses the arrow keys and Space; Watergirl uses A/D/W. Escape pauses; M returns to level selection; Enter resumes or advances after a win; R restarts; Tab toggles two-player/solo mode in the menu and switches characters during solo play. One mapped game controller controls Fireboy and a second controls Watergirl (left stick/D-pad + A). Progress is written to `fbw_progress.json` in the working directory.

This is an early interactive desktop prototype, not a finished PS3 port. The static map art and character atlas animations render in a window, but physics is still approximate, many interactive devices are not simulated, menus/audio are missing, and PS3 toolchain/build support has not yet been verified. The original assets remain subject to the original game's attribution and non-redistribution constraints.

The interactive runtime also has a first-pass mechanism simulation: pushable boxes use gravity and horizontal pushing, grouped platforms move when linked levers or pressure pushers activate, wind regions apply lift when enabled by a group, and frozen-water contact changes the character's ice movement/animation state. These are approximations, not yet equivalent to the original Box2D joints and device state machine.

The runtime's current interactive object coverage includes approximate collision for the 14 boxes (the 12 balls are drawn but not simulated — see `docs/MECHANISM_AUDIT.md`), static hanging/pulley platforms and Roman beams, grouped lever/pusher platform activation, animated liquid surfaces/doors/wind effects, and collectible removal. Several device types are still approximations (notably rotating/hanging physics), and sound playback is not implemented.

A desktop runtime smoke-test capture is available at [`docs/runtime_movement_smoke.png`](docs/runtime_movement_smoke.png). It verifies that the SDL2 window draws the Forest Temple scene and responds to directional input in the test environment.

### Latest runtime update

The desktop prototype supports gamepad-only menu navigation and solo-mode controls: D-pad selects levels, A starts, X switches solo/two-player, B exits/returns to menu, Start pauses/resumes, and Y switches the active character in solo mode. Progress is saved when returning to the menu. Current validation is 19/19 CTest tests plus `tests/check_spec.py` (`ALL OK`). This is still a desktop prototype; the PS3-specific `ioPad`/RSX/audio/save adapters and full original-mechanics parity are not complete.


Latest runtime fix: solo mode camera follows the selected character after switching heroes; two-player mode retains midpoint camera tracking. Build/test status: 19/19 CTest cases pass and the source-spec checker reports `ALL OK`. This is still a desktop prototype, not a finished PS3 port.


Physics regression coverage now explicitly includes vertical walls and ceilings in addition to platform landing, jumping, slopes, and liquid sensor filtering. Current verification: 19/19 CTest cases and `tests/check_spec.py` (`ALL OK`). The runtime remains an approximation and still needs full device/physics parity and a PS3-target build.


## Current completion estimate (2026-10-31)

Estimated overall progress toward the complete PS3 remake: **35%**. The desktop prototype and asset pipeline are partly working, but gameplay parity, complete mechanisms/audio, PSL1GHT/RSX controller/display integration, and a verified PS3 build remain unfinished. This is an engineering estimate, not a test-derived metric. Current validation: 19/19 CTest tests pass; all 32 maps pass desktop validation; source-spec check reports `ALL OK`.
