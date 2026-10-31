// Unit tests for the two-controller input layer. Build: g++ -std=c++17 -Wall -Wextra tests/test_input.cpp -o test_input && ./test_input
#include "../src/input.hpp"
#include <cstdio>
using namespace fw;
static int fails = 0, checks = 0;
#define CHECK(c) do { ++checks; if (!(c)) { ++fails; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static PadState pad(bool conn, uint32_t b = 0, float lx = 0, float ly = 0) { PadState p; p.connected = conn; p.buttons = b; p.lx = lx; p.ly = ly; return p; }

int main() {
    const float dt = 1.f / 60;
    { // two pads drive two characters independently and simultaneously
        Input in; PadState p[2] = { pad(true, BTN_RIGHT | BTN_CROSS), pad(true, BTN_LEFT) };
        in.update(p, dt);
        CHECK(in.mode() == Mode::TwoPads);
        CHECK(in.fireboy().right.isDown && in.fireboy().up.isDown && !in.fireboy().left.isDown);
        CHECK(in.watergirl().left.isDown && !in.watergirl().up.isDown && !in.watergirl().right.isDown);
        CHECK(in.fireboy().right.justDown);                  // edge flags like Phaser.Key
        in.update(p, dt);
        CHECK(!in.fireboy().right.justDown && in.fireboy().right.isDown);
    }
    { // swapped assignment (cfg.fbPad/wgPad)
        Input in; in.cfg.fbPad = 1; in.cfg.wgPad = 0;
        PadState p[2] = { pad(true, BTN_LEFT), pad(true, BTN_RIGHT) };
        in.update(p, dt);
        CHECK(in.fireboy().right.isDown && in.watergirl().left.isDown);
    }
    { // stick hysteresis: engage > 0.5, hold until < 0.3
        Input in; PadState p[2] = { pad(true), pad(true) };
        p[0].lx = 0.45f; in.update(p, dt); CHECK(!in.fireboy().right.isDown);
        p[0].lx = 0.55f; in.update(p, dt); CHECK(in.fireboy().right.isDown);
        p[0].lx = 0.40f; in.update(p, dt); CHECK(in.fireboy().right.isDown);     // still held (between 0.3 and 0.5)
        p[0].lx = 0.25f; in.update(p, dt); CHECK(!in.fireboy().right.isDown);
        p[0].lx = -0.9f; in.update(p, dt); CHECK(in.fireboy().left.isDown && !in.fireboy().right.isDown);
    }
    { // stick up jumps; opposite d-pad directions cancel
        Input in; PadState p[2] = { pad(true), pad(true, BTN_LEFT | BTN_RIGHT) };
        p[0].ly = -0.8f; in.update(p, dt); CHECK(in.fireboy().up.isDown);
        CHECK(!in.watergirl().left.isDown && !in.watergirl().right.isDown);
    }
    { // one pad only -> single mode; L1 toggles active character; inactive character gets no input
        Input in; PadState p[2] = { pad(true, BTN_RIGHT), pad(false) };
        in.update(p, dt);
        CHECK(in.mode() == Mode::OnePad && in.activeCharacter() == 0);
        CHECK(in.fireboy().right.isDown && !in.watergirl().right.isDown);
        p[0].buttons = BTN_RIGHT | BTN_L1; in.update(p, dt);                     // toggle
        CHECK(in.activeCharacter() == 1 && in.watergirl().right.isDown && !in.fireboy().right.isDown);
        CHECK(in.fireboy().right.justUp);                                       // fireboy released cleanly
        p[0].buttons = BTN_RIGHT; in.update(p, dt);
        p[0].buttons = BTN_RIGHT | BTN_R1; in.update(p, dt);                     // R1 toggles back
        CHECK(in.activeCharacter() == 0);
        p[0].buttons = BTN_RIGHT | BTN_R1; in.update(p, dt);                     // held: no re-toggle
        CHECK(in.activeCharacter() == 0);
    }
    { // hot-unplug of pad 2 mid-level: falls back to single mode, no stuck keys
        Input in; PadState p[2] = { pad(true, BTN_RIGHT), pad(true, BTN_LEFT) };
        in.update(p, dt); CHECK(in.mode() == Mode::TwoPads && in.watergirl().left.isDown);
        p[1] = pad(false); in.update(p, dt);
        CHECK(in.mode() == Mode::OnePad);
        CHECK(!in.watergirl().left.isDown && in.watergirl().left.justUp);        // no stuck input
        CHECK(in.fireboy().right.isDown);
        p[1] = pad(true, BTN_LEFT); in.update(p, dt); CHECK(in.mode() == Mode::TwoPads && in.watergirl().left.isDown);   // re-plug
    }
    { // only pad 2 connected (pad 1 missing) still plays
        Input in; PadState p[2] = { pad(false), pad(true, BTN_RIGHT) };
        in.update(p, dt); CHECK(in.mode() == Mode::OnePad && in.fireboy().right.isDown);
    }
    { // pause = START edge on either pad
        Input in; PadState p[2] = { pad(true), pad(true) };
        in.update(p, dt); CHECK(!in.pausePressed());
        p[1].buttons = BTN_START; in.update(p, dt); CHECK(in.pausePressed());
        in.update(p, dt); CHECK(!in.pausePressed());                              // held -> no repeat
    }
    { // menu: first press immediate, then repeat after delay at rate
        Input in; PadState p[2] = { pad(true), pad(false) };
        in.update(p, dt); CHECK(in.menuEvent() == MenuEvent::None);
        p[0].buttons = BTN_DOWN; in.update(p, dt); CHECK(in.menuEvent() == MenuEvent::Down);
        int repeats = 0; float t = 0;
        for (int i = 0; i < 90; ++i) { in.update(p, dt); t += dt; if (in.menuEvent() == MenuEvent::Down) { ++repeats; CHECK(t >= 0.39f); } }
        CHECK(repeats >= 8 && repeats <= 12);                                     // ~ (1.5s - 0.4s) / 0.12s
        p[0].buttons = BTN_CROSS; in.update(p, dt); CHECK(in.menuEvent() == MenuEvent::Confirm);
        p[0].buttons = 0; in.update(p, dt);
        p[0].buttons = BTN_CIRCLE; in.update(p, dt); CHECK(in.menuEvent() == MenuEvent::Back);
    }
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
