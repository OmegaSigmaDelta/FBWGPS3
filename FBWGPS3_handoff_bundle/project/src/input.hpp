// Two-controller input layer (platform independent, header-only, no allocations).
//
// The ORIGINAL game only ever reads, per character, three keys: left / right / up (isDown) plus the pause key.
//   desktop layout: Watergirl = W/A/D, Fireboy = Arrow keys  -> the game is natively 2-player.
//   touch layout  : controls == "single": ONE character is active at a time, CursorToggle switches (init.js: "single").
// This module reproduces both on gamepads:
//   TwoPads  : pad[fbPad] drives Fireboy, pad[wgPad] drives Watergirl simultaneously.
//   OnePad   : one pad, L1/R1/Square toggle the active character (the other idles) - same as "single".
// Mode is automatic: both pads connected -> TwoPads, otherwise OnePad. Unplugging a pad mid-level switches to OnePad
// on the next frame without dropping the character the player was driving.
//
// PS3 glue (not in this file): poll ioPadGetInfo2()/ioPadGetData() each frame, fill PadState, call Input::update().
#pragma once
#include <cstdint>
#include <cmath>

namespace fw {

enum Button : uint32_t {
    BTN_UP = 1u << 0, BTN_DOWN = 1u << 1, BTN_LEFT = 1u << 2, BTN_RIGHT = 1u << 3,
    BTN_CROSS = 1u << 4, BTN_CIRCLE = 1u << 5, BTN_SQUARE = 1u << 6, BTN_TRIANGLE = 1u << 7,
    BTN_L1 = 1u << 8, BTN_R1 = 1u << 9, BTN_L2 = 1u << 10, BTN_R2 = 1u << 11,
    BTN_START = 1u << 12, BTN_SELECT = 1u << 13,
};

struct PadState {                // raw state from the platform layer
    bool connected = false;
    uint32_t buttons = 0;        // bitmask of Button
    float lx = 0, ly = 0;        // left stick, -1..1, +y = DOWN (ioPad: convert 0..255 -> (v-128)/128)
};

struct Key { bool isDown = false; bool justDown = false; bool justUp = false; };   // mirrors Phaser.Key

struct Cursors { Key left, right, up; };                                           // what Character.update() reads

enum class Mode { TwoPads, OnePad };
enum class MenuEvent { None, Up, Down, Left, Right, Confirm, Back };

struct Config {
    float stickOn  = 0.50f;      // stick engages above this ...
    float stickOff = 0.30f;      // ... and releases below this (hysteresis, avoids flicker around the threshold)
    float jumpOn   = 0.65f;      // pushing the stick up jumps
    int   fbPad = 0, wgPad = 1;  // which pad drives which character (swap for left-handed players)
    float repeatDelay = 0.40f, repeatRate = 0.12f;   // menu auto-repeat (seconds)
};

class Input {
public:
    Config cfg;

    // Call once per frame. dt in seconds.
    void update(const PadState pads[2], float dt) {
        const bool both = pads[0].connected && pads[1].connected;
        mode_ = both ? Mode::TwoPads : Mode::OnePad;

        for (int p = 0; p < 2; ++p) {
            prevBtn_[p] = btn_[p];
            btn_[p] = pads[p].connected ? pads[p].buttons : 0u;
            // stick -> virtual d-pad bits with hysteresis
            stickLeft_[p]  = hyst(stickLeft_[p],  pads[p].connected && pads[p].lx < -cfg.stickOn,  pads[p].connected && pads[p].lx > -cfg.stickOff);
            stickRight_[p] = hyst(stickRight_[p], pads[p].connected && pads[p].lx >  cfg.stickOn,  pads[p].connected && pads[p].lx <  cfg.stickOff);
            stickUp_[p]    = hyst(stickUp_[p],    pads[p].connected && pads[p].ly < -cfg.jumpOn,   pads[p].connected && pads[p].ly > -cfg.stickOff);
            stickDown_[p]  = hyst(stickDown_[p],  pads[p].connected && pads[p].ly >  cfg.stickOn,  pads[p].connected && pads[p].ly <  cfg.stickOff);
        }

        // ---- gameplay cursors ----
        Cursors next[2];   // [0]=Fireboy, [1]=Watergirl
        if (mode_ == Mode::TwoPads) {
            fill(next[0], cfg.fbPad);
            fill(next[1], cfg.wgPad);
        } else {
            const int p = firstConnected(pads);                 // the single pad (or -1)
            if (p >= 0) {
                if (pressedEdge(p, BTN_L1 | BTN_R1 | BTN_SQUARE)) active_ = 1 - active_;   // CursorToggle
                fill(next[active_], p);                           // only the active character receives input
            }
        }
        for (int c = 0; c < 2; ++c) {
            cur_[c].left  = edge(cur_[c].left,  next[c].left.isDown);
            cur_[c].right = edge(cur_[c].right, next[c].right.isDown);
            cur_[c].up    = edge(cur_[c].up,    next[c].up.isDown);
        }

        // ---- pause: START on either connected pad (edge) ----
        pause_ = pressedEdge(0, BTN_START) || pressedEdge(1, BTN_START);

        // ---- menu navigation (any pad), D-pad/stick with auto-repeat ----
        menuEvent_ = MenuEvent::None;
        uint32_t dir = 0;
        for (int p = 0; p < 2; ++p) {
            if (btn_[p] & BTN_UP   || stickUp_[p])    dir |= BTN_UP;
            if (btn_[p] & BTN_DOWN || stickDown_[p])  dir |= BTN_DOWN;
            if (btn_[p] & BTN_LEFT || stickLeft_[p])  dir |= BTN_LEFT;
            if (btn_[p] & BTN_RIGHT|| stickRight_[p]) dir |= BTN_RIGHT;
        }
        if (dir != 0 && dir == menuDir_) {
            menuHold_ += dt;
            if (menuHold_ >= menuNext_) { menuEvent_ = dirEvent(dir); menuNext_ += cfg.repeatRate; }
        } else {
            menuDir_ = dir; menuHold_ = 0; menuNext_ = cfg.repeatDelay;
            if (dir) menuEvent_ = dirEvent(dir);                   // immediate on first press
        }
        if (menuEvent_ == MenuEvent::None) {
            if (pressedEdge(0, BTN_CROSS) || pressedEdge(1, BTN_CROSS))   menuEvent_ = MenuEvent::Confirm;
            else if (pressedEdge(0, BTN_CIRCLE) || pressedEdge(1, BTN_CIRCLE)) menuEvent_ = MenuEvent::Back;
        }
    }

    const Cursors& fireboy()   const { return cur_[0]; }
    const Cursors& watergirl() const { return cur_[1]; }
    Mode mode() const { return mode_; }
    int  activeCharacter() const { return active_; }          // OnePad only: 0 = Fireboy, 1 = Watergirl
    void setActiveCharacter(int c) { active_ = c ? 1 : 0; }
    bool pausePressed() const { return pause_; }
    MenuEvent menuEvent() const { return menuEvent_; }

private:
    Mode mode_ = Mode::OnePad;
    int active_ = 0;
    uint32_t btn_[2] = {0, 0}, prevBtn_[2] = {0, 0};
    bool stickLeft_[2] = {}, stickRight_[2] = {}, stickUp_[2] = {}, stickDown_[2] = {};
    Cursors cur_[2];
    bool pause_ = false;
    MenuEvent menuEvent_ = MenuEvent::None;
    uint32_t menuDir_ = 0; float menuHold_ = 0, menuNext_ = 0;

    static bool hyst(bool state, bool on, bool released) { return state ? !released : on; }
    bool pressedEdge(int p, uint32_t mask) const { return ((btn_[p] & mask) != 0) && ((prevBtn_[p] & mask) == 0); }
    static Key edge(const Key& old, bool down) { Key k; k.isDown = down; k.justDown = down && !old.isDown; k.justUp = !down && old.isDown; return k; }
    static MenuEvent dirEvent(uint32_t d) {
        if (d & BTN_UP)    return MenuEvent::Up;
        if (d & BTN_DOWN)  return MenuEvent::Down;
        if (d & BTN_LEFT)  return MenuEvent::Left;
        if (d & BTN_RIGHT) return MenuEvent::Right;
        return MenuEvent::None;
    }
    static int firstConnected(const PadState pads[2]) { return pads[0].connected ? 0 : (pads[1].connected ? 1 : -1); }
    void fill(Cursors& c, int p) const {
        c.left.isDown  = (btn_[p] & BTN_LEFT)  || stickLeft_[p];
        c.right.isDown = (btn_[p] & BTN_RIGHT) || stickRight_[p];
        c.up.isDown    = (btn_[p] & (BTN_CROSS | BTN_UP)) || stickUp_[p];     // Cross / D-pad up / stick up = jump
        if (c.left.isDown && c.right.isDown) c.left.isDown = c.right.isDown = false;   // opposite directions cancel
    }
};

} // namespace fw
