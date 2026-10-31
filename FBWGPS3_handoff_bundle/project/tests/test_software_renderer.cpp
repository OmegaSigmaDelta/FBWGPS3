#include "software_renderer.hpp"

#include <cstdint>
#include <iostream>
#include <map>
#include <string>

namespace {
bool pixelEquals(const fbw::ImageRGBA& image, int x, int y,
	std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
	const size_t offset = (static_cast<size_t>(y) * image.width + x) * 4;
	return image.pixels[offset] == r && image.pixels[offset + 1] == g &&
		image.pixels[offset + 2] == b && image.pixels[offset + 3] == a;
}
}

int main() {
	fbw::Atlas atlas;
	atlas.image = "fixture.png";
	atlas.image_w = 2;
	atlas.image_h = 2;
	fbw::AtlasFrame frame;
	frame.name = "pixel_test";
	frame.x = 0; frame.y = 0; frame.w = 2; frame.h = 2;
	frame.source_x = 0; frame.source_y = 0;
	frame.source_w = 2; frame.source_h = 2;
	frame.original_w = 2; frame.original_h = 2;
	atlas.frames.push_back(frame);
	atlas.frame_index.emplace(frame.name, 0);

	fbw::ImageRGBA source;
	source.width = 2;
	source.height = 2;
	source.pixels = {
		255, 0, 0, 255,    0, 255, 0, 128,
		0, 0, 255, 255,    255, 255, 255, 0
	};
	std::map<std::string, fbw::ImageRGBA> atlases;
	atlases.emplace("Fixture", source);
	std::string error;
	fbw::SoftwareRenderer renderer;
	fbw::RenderQueue queue;
	fbw::ImageRGBA output;

	if (!queue.appendFrame(atlas, "Fixture", "pixel_test", 1.0, 1.0, 1.0,
		fbw::RenderLayer::Characters, &error) ||
		!renderer.render(queue, atlases, 2, 2, output, &error)) {
		std::cerr << "FAIL: normal draw: " << error << "\n";
		return 1;
	}
	if (!pixelEquals(output, 0, 0, 255, 0, 0) ||
		!pixelEquals(output, 1, 0, 10, 140, 16) ||
		!pixelEquals(output, 0, 1, 0, 0, 255) ||
		!pixelEquals(output, 1, 1, 20, 24, 32)) {
		std::cerr << "FAIL: source sampling or alpha compositing result mismatch\n";
		return 1;
	}

	queue.clear();
	if (!queue.appendFrame(atlas, "Fixture", "pixel_test", 1.0, 1.0, 1.0,
		fbw::RenderLayer::Characters, &error, 0.0, true, false) ||
		!renderer.render(queue, atlases, 2, 2, output, &error)) {
		std::cerr << "FAIL: horizontal flip draw: " << error << "\n";
		return 1;
	}
	if (!pixelEquals(output, 0, 0, 10, 140, 16) || !pixelEquals(output, 1, 0, 255, 0, 0)) {
		std::cerr << "FAIL: horizontal flip result mismatch\n";
		return 1;
	}

	queue.clear();
	if (!queue.appendFrame(atlas, "Fixture", "pixel_test", 1.0, 1.0, 1.0,
		fbw::RenderLayer::Characters, &error, 90.0, false, false) ||
		!renderer.render(queue, atlases, 2, 2, output, &error)) {
		std::cerr << "FAIL: rotated draw: " << error << "\n";
		return 1;
	}
	if (!pixelEquals(output, 0, 0, 0, 0, 255) || !pixelEquals(output, 1, 0, 255, 0, 0) ||
		!pixelEquals(output, 0, 1, 20, 24, 32) || !pixelEquals(output, 1, 1, 10, 140, 16)) {
		std::cerr << "FAIL: 90-degree rotation result mismatch\n";
		return 1;
	}

	std::cout << "PASS: software renderer RGBA sampling, alpha blend, horizontal flip and rotation\n";
	return 0;
}
