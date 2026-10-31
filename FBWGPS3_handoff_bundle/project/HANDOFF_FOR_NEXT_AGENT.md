# FBWGPS3 — Handoff Brief for the Next Agent

**Snapshot date:** 2026-10-31  
**User goal:** Continue rebuilding Fireboy & Watergirl: Forest Temple as an offline game, ultimately targeting PlayStation 3. The user asked that all project state and notes be consolidated so another agent can continue without asking them to repeat context.

## Read these first

1. `HANDOFF_FOR_NEXT_AGENT.md` — this handoff and exact next steps.
2. `README.md` — current project overview, build instructions and known limitations.
3. `PORTING.md` — chronological porting notes, implementation details and progress estimate.
4. `NOTES.md` and `LEVELS.md` — original source/level notes.
5. `BUILD_INFO.txt` and `BUILD_TEST_RESULTS.txt` — most recent recorded build/test snapshot.

## Current state — do not overstate it

This is an **early interactive Linux desktop prototype and a PS3 porting base, not a finished game**. The target is a verified PS3 build that can be played offline. No PS3-target binary has been built or tested in this snapshot.

- C++17/CMake project.
- Supplied Tiled map data and game assets are under `game/`.
- All 32 Forest Temple maps load through the project parser and pass the desktop runtime's all-level startup validation.
- Eight supplied texture atlas JSON files / 984 frames have been checked against PNG dimensions.
- CPU preview renderer and visual contact sheet are included in `docs/`.
- SDL2 desktop runtime exists in `src/game_main.cpp`; keyboard and generic SDL gamepad input, basic menu/navigation, camera, pause, level transitions and progress JSON are implemented in prototype form.
- `src/physics_world.hpp` is a fallback approximate AABB/polygon solver, not the original Box2D implementation. It has basic gravity, movement, jumping and regression coverage for floors, walls, ceilings, floor seams and wall sliding.
- `src/game_world.hpp` integrates the fallback simulation with some basic object overlaps and game state. Several mechanisms are approximate; original-game physics parity and all-level completion are not verified.
- Some levers, pushers, moving platforms, boxes/balls, wind and frozen-water interactions have first-pass approximations. This is not proof that all puzzles are solvable.
- Progression/scoring/save logic and a level-session layer exist, but platform-specific save paths and all production menu flows need verification.
- Audio playback is not implemented.
- Original `font.fnt` is missing from the supplied files; the BMFont parser is tested with a fixture only.
- SDL2/Box2D development headers were not available in the recorded environment; runtime SDL2 support is linked through a minimal ABI header. There is no Box2D world adapter.
- No PS3 `ioPad`, RSX renderer, audio, save-data adapter, PSL1GHT toolchain validation, package generation or hardware test is available yet.

## Update after the second session (2026-10-31)

- Baseline rebuilt from scratch and re-verified; one new component added, so the suite is now **20/20 CTest** (was 19/19) plus `check_spec.py` = ALL OK.
- **PS3 toolchain gate (step 7) was checked: BLOCKED in the cloud workspace** — no PS3 SDK installed and its sources are unreachable from there (details in `PORTING.md` step 1.33). A PS3 build needs a machine with ps3toolchain + PSL1GHT. Do not promise or claim a PS3 build until one is produced.
- New: `src/ps3_pad.hpp` + `tests/test_ps3_pad.cpp` (raw PS3 pad sample -> `fw::PadState`). Its bit constants are unverified against the real SDK headers.
- Step 4 (mechanism audit) started: see `docs/MECHANISM_AUDIT.md`. It corrected an earlier claim — balls were **not** simulated (only the 14 boxes were) — and lists the next fixes in order (rotating platforms, levers, hanging/pulley/Roman).
- Balls are now rolling circle bodies (`PORTING.md` step 1.35, `tests/test_ball_physics.cpp`). Not yet looked at on screen: the runtime has no screenshot hook, so the ball sprite's position/rotation sync in `game_main.cpp` is compiled but unseen. Suite is now **22/22** (`mechanism_inventory` and `rolling_balls` were added this session).
- Still not done: physics parity, mechanism audit, audio, menus on PS3, RSX renderer, save-data adapter, packaging, any on-target test.
- GitHub: this session's push access to the repository was refused (the Claude GitHub App is not installed for it), so these changes exist only in the session's local clone, not on GitHub.

## Recorded validation (first snapshot)

Snapshot logs say:

- CMake Release build succeeded on Linux x86-64 using C++17.
- `ctest --test-dir build --output-on-failure`: **19/19 passed**.
- `python3 tests/check_spec.py`: **ALL OK**.
- Desktop runtime startup validation and all-32-level validation passed.

Treat these as component/regression checks only. They do **not** prove that every level can be completed, that all device interactions match the original, or that the PS3 port runs.

## Build / test commands

Run from this directory (project root):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 tests/check_spec.py
```

Run the desktop prototype from the project root so asset paths resolve:

```sh
./build/fbw_game game
```

If the archive was extracted elsewhere, run the build again rather than assuming old CMake cache paths remain valid. The `build/` directory is intentionally not relied upon in this handoff; use the sources and rebuild on the new machine.

## Important files

- `CMakeLists.txt` — targets and tests.
- `src/game_main.cpp` — SDL2 desktop runtime, render/input/update loop.
- `src/game_world.hpp` — current integrated fallback game world.
- `src/physics_world.hpp` — approximate fallback physics.
- `src/contact_router.hpp` — engine-neutral contact event routing contract.
- `src/gameplay_runtime.hpp` — contact rules and gameplay state.
- `src/level_session.hpp` — session, timer, result and progress integration.
- `src/progression.hpp`, `src/progression_catalog.hpp`, `src/progress_save.hpp` — scoring/catalogue/save data.
- `src/render_queue.hpp`, `src/world_render.hpp`, `src/character_animation.hpp`, `src/software_renderer.*` — render preparation and preview pipeline.
- `tests/` — regression and integration tests.
- `game/` — supplied maps, atlas images/metadata and original code/assets.
- `docs/forest_temple_contact_sheet.jpg` — preview contact sheet for all 32 maps.
- `docs/runtime_*_smoke.png` — runtime smoke-test captures.
- `compiled/linux-x86_64/` — prebuilt Linux tools/tests only; not PS3 binaries.

## Recommended next steps — do these in order

1. **Rebuild and establish baseline.** Run all four commands above, record exact output and resolve any portability/build errors before modifying code.
2. **Inspect `src/game_main.cpp`, `src/game_world.hpp`, `src/physics_world.hpp` together.** Trace one frame from input through physics, contacts, gameplay state, animation and draw queue. Fix the integration seams rather than adding isolated helper headers.
3. **Physics correctness.** Add reproducible tests for character bounds, sloped ground, corners, moving platforms, box pushing, liquid sensors, freeze/unfreeze, collision ordering and frame-rate independence. Compare values/behavior with `game/js/` source wherever possible. Do not claim Box2D parity while using the fallback solver.
4. **Mechanism audit.** Inventory every object type from all 32 maps and document which are static art, collision geometry, sensor, or dynamic mechanism. Implement device state transitions and puzzle requirements. Add tests per mechanism and run level-by-level playability checks.
5. **Gameplay completeness.** Verify death/restart, door conditions, character-specific liquid rules, all collectibles, timing/stars, level unlocks, save/load, solo switching and two-player control. Test each level manually or with deterministic simulation where feasible.
6. **Visual/audio pass.** Check atlas frame selection, ordering, camera behavior, missing font/audio resources, menu readability, sound playback and offline asset path handling.
7. **PS3 feasibility gate.** First determine the exact available PS3 homebrew SDK/toolchain and target expectations (ELF/PKG and CFW/HEN environment). Verify the SDK exists before promising a PS3 build. Add separate platform adapters for `ioPad`, RSX/GCM or another actually supported renderer, audio, timing and save data. Keep core game logic platform-neutral.
8. **Build and test on target.** A PS3 port is not complete until the target build is produced and launched on an emulator or actual compatible hardware, controller mappings work, and several representative levels plus save/load are tested. State clearly if hardware/toolchain access blocks this.
9. Update `PORTING.md`, `README.md`, `BUILD_INFO.txt` and `BUILD_TEST_RESULTS.txt` after each meaningful change. Rebuild archives only after tests and a runtime smoke check.

## Progress estimate

`PORTING.md` records an approximate **35% overall completion** toward the full remake-to-PS3 deliverable as of 2026-10-31. This is an engineering estimate, not a measured metric. It reflects that source/assets ingestion and a desktop prototype exist, but gameplay parity and the PS3-specific implementation are mostly outstanding. Re-estimate only when supported by concrete implementation and validation evidence.

## Attribution / asset handling

The original game is credited to **Oslo Albet / Agame**. Preserve attribution. The project was framed as a personal offline porting effort; do not redistribute the original game assets publicly. Before sharing outside the user's intended private workflow, check rights and remove original assets if needed.

## Working agreement

- Make changes in the actual project, not just describe intended work.
- Prefer small, buildable increments with tests that fail before a fix and pass after it.
- Keep the user's requested final goal in view: a complete offline PS3 port, not just passing unit tests or a desktop demo.
- Never say the game is finished until a real playable build is verified.
- Do not claim a GitHub commit or release unless a tool confirms it succeeded. Earlier GitHub write attempts were denied (HTTP 403); this snapshot is local.
