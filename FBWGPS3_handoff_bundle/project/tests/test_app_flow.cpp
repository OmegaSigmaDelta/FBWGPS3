// Drives fbw::App only through simulated pad samples (the same path the PS3 layer uses) and checks the whole flow.
#include "app.hpp"
#include "ps3_pad.hpp"
#include <cstdio>
#include <iostream>

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++failures; } } while (0)

struct Pads {
	fw::Input input; fw::ps3::RawPad raw[2];
	fbw::App* app;
	void frame(double dt = 1.0 / 60.0) {
		fw::PadState st[2] = {fw::ps3::toPadState(raw[0]), fw::ps3::toPadState(raw[1])};
		input.update(st, static_cast<float>(dt)); app->update(input, dt);
	}
	void tap(int pad, bool d1, uint8_t bit) {   // press for 2 frames then release for 2
		(d1 ? raw[pad].digital1 : raw[pad].digital2) |= bit; frame(); frame();
		(d1 ? raw[pad].digital1 : raw[pad].digital2) &= static_cast<uint8_t>(~bit); frame(); frame();
	}
};

int main(int argc, char** argv) {
	const std::string game = argc > 1 ? argv[1] : "game";
	const std::string save = argc > 2 ? argv[2] : "app_flow_save.json";
	std::remove(save.c_str());
	fbw::AppConfig cfg; cfg.game_dir = game; cfg.save_path = save;
	fbw::App app; std::string error;
	if (!app.init(cfg, error)) { std::cerr << "init: " << error << "\n"; return 1; }
	Pads pads; pads.app = &app;
	pads.raw[0].connected = true;   // one pad only

	CHECK(app.inMenu(), "starts in menu");
	const size_t first = app.levelIndex();
	pads.tap(0, true, fw::ps3::D1_DOWN);
	CHECK(app.levelIndex() != first, "D-pad down moves the menu selection");
	pads.tap(0, true, fw::ps3::D1_UP);
	CHECK(app.levelIndex() == first, "D-pad up moves back");
	// a locked level must refuse to start
	for (int i = 0; i < 5; ++i) pads.tap(0, true, fw::ps3::D1_DOWN);
	pads.tap(0, false, fw::ps3::D2_CROSS);
	CHECK(app.inMenu(), "locked level does not start");
	for (int i = 0; i < 5; ++i) pads.tap(0, true, fw::ps3::D1_UP);
	CHECK(app.levelIndex() == first, "back on the first level");
	pads.tap(0, false, fw::ps3::D2_CROSS);
	CHECK(!app.inMenu(), "Cross starts the unlocked first level");
	CHECK(app.world().body("fb") && app.world().body("wg"), "both hero bodies exist");

	// walk Fireboy right for a second: only the active hero (Fireboy) should move in one-pad mode
	const double fx0 = app.world().body("fb")->x, wx0 = app.world().body("wg")->x;
	pads.raw[0].digital1 |= fw::ps3::D1_RIGHT; for (int i = 0; i < 60; ++i) pads.frame(); pads.raw[0].digital1 = 0;
	CHECK(app.world().body("fb")->x > fx0 + 20, "active hero walks right");
	CHECK(std::abs(app.world().body("wg")->x - wx0) < 5, "inactive hero idles in one-pad mode");
	// switch hero with L1, walk Watergirl
	pads.tap(0, false, fw::ps3::D2_L1);
	const double wx1 = app.world().body("wg")->x;
	pads.raw[0].digital1 |= fw::ps3::D1_RIGHT; for (int i = 0; i < 60; ++i) pads.frame(); pads.raw[0].digital1 = 0;
	CHECK(app.world().body("wg")->x > wx1 + 20, "L1 hands control to Watergirl");

	// pause freezes the world
	pads.tap(0, true, fw::ps3::D1_START);
	CHECK(app.paused(), "START pauses");
	const double frozen = app.world().body("wg")->x; pads.raw[0].digital1 |= fw::ps3::D1_RIGHT; for (int i = 0; i < 30; ++i) pads.frame(); pads.raw[0].digital1 = 0;
	CHECK(app.world().body("wg")->x == frozen, "paused world does not move");
	pads.tap(0, true, fw::ps3::D1_START);
	CHECK(!app.paused(), "START resumes");
	// pause -> circle goes back to the menu and writes the save
	pads.tap(0, true, fw::ps3::D1_START); pads.tap(0, false, fw::ps3::D2_CIRCLE);
	CHECK(app.inMenu(), "Circle from pause returns to the menu");
	FILE* f = std::fopen(save.c_str(), "rb"); CHECK(f != nullptr, "save file written when returning to the menu"); if (f) std::fclose(f);

	// two pads: both heroes respond
	pads.raw[1].connected = true; pads.frame();
	pads.tap(0, false, fw::ps3::D2_CROSS);
	CHECK(!app.inMenu(), "level starts with two pads");
	const double a0 = app.world().body("fb")->x, b0 = app.world().body("wg")->x;
	pads.raw[0].digital1 |= fw::ps3::D1_RIGHT; pads.raw[1].digital1 |= fw::ps3::D1_RIGHT; for (int i = 0; i < 60; ++i) pads.frame();
	CHECK(app.world().body("fb")->x > a0 + 20 && app.world().body("wg")->x > b0 + 20, "two pads move both heroes");

	// rendering works in every state
	fbw::ImageRGBA frame; CHECK(app.render(frame, &error), "render gameplay: " << error);
	CHECK(frame.width == 1280 && frame.height == 720 && frame.pixels.size() == 1280u * 720u * 4u, "frame size is 1280x720 RGBA");
	app.shutdown();
	std::remove(save.c_str());
	std::cout << "app flow " << (failures ? "FAILED" : "OK") << "\n";
	return failures ? 1 : 0;
}
