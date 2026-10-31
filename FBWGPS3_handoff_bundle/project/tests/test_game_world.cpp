#include "game_world.hpp"

#include <cmath>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
	if (argc != 2) return 2;
	std::string error;
	fw::progression::ForestCatalog catalog;
	if (!fw::progression::loadForestCatalog(argv[1], catalog, &error) || catalog.levels.empty()) {
		std::cerr << "catalog: " << error << '\n';
		return 2;
	}
	fw::progression::Progress progress;
	fbw::GameWorld world;
	if (!world.start(argv[1], catalog.levels.front(), false, progress, &error)) {
		std::cerr << "world start: " << error << '\n';
		return 1;
	}
	const auto* fb_before = world.body("fb");
	const auto* wg_before = world.body("wg");
	if (!fb_before || !wg_before || fb_before->id != "fb" || wg_before->id != "wg") {
		std::cerr << "world did not create both character bodies\n";
		return 1;
	}
	if (world.platforms().empty() || world.boxes().empty()) {
		std::cerr << "world did not initialize the level's platforms and pushable boxes\n";
		return 1;
	}
	const double x_before = fb_before->x;
	fbw::GameWorldInput input;
	input.fireboy.right = true;
	for (int i = 0; i < 20; ++i) world.update(input, 1.0 / 60.0);
	const auto* fb_after = world.body("fb");
	if (!fb_after || fb_after->x <= x_before) {
		std::cerr << "integrated world did not move Fireboy right\n";
		return 1;
	}
	const double elapsed_before = world.session().elapsedSeconds();
	world.update({}, 0.1);
	if (world.session().elapsedSeconds() <= elapsed_before) {
		std::cerr << "integrated world did not advance the level timer\n";
		return 1;
	}
	if (world.session().gameplay().characters().size() != 2) {
		std::cerr << "gameplay runtime lost character state\n";
		return 1;
	}
	std::cout << "Integrated game world PASS: 2 bodies, platforms, boxes, movement, timer, runtime linked\n";
	return 0;
}
