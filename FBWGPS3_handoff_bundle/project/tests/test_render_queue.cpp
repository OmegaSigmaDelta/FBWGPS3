#include "atlas.hpp"
#include "ground.hpp"
#include "level.hpp"
#include "render_queue.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {
bool near(double a, double b) { return std::abs(a - b) < 0.0001; }
}

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: test_render_queue <game-root>\n";
		return 2;
	}
	const std::string root = argv[1];
	std::string error;

	fbw::Atlas ground_atlas;
	if (!ground_atlas.load(root + "/assets/atlasses/GroundAssets.json", &error)) {
		std::cerr << "FAIL: " << error << "\n";
		return 1;
	}

	const fw::Json catalog = fw::loadJson(root + "/data/forest/temple.json");
	if (!catalog["levels"].isArr() || catalog["levels"].size() != 32) {
		std::cerr << "FAIL: expected 32 catalog levels\n";
		return 1;
	}
	fbw::RenderQueue queue;
	size_t total_draws = 0;
	size_t total_liquid_sprites = 0;
	for (const fw::Json& entry : catalog["levels"].a) {
		const std::string relative = entry["filename"].str();
		const fw::Level level = fw::loadLevel(root, relative);
		const fw::GroundResult ground = fw::buildGround(level);
		const size_t before = queue.commands().size();
		if (!queue.appendGround(ground, ground_atlas, "GroundAssets", level.tileW, level.tileH, 1234, &error)) {
			std::cerr << "FAIL: " << relative << ": " << error << "\n";
			return 1;
		}
		if (queue.commands().size() - before != ground.draws.size()) {
			std::cerr << "FAIL: render command count mismatch in " << relative << "\n";
			return 1;
		}
		if (!ground.draws.empty()) {
			const auto& first_draw = ground.draws.front();
			const auto& first_command = queue.commands()[before];
			const fbw::AtlasFrame* frame = ground_atlas.find(first_command.frame_name);
			if (!frame || !near(first_command.x,
				static_cast<double>(first_draw.px) - (frame->w - level.tileW) * 0.5) ||
				!near(first_command.y,
				static_cast<double>(first_draw.py) - (frame->h - level.tileH) * 0.5)) {
				std::cerr << "FAIL: ground canvas border offset is not converted to world coordinates\n";
				return 1;
			}
		}
		const size_t liquid_before = queue.commands().size();
		if (!queue.appendLiquids(ground, ground_atlas, "GroundAssets", 0.25, {}, level.tileW, level.tileH, &error)) {
			std::cerr << "FAIL: liquid rendering in " << relative << ": " << error << "\n";
			return 1;
		}
		size_t expected_liquid_sprites = 0;
		for (const auto& liquid : ground.liquids)
			expected_liquid_sprites += static_cast<size_t>(liquid.width < 2 ? 2 : liquid.width);
		if (queue.commands().size() - liquid_before != expected_liquid_sprites) {
			std::cerr << "FAIL: liquid sprite count mismatch in " << relative << "\n";
			return 1;
		}
		total_liquid_sprites += expected_liquid_sprites;
		total_draws += ground.draws.size();
	}
	if (queue.commands().size() != total_draws + total_liquid_sprites || total_draws == 0) {
		std::cerr << "FAIL: not all ground draw ops were converted\n";
		return 1;
	}
	for (const auto& command : queue.commands()) {
		if (command.image != ground_atlas.image ||
			(command.layer != fbw::RenderLayer::Ground && command.layer != fbw::RenderLayer::Liquids) ||
			command.source.w <= 0 || command.source.h <= 0 || command.w <= 0 || command.h <= 0) {
			std::cerr << "FAIL: malformed render command\n";
			return 1;
		}
	}

	// Verify the 30 fps animation frame and frozen-water replacement.
	fw::GroundResult liquid_fixture;
	liquid_fixture.liquids.push_back({5, 2, 3, 3});
	fbw::RenderQueue liquid_queue;
	if (!liquid_queue.appendLiquids(liquid_fixture, ground_atlas, "GroundAssets", 0.1, {}, 32, 32, &error)) {
		std::cerr << "FAIL: animated liquid fixture: " << error << "\n";
		return 1;
	}
	if (liquid_queue.commands().size() != 3 ||
		liquid_queue.commands()[0].frame_name != "WaterBoxLeft0003" ||
		liquid_queue.commands()[1].frame_name != "WaterBox0003" ||
		liquid_queue.commands()[2].frame_name != "WaterBoxRight0003") {
		std::cerr << "FAIL: liquid animation frame selection is incorrect\n";
		return 1;
	}
	liquid_queue.clear();
	if (liquid_queue.appendLiquids(liquid_fixture, ground_atlas, "GroundAssets", 2.0, {true, false}, 32, 32, &error)) {
		std::cerr << "FAIL: mismatched liquid state count should be rejected\n";
		return 1;
	}
	if (!liquid_queue.appendLiquids(liquid_fixture, ground_atlas, "GroundAssets", 2.0, {true}, 32, 32, &error) ||
		liquid_queue.commands().size() != 3 ||
		liquid_queue.commands()[0].frame_name != "IceBoxLeft0000" ||
		liquid_queue.commands()[1].frame_name != "IceBox0000" ||
		liquid_queue.commands()[2].frame_name != "IceBoxRight0000") {
		std::cerr << "FAIL: frozen liquid frame selection is incorrect\n";
		return 1;
	}

	// Verify trimmed-frame placement: source crop offsets must remain inside the
	// original untrimmed sprite rectangle while the pivot stays at its centre.
	fbw::Atlas fixture;
	fixture.image = "fixture.png";
	fbw::AtlasFrame trimmed;
	trimmed.name = "trimmed";
	trimmed.x = 2; trimmed.y = 3; trimmed.w = 6; trimmed.h = 5;
	trimmed.source_x = 4; trimmed.source_y = 2;
	trimmed.source_w = 6; trimmed.source_h = 5;
	trimmed.original_w = 16; trimmed.original_h = 12;
	trimmed.rotated = true;
	fixture.frames.push_back(trimmed);
	fixture.frame_index.emplace(trimmed.name, 0);
	if (!queue.appendFrame(fixture, "Fixture", "trimmed", 100.0, 50.0, 2.0,
		fbw::RenderLayer::Characters, &error, 15.0, true, false)) {
		std::cerr << "FAIL: " << error << "\n";
		return 1;
	}
	const auto& sprite = queue.commands().back();
	if (!near(sprite.x, 96.0) || !near(sprite.y, 42.0) ||
		!near(sprite.w, 12.0) || !near(sprite.h, 10.0) ||
		!near(sprite.pivot_x, 100.0) || !near(sprite.pivot_y, 50.0) ||
		sprite.rotation_degrees != 15.0 || !sprite.flip_x || sprite.flip_y) {
		std::cerr << "FAIL: trimmed sprite transform is incorrect\n";
		return 1;
	}

	fbw::AtlasFrame rotated;
	rotated.name = "rotated";
	rotated.x = 0; rotated.y = 0; rotated.w = 5; rotated.h = 6;
	rotated.source_x = 0; rotated.source_y = 0;
	rotated.source_w = 6; rotated.source_h = 5;
	rotated.original_w = 6; rotated.original_h = 5;
	rotated.rotated = true;
	fixture.frames.push_back(rotated);
	fixture.frame_index.emplace(rotated.name, 1);
	if (!queue.appendFrame(fixture, "Fixture", "rotated", 20.0, 30.0, 1.0,
		fbw::RenderLayer::Effects, &error) ||
		!near(queue.commands().back().w, 6.0) || !near(queue.commands().back().h, 5.0) ||
		!queue.commands().back().rotated_in_atlas) {
		std::cerr << "FAIL: rotated atlas frame dimensions are incorrect\n";
		return 1;
	}

	const auto ordered = queue.ordered();
	if (ordered.size() != queue.commands().size() || ordered.back().layer != fbw::RenderLayer::Effects) {
		std::cerr << "FAIL: render layer ordering is incorrect\n";
		return 1;
	}
	for (size_t i = 1; i < ordered.size(); ++i) {
		if (static_cast<int>(ordered[i - 1].layer) > static_cast<int>(ordered[i].layer)) {
			std::cerr << "FAIL: ordered commands are not layer-sorted\n";
			return 1;
		}
	}

	queue.clear();
	if (!queue.commands().empty()) {
		std::cerr << "FAIL: clear() did not empty render queue\n";
		return 1;
	}
	std::cout << "PASS: all 32 levels; " << total_draws << " ground draw ops and "
		<< total_liquid_sprites << " liquid sprites converted; animation/frozen frames, trimmed transforms and layer sorting verified\n";
	return 0;
}
