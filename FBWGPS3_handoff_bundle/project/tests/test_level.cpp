#include "level.hpp"
#include "ground.hpp"
#include <cstdio>
#include <exception>
#include <string>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::fprintf(stderr, "usage: test_level <game-dir>\n");
		return 2;
	}
	try {
		const std::string gameDir = argv[1];
		const fw::Json catalog = fw::loadJson(gameDir + "/data/forest/temple.json");
		const fw::Json& entries = catalog["levels"];
		if (!entries.isArr() || entries.size() != 32) {
			std::fprintf(stderr, "expected 32 levels; found %zu\n", entries.size());
			return 1;
		}
		std::size_t totalFixtures = 0, totalObjects = 0, totalChars = 0;
		for (const fw::Json& entry : entries.a) {
			const std::string id = std::to_string(entry["id"].i32());
			const std::string file = entry["filename"].str();
			if (file.empty()) {
				std::fprintf(stderr, "level %s has no filename\n", id.c_str());
				return 1;
			}
			const fw::Level level = fw::loadLevel(gameDir, file);
			if (level.w <= 0 || level.h <= 0 || level.gid.size() != static_cast<std::size_t>(level.w * level.h)) {
				std::fprintf(stderr, "level %s has invalid dimensions/data: %s\n", id.c_str(), file.c_str());
				return 1;
			}
			const fw::GroundResult ground = fw::buildGround(level);
			totalFixtures += ground.fixtures.size();
			totalObjects += level.objects.size();
			totalChars += level.chars.size();
			std::printf("OK level %2s: %-36s %2dx%-2d tiles=%zu fixtures=%zu objects=%zu chars=%zu\n",
				id.c_str(), file.c_str(), level.w, level.h, level.gid.size(), ground.fixtures.size(),
				level.objects.size(), level.chars.size());
		}
		std::printf("PASS: 32/32 levels loaded; fixtures=%zu objects=%zu chars=%zu\n", totalFixtures, totalObjects, totalChars);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "ERROR: %s\n", e.what());
		return 1;
	}
	return 0;
}
