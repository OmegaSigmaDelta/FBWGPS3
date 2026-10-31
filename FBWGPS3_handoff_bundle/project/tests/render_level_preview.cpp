#include "atlas.hpp"
#include "ground.hpp"
#include "level.hpp"
#include "software_renderer.hpp"
#include "world_render.hpp"

#include <iostream>
#include <map>
#include <string>

int main(int argc, char** argv) {
	if (argc != 4) {
		std::cerr << "usage: render_level_preview <game-dir> <level-relative-path> <output.png>\n";
		return 2;
	}
	const std::string root = argv[1];
	const std::string level_path = argv[2];
	const std::string output_path = argv[3];
	std::string error;
	try {
		const fw::Level level = fw::loadLevel(root, level_path);
		const fw::GroundResult ground = fw::buildGround(level);
		fbw::Atlas ground_atlas, char_atlas, mech_atlas, temple_atlas;
		const std::string atlas_dir = root + "/assets/atlasses/";
		if (!ground_atlas.load(atlas_dir + "GroundAssets.json", &error) ||
			!char_atlas.load(atlas_dir + "CharAssets.json", &error) ||
			!mech_atlas.load(atlas_dir + "MechAssets.json", &error) ||
			!temple_atlas.load(atlas_dir + "Temples/forest/TempleAssets.json", &error)) {
			std::cerr << "ERROR: " << error << "\n";
			return 1;
		}

		fbw::RenderQueue queue;
		if (!queue.appendGround(ground, ground_atlas, "GroundAssets", level.tileW, level.tileH, 1234, &error) ||
			!queue.appendLiquids(ground, ground_atlas, "GroundAssets", 0.0, {}, level.tileW, level.tileH, &error)) {
			std::cerr << "ERROR: " << error << "\n";
			return 1;
		}
		fbw::WorldRenderStats stats;
		if (!fbw::appendWorldObjects(level, char_atlas, mech_atlas, temple_atlas, queue, &stats, &error)) {
			std::cerr << "ERROR: " << error << "\n";
			return 1;
		}

		std::map<std::string, fbw::ImageRGBA> images;
		for (const auto& item : std::vector<std::pair<std::string, std::string>>{
			{"GroundAssets", atlas_dir + "GroundAssets.png"},
			{"CharAssets", atlas_dir + "CharAssets.png"},
			{"MechAssets", atlas_dir + "MechAssets.png"},
			{"TempleAssets", atlas_dir + "Temples/forest/TempleAssets.png"}}) {
			fbw::ImageRGBA image;
			if (!image.loadPng(item.second, &error)) {
				std::cerr << "ERROR: " << error << "\n";
				return 1;
			}
			images.emplace(item.first, std::move(image));
		}

		auto region = [](const fbw::Atlas& atlas, const std::string& key, const std::string& name) {
			const fbw::AtlasFrame* frame = atlas.find(name);
			if (!frame) throw std::runtime_error("missing atlas frame " + name);
			return fbw::TextureRegion{key, {frame->x, frame->y, frame->w, frame->h},
				frame->source_w, frame->source_h, frame->original_w, frame->original_h};
		};
		const fbw::TextureRegion background = region(temple_atlas, "TempleAssets", "BackGround0000");
		const fbw::TextureRegion foreground = region(temple_atlas, "TempleAssets", "ForeGround0000");
		fbw::ImageRGBA output;
		fbw::SoftwareRenderer renderer;
		if (!renderer.renderTexturedLevel(queue, images, background, foreground,
			level.w * level.tileW, level.h * level.tileH, output, &error) ||
			!output.savePng(output_path, &error)) {
			std::cerr << "ERROR: " << error << "\n";
			return 1;
		}
		std::cout << "Rendered " << level_path << " to " << output_path << " ("
			<< output.width << "x" << output.height << ", commands=" << queue.commands().size()
			<< ", characters=" << stats.characters << ", doors=" << stats.doors
			<< ", diamonds=" << stats.diamonds << ", devices=" << stats.devices << ")\n";
	} catch (const std::exception& exception) {
		std::cerr << "ERROR: " << exception.what() << "\n";
		return 1;
	}
	return 0;
}
