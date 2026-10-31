// Composes real gameplay frames on the CPU and checks them. Optional 2nd arg: output dir for PNG inspection.
#include "frame_composer.hpp"
#include <iostream>
#include <set>

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++failures; } } while (0)

int main(int argc, char** argv) {
	const std::string game = argc > 1 ? argv[1] : "game";
	const std::string out_dir = argc > 2 ? argv[2] : "";
	std::string error;
	fbw::FrameAssets assets;
	if (!fbw::loadFrameAssets(game, assets, error)) { std::cerr << "assets: " << error << "\n"; return 1; }
	fw::progression::ForestCatalog catalog;
	if (!fw::progression::loadForestCatalog(game, catalog, &error)) { std::cerr << "catalog: " << error << "\n"; return 1; }
	fw::progression::Progress progress;
	const int W = 1280, H = 720;   // PS3 720p
	int checked = 0;
	for (size_t i = 0; i < catalog.levels.size(); ++i) {
		const auto& level = catalog.levels[i];
		fbw::GameWorld world;
		if (!world.start(game, level, false, progress, &error)) { std::cerr << "start " << level.rules.id << ": " << error << "\n"; return 1; }
		if (!fbw::buildLevelImage(game, level, assets, error)) { std::cerr << "bake " << level.rules.id << ": " << error << "\n"; return 1; }
		fbw::GameWorldInput in; in.fireboy.right = true; in.watergirl.left = true;
		for (int f = 0; f < 30; ++f) world.update(in, 1.0 / 60.0);
		fbw::GameplayFrameParams p; p.level_id = level.rules.id; p.level_type = level.rules.type; p.last_input = in;
		p.view = fbw::cameraFor(world, false, "fb", assets.level_image.width, assets.level_image.height, W, H);
		fbw::ImageRGBA frame; frame.width = W; frame.height = H;
		if (!fbw::composeGameplayFrame(world, assets, p, frame, &error)) { std::cerr << "compose " << level.rules.id << ": " << error << "\n"; return 1; }
		std::set<uint32_t> colours; size_t bg_like = 0;
		for (size_t k = 0; k < frame.pixels.size(); k += 4 * 37) {
			const uint32_t c = frame.pixels[k] << 16 | frame.pixels[k + 1] << 8 | frame.pixels[k + 2]; colours.insert(c);
			if (c == (15u << 16 | 22u << 8 | 29u)) ++bg_like;
		}
		CHECK(colours.size() > 40, "level " << level.rules.id << " frame looks flat (" << colours.size() << " colours)");
		CHECK(bg_like < (frame.pixels.size() / (4 * 37)) / 2, "level " << level.rules.id << " is mostly empty fill");
		++checked;
		if (!out_dir.empty() && (level.rules.id == 1 || level.rules.id == 7 || level.rules.id == 15)) {
			// level 15 also with a pause banner for the overlay path
			if (level.rules.id == 15) { p.paused = true; fbw::composeGameplayFrame(world, assets, p, frame, &error); }
			frame.savePng(out_dir + "/frame_level_" + std::to_string(level.rules.id) + ".png", &error);
		}
	}
	std::cout << "composed " << checked << " levels at " << W << "x" << H << (failures ? " WITH FAILURES" : " OK") << "\n";
	return failures ? 1 : 0;
}
