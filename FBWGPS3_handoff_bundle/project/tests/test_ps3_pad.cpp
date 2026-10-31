// Linux-side unit tests for the PS3 pad conversion layer. These prove the mapping logic and its integration with
// fw::Input; they do NOT prove the bit constants match the real PS3 SDK (see the note in src/ps3_pad.hpp).
#include <cmath>
#include <cstdio>

#include "ps3_pad.hpp"

namespace {
int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

fw::ps3::RawPad pad(uint8_t d1 = 0, uint8_t d2 = 0, uint8_t lx = 128, uint8_t ly = 128, bool connected = true) {
    fw::ps3::RawPad p;
    p.connected = connected; p.digital1 = d1; p.digital2 = d2; p.left_x = lx; p.left_y = ly;
    return p;
}

void testBitMapping() {
    using namespace fw;
    using namespace fw::ps3;
    CHECK(toButtons(D1_UP, 0) == BTN_UP);
    CHECK(toButtons(D1_DOWN, 0) == BTN_DOWN);
    CHECK(toButtons(D1_LEFT, 0) == BTN_LEFT);
    CHECK(toButtons(D1_RIGHT, 0) == BTN_RIGHT);
    CHECK(toButtons(D1_START, 0) == BTN_START);
    CHECK(toButtons(D1_SELECT, 0) == BTN_SELECT);
    CHECK(toButtons(0, D2_CROSS) == BTN_CROSS);
    CHECK(toButtons(0, D2_CIRCLE) == BTN_CIRCLE);
    CHECK(toButtons(0, D2_SQUARE) == BTN_SQUARE);
    CHECK(toButtons(0, D2_TRIANGLE) == BTN_TRIANGLE);
    CHECK(toButtons(0, D2_L1) == BTN_L1);
    CHECK(toButtons(0, D2_R1) == BTN_R1);
    CHECK(toButtons(0, D2_L2) == BTN_L2);
    CHECK(toButtons(0, D2_R2) == BTN_R2);
    CHECK(toButtons(0, 0) == 0u);
    // L3/R3 are deliberately unused by the game.
    CHECK(toButtons(D1_L3 | D1_R3, 0) == 0u);
    // Every digital bit is distinct: all mapped bits together produce 14 distinct Button bits.
    const uint32_t all = toButtons(0xFF, 0xFF);
    int bits = 0;
    for (uint32_t v = all; v; v &= v - 1) ++bits;
    CHECK(bits == 14);
}

void testAxis() {
    using fw::ps3::axisToUnit;
    CHECK(axisToUnit(128) == 0.0f);
    CHECK(axisToUnit(0) == -1.0f);
    CHECK(axisToUnit(255) > 0.99f && axisToUnit(255) < 1.0f);
    CHECK(axisToUnit(64) < 0.0f && axisToUnit(192) > 0.0f);
}

void testDisconnectedIsNeutral() {
    const fw::PadState s = fw::ps3::toPadState(pad(0xFF, 0xFF, 0, 0, false));
    CHECK(!s.connected);
    CHECK(s.buttons == 0u);
    CHECK(s.lx == 0.0f && s.ly == 0.0f);
}

void testConnectedConversion() {
    const fw::PadState s = fw::ps3::toPadState(pad(fw::ps3::D1_LEFT, fw::ps3::D2_CROSS, 0, 255));
    CHECK(s.connected);
    CHECK(s.buttons == (fw::BTN_LEFT | fw::BTN_CROSS));
    CHECK(s.lx == -1.0f);
    CHECK(s.ly > 0.99f);   // +y is down
}

// ---- integration with the gameplay input layer ----
void testTwoPadsDriveBothCharacters() {
    fw::Input in;
    fw::PadState pads[2];
    pads[0] = fw::ps3::toPadState(pad(fw::ps3::D1_RIGHT, fw::ps3::D2_CROSS));   // pad 0 = Fireboy
    pads[1] = fw::ps3::toPadState(pad(fw::ps3::D1_LEFT));                       // pad 1 = Watergirl
    in.update(pads, 1.0f / 60.0f);
    CHECK(in.mode() == fw::Mode::TwoPads);
    CHECK(in.fireboy().right.isDown && in.fireboy().up.isDown && !in.fireboy().left.isDown);
    CHECK(in.watergirl().left.isDown && !in.watergirl().up.isDown && !in.watergirl().right.isDown);
}

void testStickWithHysteresis() {
    fw::Input in;
    fw::PadState pads[2];
    // 255 -> full right: engages.
    pads[0] = fw::ps3::toPadState(pad(0, 0, 255, 128));
    in.update(pads, 1.0f / 60.0f);
    // Single-pad mode routes input to the active character (Fireboy by default); the stick acts in the same frame.
    CHECK(in.mode() == fw::Mode::OnePad);
    CHECK(in.fireboy().right.isDown);
    // Back to ~0.45 (between off=0.30 and on=0.50): must stay engaged thanks to hysteresis.
    pads[0] = fw::ps3::toPadState(pad(0, 0, static_cast<uint8_t>(128 + 58), 128));   // (186-128)/128 ~ 0.45
    in.update(pads, 1.0f / 60.0f);
    CHECK(in.fireboy().right.isDown);
    // Back to centre: releases.
    pads[0] = fw::ps3::toPadState(pad(0, 0, 128, 128));
    in.update(pads, 1.0f / 60.0f);
    CHECK(!in.fireboy().right.isDown);
}

void testSinglePadToggleAndUnplug() {
    fw::Input in;
    fw::PadState pads[2];
    pads[0] = fw::ps3::toPadState(pad());
    in.update(pads, 1.0f / 60.0f);
    CHECK(in.mode() == fw::Mode::OnePad);
    CHECK(in.activeCharacter() == 0);
    // L1 edge toggles to Watergirl.
    pads[0] = fw::ps3::toPadState(pad(0, fw::ps3::D2_L1));
    in.update(pads, 1.0f / 60.0f);
    CHECK(in.activeCharacter() == 1);
    // Holding L1 does not toggle again.
    in.update(pads, 1.0f / 60.0f);
    CHECK(in.activeCharacter() == 1);
    // Cross now jumps Watergirl, not Fireboy.
    pads[0] = fw::ps3::toPadState(pad(0, fw::ps3::D2_CROSS));
    in.update(pads, 1.0f / 60.0f);
    CHECK(in.watergirl().up.isDown && !in.fireboy().up.isDown);
}

void testStartPausesAndMenuConfirm() {
    fw::Input in;
    fw::PadState pads[2];
    pads[0] = fw::ps3::toPadState(pad());
    in.update(pads, 1.0f / 60.0f);
    pads[0] = fw::ps3::toPadState(pad(fw::ps3::D1_START));
    in.update(pads, 1.0f / 60.0f);
    CHECK(in.pausePressed());
    in.update(pads, 1.0f / 60.0f);
    CHECK(!in.pausePressed());   // edge only
    pads[0] = fw::ps3::toPadState(pad(0, fw::ps3::D2_CROSS));
    in.update(pads, 1.0f / 60.0f);
    CHECK(in.menuEvent() == fw::MenuEvent::Confirm);
    pads[0] = fw::ps3::toPadState(pad());
    in.update(pads, 1.0f / 60.0f);
    pads[0] = fw::ps3::toPadState(pad(0, fw::ps3::D2_CIRCLE));
    in.update(pads, 1.0f / 60.0f);
    CHECK(in.menuEvent() == fw::MenuEvent::Back);
}
} // namespace

int main() {
    testBitMapping();
    testAxis();
    testDisconnectedIsNeutral();
    testConnectedConversion();
    testTwoPadsDriveBothCharacters();
    testStickWithHysteresis();
    testSinglePadToggleAndUnplug();
    testStartPausesAndMenuConfirm();
    if (failures) { std::printf("%d check(s) failed\n", failures); return 1; }
    std::printf("test_ps3_pad: all checks passed\n");
    return 0;
}
