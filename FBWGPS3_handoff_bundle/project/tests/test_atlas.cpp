#include "atlas.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: test_atlas <game-root>\n";
		return 2;
	}
	const std::string root = argv[1];
	const std::vector<std::string> paths = {
		"assets/atlasses/CharAssets.json",
		"assets/atlasses/GroundAssets.json",
		"assets/atlasses/MechAssets.json",
		"assets/atlasses/MenuAssets.json",
		"assets/atlasses/MenuBackgrounds.json",
		"assets/atlasses/PopupAssets.json",
		"assets/atlasses/PreloaderAssets.json",
		"assets/atlasses/Temples/forest/TempleAssets.json"
	};
	size_t total_frames = 0;
	for (const auto& relative : paths) {
		fbw::Atlas atlas;
		std::string error;
		const std::string path = root + "/" + relative;
		if (!atlas.load(path, &error)) {
			std::cerr << "FAIL: " << relative << ": " << error << "\n";
			return 1;
		}
		if (atlas.frames.empty()) {
			std::cerr << "FAIL: no frames in " << relative << "\n";
			return 1;
		}
		for (const auto& frame : atlas.frames) {
			if (atlas.find(frame.name) == nullptr) {
				std::cerr << "FAIL: lookup failed for " << frame.name << " in " << relative << "\n";
				return 1;
			}
		}
		total_frames += atlas.frames.size();
		std::cout << "OK: " << relative << " image=" << atlas.image << " size="
			<< atlas.image_w << "x" << atlas.image_h << " frames=" << atlas.frames.size() << "\n";
	}

	fbw::Atlas chars;
	std::string error;
	if (!chars.load(root + "/assets/atlasses/CharAssets.json", &error)) {
		std::cerr << error << "\n";
		return 1;
	}
	if (!chars.find("_default0000") || !chars.find("death_smoke0000")) {
		std::cerr << "FAIL: known character atlas frames missing\n";
		return 1;
	}
	std::cout << "PASS: " << paths.size() << " atlases, " << total_frames << " frames validated\n";
	return 0;
}
