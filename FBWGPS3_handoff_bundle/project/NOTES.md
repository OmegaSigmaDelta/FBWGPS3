# PS3 Port Notes

Source audited: `game/js/init.js` from `fireboy_watergirl_offline.zip`.

Found **34 annotated code comments**. The file header also mentions the string `[PS3-PORT]`, so a raw text search returns 35 occurrences, but only 34 are actual annotated code comments. The roadmap expected 35 entries; resolve this mismatch before marking the step fully complete.

| # | Line | Port implication |
|---:|---:|---|
| 1 | 17769 | *** ALL MENUS ARE POINTER-DRIVEN *** (Phaser inputEnabled + onInputDown/Up, no focus navigation). For a controller you must add either (a) a virtual mouse cursor (left stick moves, Cross = click) or (b) a focus ring over Button instances (D-pad moves, Cross = onInputUp). Every menu State/Button derives from this. |
| 2 | 24510 | LEVEL DEVICES (one class per Tiled object.type): Ball, Beam(er), Box/BoxHeavy/BoxMirror, Diamond, Door, Hanging, InfiniteMirror, Lever, LightPusher, Liquid, MovingPlatform, Portal, Pulley, Pusher, Roman, RotMirror, Slider, Snow, TimedPusher, Wind, Window. Each: constructor reads Tiled properties, builds Box2D bodies/sensors, exposes sounds() (audio keys to preload), update(), and reacts to Mech triggers. Light-beam devices (Beam, mirrors, LightPusher, Window) do 2D ray casting - port with b2World::RayCast. |
| 3 | 25921 | DEVICE REGISTRY: maps Tiled object.type string -> device class (k.className(type)). Unknown types only log "Object type X not found!" - keep that behaviour. |
| 4 | 26961 | Pixi/Phaser WebGL filters (outline, inline, shadow, bevel, colorMatrix, bwMatrix) are purely cosmetic (button highlights, locked-state greyscale). Re-implement as RSX fragment shaders or pre-baked sprites, or skip. |
| 5 | 27251 | Builds terrain: reads tile layer "Ground", creates static Box2D b2PolygonShape fixtures for solid tiles, and bakes auto-tiled visuals (neighbour-aware tiles via getTileUp/Left/Right) into a bitmapData. Native: pre-bake once into a texture at level load. |
| 6 | 27547 | KEY BINDINGS (desktop): wg: W/A/D, fb: UP/LEFT/RIGHT. No gamepad API is used by the game logic. For PS3 expose cursors.up/left/right from pad1/pad2 D-pad or left stick (deadzone ~0.35) + Cross for jump. |
| 7 | 27571 | *** PLAYER CHARACTERS *** id "fb" = Fireboy, id "wg" = Watergirl (NOT symmetric: each dies in the other's liquid). Box2D dynamic body + feet sensor; custom run force / jump impulse / ice friction logic in update(). Reads this.cursors.{left,right,up}.isDown only -> trivial to feed from gamepads. *** TWO-CONTROLLER HOOK ***: see the addKey(...) block below. Replace Phaser.Key objects with a tiny struct {isDown,justDown} fed from pad state: pad1 -> one character, pad2 -> the other (original desktop layout: wg=WASD, fb=Arrow keys). Single-pad fallback = "single" controls mode with CursorToggle (shoulder button switches the active character). |
| 8 | 27638 | CHARACTER PHYSICS TICK: run force ramp (currentRunForce), jumpImpulse/jumpVelocity, feetSensor contact sort to find ground normal, ice sliding (fb/wg differ), liquid float, speed clamps (maxSpeed/absoluteMaxSpeed). Uses Box2D calls: SetLinearVelocity, GetLinearVelocity, ApplyLinearImpulse, ApplyForce. Port verbatim. |
| 9 | 27674 | "dark" levels: radial-gradient light cut-out drawn on a 2D canvas (globalCompositeOperation "lighter"). Native: render a light texture additively into an offscreen buffer, or use an alpha-mask shader. |
| 10 | 27699 | Connects devices to each other: registerDevice(), deviceChanged() (trigger propagation), resetStates(), connectPortals(), connectToWall(). |
| 11 | 27784 | Follow camera driven by both characters: adjusts game.camera.x/y and camera.scale (zoom, clamped with Math.min/max). Pure math. |
| 12 | 28043 | Toggles: music, fx, vibrate, controls(keyboard\|single). On PS3 hide "controls" and add "players: 1 / 2" (2 = two pads). |
| 13 | 28125 | Touch/"single" mode widget to switch the controlled character. Reuse for single-pad play (map to L1/R1). |
| 14 | 28214 | HUD: clock, diamond counter, pause button. Clock uses levelData.time / levelData.mobileTime (from temple.json). |
| 15 | 28278 | End-of-level popup (Next/Retry/Menu). Result (stars, best time) is stored through ProgressModel (see betterThan/stars logic there). |
| 16 | 28389 | *** CORE GAMEPLAY STATE *** (~270 lines). Lifecycle: init(levelData) -> create() loads Tiled map data/<filename> + Temples/<id>/TempleAssets atlas + audio -> baseLoadComplete -> loadComplete() builds: Ground (tiles), Box2D world, MechManager, characters, devices, camera, UI -> start()/startLevel() -> update() each frame -> checkEndGame/gameFinish/gameOver -> quit/retry. Level types (levelData.type): undefined=normal, "speed" (timer), "dark" (darkness mask around chars), "puzzle". |
| 17 | 28418 | Builds the sound list: 18 fixed sounds + each device's sounds(); file name = Capitalised key + ".mp3". Music key varies by level type (levelMusic_dark / levelMusic_speed). |
| 18 | 28428 | Creates Phaser sounds from loaded buffers; map to cellAudio / libmixer voices. |
| 19 | 28444 | Converts the Tiled JSON into game-side data (objects grouped by layer). Port a tiny Tiled parser. |
| 20 | 28483 | BUILD STEP: instantiates Ground, Box2D bodies, characters (layer "Chars") and devices (layer "Objects"). |
| 21 | 28489 | PHYSICS INIT: Box2D world, PTM ratio 32 px/m. Gravity is set later in loadComplete: box2d.gravity.y = 1000 (pixel units, i.e. ~31 m/s^2 at PTM 32). |
| 22 | 28528 | Bound to keyboard P (pauseKey). Map to START button. |
| 23 | 28613 | Per-frame level tick (UI, dust, tutorial text, camera). Physics stepping itself is done by the Phaser Box2D plugin; natively call b2World::Step(dt, 8, 3) once per frame (dt from game.time.elapsed). |
| 24 | 28730 | "Level locked" popup. Keep (unlock rules are in ProgressModel). |
| 25 | 28879 | In-app purchase screens (mobile store). Dead for this build, DROP. |
| 26 | 28985 | Level select (tree of 32 levels laid out from temple.json x/y in 0..1). Locked levels per ProgressModel. |
| 27 | 29073 | Title screen (Play, settings gear). Entry to LevelMenu via TempleHall. |
| 28 | 34145 | SAVE DATA (Backbone model). Persists JSON to localStorage "progress_<gameId>": per level {best:{stars,time,...}, played, locked, state, isNew}. Level unlock rule: level.required / stars of previous. Reimplement as a plain struct + JSON (or binary) save. toJSON()/fetch()/save()/reset are the only API surface. |
| 29 | 34245 | BOOT STATE: loads game.json + PreloaderAssets atlas + Kizi/credit logo, builds ProgressModel (fetch -> reads data/<temple>/temple.json for every id in game.json.temples), then starts States/Load. The "cordova" branch (deviceready, in-app store) is mobile-store code: IGNORE on PS3. |
| 30 | 34290 | Loading progress bar. The portal postMessage handshake was removed here. |
| 31 | 34321 | LOAD STATE: queues every shared asset: atlases (Char/Ground/Mech/Menu/Popup/Backgrounds), 4 bitmap fonts (assets/fonts/font*.png + the single shared font.fnt), TempleHall<Name>.jpg, GameName<Name>.png. Single-temple build uses the ELSE branch (TempleHallForest.jpg / GameNameForest.png). The multi-temple branch (TempleIcons atlas, GameNameElements.png) is dead code for this game. |
| 32 | 34385 | i18n lookup (English only: utils/lang/en). Keep as a string table. |
| 33 | 34412 | Touch gesture helper (hammer.js). DROP on PS3. |
| 34 | 34939 | ENTRY POINT. Builds Phaser config + Game, registers states Boot/Load/Level, settings = {vibrate,music,fx,controls}. controls is "keyboard" on desktop (both characters active at once: fb = arrow keys, wg = WASD) and "single" on touch devices (one on-screen character at a time, CursorToggle switches). config.devSkipAds is forced true in this copy. Settings persist in localStorage key "settings". PS3: replace localStorage with a save file (e.g. /dev_hdd0/game/<TITLEID>/USRDIR/save.json) - same JSON. |
