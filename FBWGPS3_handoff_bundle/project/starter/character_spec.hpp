// Character physics SPEC (data only, no engine dependency). Extracted from init.js "States/Level/character".
// Wire this to Box2D (b2Body dynamic, bullet, fixed rotation). Coordinates below are SCREEN space, y DOWN, in METRES
// (32 px = 1 m). The original stored mirrored coordinates (y up = negative); signs here are already converted.
//
// tests/check_spec.py re-verifies every number in this file against js/init.js.
#pragma once

namespace fw { namespace character {

constexpr double PTM = 32.0;

// ---- spawn ----
// Tiled tile object (x, y) = bottom-left of a 64x64 tile  ->  body spawns at (x + 32, y - 32) px (its centre).
constexpr double spawnOffsetX = 32.0, spawnOffsetY = -32.0;

// ---- movement constants (Character.prototype.*) ----
constexpr double jumpImpulse      = 1.0;     // ApplyLinearImpulse(0, jumpImpulse) at jump start (see update())
constexpr double jumpVelocity     = 15.5;    // m/s
constexpr double jumpIceVelocity  = 10.0;    // m/s when standing on ice
constexpr double runForce         = 50.0;    // N, target of the run-force ramp (currentRunForce)
constexpr double floatForce       = 30.0;    // N, used while in liquid
constexpr double defaultMaxSpeed  = 8.5;     // m/s
constexpr double waterIceMaxSpeed = 2.0;     // m/s
constexpr double absoluteMaxSpeed = 40.0;    // m/s hard clamp
constexpr double feetFriction     = 0.5;
constexpr double spriteScale      = 0.95;

// ---- body shape ----
constexpr double r1 = 0.48;   // upper (body) circle radius
constexpr double r2 = 0.20;   // lower (feet) circle radius
constexpr double dr = 0.03;   // inset used for the hull
constexpr double y1 = -0.20;  // body circle centre, y DOWN (original +0.2 in mirrored space) -> 6.4 px ABOVE body origin
constexpr double y2 = +0.90;  // feet circle centre, y DOWN (original -0.9 in mirrored space) -> 28.8 px BELOW origin

// Collision filtering
constexpr unsigned short categoryChar = 16, maskChar = 4353;       // 0x1101: ground(1) + devices(256) + liquids(4096)
constexpr unsigned short categoryPortalSensor = 4096, maskPortalSensor = 4096; constexpr short portalGroupIndex = -1;

// Fixtures created in this order (all on one dynamic body, bullet = true, fixedRotation = true):
//   1. portal sensor : circle r = 0.1 at (0,0),           density 0,   friction 0,           cat 4096 mask 4096 group -1
//   2. body circle   : circle r = r1  at (0, y1),         density 0,   friction 0,           cat 16   mask 4353
//   3. feet circle   : circle r = r2  at (0, y2),         density 0,   friction feetFriction cat 16   mask 4353   userData "feet"
//   4. hull polygon  : (+(r1-dr), y1) (-(r1-dr), y1) (+(r2-dr), y2) (-(r2-dr), y2)
//                                                          density 1.4, friction 0,           cat 16   mask 4353
// Mass therefore comes only from the hull (density 1.4). Ground contact is detected via the feet fixture's contacts
// (normal sorted by dot with gravity direction) - see update() in init.js, port it verbatim.
constexpr double hullDensity = 1.4;

}} // namespace fw::character
