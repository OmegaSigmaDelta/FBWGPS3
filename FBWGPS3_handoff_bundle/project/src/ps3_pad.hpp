// PS3 pad sample -> engine-neutral PadState (header-only, no PSL1GHT dependency).
//
// The PS3 pad API (libio ioPad / Cell pad) delivers each sample as two digital bytes plus 0..255 analog axes.
// This header converts such a raw sample into fw::PadState so the tested fw::Input layer can consume it unchanged.
// The platform glue (NOT in this file, NOT yet built or run on a PS3) only has to:
//     ioPadGetInfo2()  -> RawPad::connected for pads 0 and 1
//     ioPadGetData()   -> fill RawPad::digital1 / digital2 / left_x / left_y
//     fw::Input::update(pads, dt) with the PadState values returned by toPadState().
//
// VERIFY ON TARGET: the bit values below follow the Cell pad CTRL constants as recalled when no PSL1GHT headers were
// available in the development environment. They are covered by Linux unit tests against these constants, which proves
// the conversion logic, not that the constants match the real SDK. Check them against PSL1GHT's io/pad.h before use.
#pragma once
#include <cstdint>

#include "input.hpp"

namespace fw {
namespace ps3 {

// digital1 bits
constexpr uint8_t D1_SELECT = 0x01;
constexpr uint8_t D1_L3     = 0x02;
constexpr uint8_t D1_R3     = 0x04;
constexpr uint8_t D1_START  = 0x08;
constexpr uint8_t D1_UP     = 0x10;
constexpr uint8_t D1_RIGHT  = 0x20;
constexpr uint8_t D1_DOWN   = 0x40;
constexpr uint8_t D1_LEFT   = 0x80;

// digital2 bits
constexpr uint8_t D2_L2       = 0x01;
constexpr uint8_t D2_R2       = 0x02;
constexpr uint8_t D2_L1       = 0x04;
constexpr uint8_t D2_R1       = 0x08;
constexpr uint8_t D2_TRIANGLE = 0x10;
constexpr uint8_t D2_CIRCLE   = 0x20;
constexpr uint8_t D2_CROSS    = 0x40;
constexpr uint8_t D2_SQUARE   = 0x80;

struct RawPad {
    bool connected = false;
    uint8_t digital1 = 0;
    uint8_t digital2 = 0;
    uint8_t left_x = 128;   // 0 = full left,  255 = full right, ~128 = centred
    uint8_t left_y = 128;   // 0 = full up,    255 = full down,  ~128 = centred
};

// 0..255 -> -1..~+1 (128 maps to exactly 0). +y is DOWN, matching fw::PadState.
inline float axisToUnit(uint8_t value) { return (static_cast<float>(value) - 128.0f) / 128.0f; }

inline uint32_t toButtons(uint8_t d1, uint8_t d2) {
    uint32_t b = 0;
    if (d1 & D1_UP)       b |= BTN_UP;
    if (d1 & D1_DOWN)     b |= BTN_DOWN;
    if (d1 & D1_LEFT)     b |= BTN_LEFT;
    if (d1 & D1_RIGHT)    b |= BTN_RIGHT;
    if (d1 & D1_START)    b |= BTN_START;
    if (d1 & D1_SELECT)   b |= BTN_SELECT;
    if (d2 & D2_CROSS)    b |= BTN_CROSS;
    if (d2 & D2_CIRCLE)   b |= BTN_CIRCLE;
    if (d2 & D2_SQUARE)   b |= BTN_SQUARE;
    if (d2 & D2_TRIANGLE) b |= BTN_TRIANGLE;
    if (d2 & D2_L1)       b |= BTN_L1;
    if (d2 & D2_R1)       b |= BTN_R1;
    if (d2 & D2_L2)       b |= BTN_L2;
    if (d2 & D2_R2)       b |= BTN_R2;
    return b;
}

// A disconnected pad yields a neutral state so stale bytes can never leak into gameplay.
inline PadState toPadState(const RawPad& raw) {
    PadState s;
    if (!raw.connected) return s;
    s.connected = true;
    s.buttons = toButtons(raw.digital1, raw.digital2);
    s.lx = axisToUnit(raw.left_x);
    s.ly = axisToUnit(raw.left_y);
    return s;
}

} // namespace ps3
} // namespace fw
