// Locks in the object/mechanism inventory of the 32 Forest Temple maps as seen through the project's own loader.
// The expected numbers were obtained independently with a Python script that resolves Tiled gids through each map's
// tilesets (see docs/MECHANISM_AUDIT.md). If the loader and that script ever disagree, this test fails.
#include "level.hpp"

#include <cstdio>
#include <exception>
#include <map>
#include <string>

namespace {
int failures = 0;
void expectCount(const std::map<std::string, int>& counts, const std::string& key, int expected) {
	auto it = counts.find(key);
	const int actual = it == counts.end() ? 0 : it->second;
	if (actual != expected) {
		std::printf("FAIL %-22s expected %3d, loader found %3d\n", key.c_str(), expected, actual);
		++failures;
	}
}
}

int main(int argc, char** argv) {
	if (argc != 2) { std::fprintf(stderr, "usage: test_mechanism_inventory <game-dir>\n"); return 2; }
	try {
		const std::string gameDir = argv[1];
		const fw::Json catalog = fw::loadJson(gameDir + "/data/forest/temple.json");
		std::map<std::string, int> objects, chars;
		int rotatingPlatforms = 0, fullRotationHangers = 0, leversWithStartPos = 0;
		for (const fw::Json& entry : catalog["levels"].a) {
			const fw::Level level = fw::loadLevel(gameDir, entry["filename"].str());
			for (const fw::MapObject& o : level.objects) {
				++objects[o.type];
				if (o.type == "platform" && o.props.has("dtheta")) ++rotatingPlatforms;
				if (o.type == "hanging" && o.props["fullRotation"].boolean(false)) ++fullRotationHangers;
				// Every lever carries onis/startPos: they come from per-variant tileset defaults, not from the map.
				if (o.type == "lever" && o.props["onis"].num() != 0.0 && o.props["startPos"].num() != 0.0) ++leversWithStartPos;
			}
			for (const fw::MapObject& o : level.chars) ++chars[o.type];
		}
		for (const auto& kv : objects) std::printf("objects  %-12s %3d\n", kv.first.c_str(), kv.second);
		for (const auto& kv : chars) std::printf("chars    %-12s %3d\n", kv.first.c_str(), kv.second);

		expectCount(objects, "ball", 12);
		expectCount(objects, "box", 14);
		expectCount(objects, "lever", 49);
		expectCount(objects, "pusher", 47);
		expectCount(objects, "hanging", 8);
		expectCount(objects, "platform", 98);
		expectCount(objects, "pulley", 6);
		expectCount(objects, "roman", 3);
		expectCount(objects, "wind", 24);
		// Device types that exist in the tileset but are never placed in any of the 32 maps.
		for (const char* unused : {"beamer", "rotmirror", "lightpusher", "boxmirror", "boxheavy", "infinitemirror", "timedpusher"})
			expectCount(objects, unused, 0);
		if (rotatingPlatforms != 12) { std::printf("FAIL rotating platforms expected 12, found %d\n", rotatingPlatforms); ++failures; }
		if (fullRotationHangers != 6) { std::printf("FAIL fullRotation hangers expected 6, found %d\n", fullRotationHangers); ++failures; }
		if (leversWithStartPos != 49) { std::printf("FAIL levers with onis/startPos expected 49, found %d\n", leversWithStartPos); ++failures; }
	} catch (const std::exception& e) {
		std::fprintf(stderr, "ERROR: %s\n", e.what());
		return 1;
	}
	if (failures) { std::printf("%d check(s) failed\n", failures); return 1; }
	std::printf("PASS: mechanism inventory matches the independent audit\n");
	return 0;
}
