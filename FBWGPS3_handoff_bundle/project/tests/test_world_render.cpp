#include "atlas.hpp"
#include "level.hpp"
#include "world_render.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: test_world_render <game-root>\n";
		return 2;
	}
	const std::string root = argv[1];
	std::string error;
	fbw::Atlas chars, mech, temple;
	if (!chars.load(root + "/assets/atlasses/CharAssets.json", &error) ||
		!mech.load(root + "/assets/atlasses/MechAssets.json", &error) ||
		!temple.load(root + "/assets/atlasses/Temples/forest/TempleAssets.json", &error)) {
		std::cerr << "FAIL: " << error << "\n";
		return 1;
	}

	const fw::Json catalog = fw::loadJson(root + "/data/forest/temple.json");
	if (!catalog["levels"].isArr() || catalog["levels"].size() != 32) {
		std::cerr << "FAIL: expected 32 catalog levels\n";
		return 1;
	}

	fbw::RenderQueue queue;
	fbw::WorldRenderStats totals;
	for (const fw::Json& entry : catalog["levels"].a) {
		const std::string rel = entry["filename"].str();
		const fw::Level level = fw::loadLevel(root, rel);
		fbw::WorldRenderStats stats;
		if (!fbw::appendWorldObjects(level, chars, mech, temple, queue, &stats, &error)) {
			std::cerr << "FAIL: " << rel << ": " << error << "\n";
			return 1;
		}
		if (stats.unsupported != 0) {
			std::cerr << "FAIL: " << rel << " has " << stats.unsupported << " unsupported object types\n";
			return 1;
		}
		totals.characters += stats.characters;
		totals.doors += stats.doors;
		totals.diamonds += stats.diamonds;
		totals.devices += stats.devices;
	}
	for (const auto& command : queue.commands()) {
		if (command.frame_name.empty() || command.source.w <= 0 || command.source.h <= 0 ||
			command.w <= 0.0 || command.h <= 0.0) {
			std::cerr << "FAIL: invalid object render command\n";
			return 1;
		}
	}
	if (totals.characters < 2 || totals.doors < 2 || totals.diamonds == 0 || totals.devices == 0) {
		std::cerr << "FAIL: expected character, door, diamond and device render output\n";
		return 1;
	}

	// Confirm Tiled polyline/polygon geometry survives loading for hanging,
	// pulley and Roman devices; these paths drive the static rope/bar render pass.
	bool saw_polyline = false;
	bool saw_polygon = false;
	for (const fw::Json& entry : catalog["levels"].a) {
		const fw::Level level = fw::loadLevel(root, entry["filename"].str());
		for (const auto& object : level.objects) {
			if (object.type == "hanging" || object.type == "pulley") saw_polyline |= object.polyline.size() >= 2;
			if (object.type == "roman") saw_polygon |= object.polygon.size() >= 3;
		}
	}
	if (!saw_polyline || !saw_polygon) {
		std::cerr << "FAIL: Tiled object geometry was not preserved\n";
		return 1;
	}

	std::cout << "PASS: all 32 levels; characters=" << totals.characters
		<< " doors=" << totals.doors << " diamonds=" << totals.diamonds
		<< " devices=" << totals.devices << " commands=" << queue.commands().size()
		<< "; all current object types mapped; polyline/polygon geometry retained\n";
	return 0;
}
