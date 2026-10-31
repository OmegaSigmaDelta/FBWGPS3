#pragma once

#include "render_queue.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace fbw {

struct TextureRegion {
	std::string atlas_key;
	SourceRect source;
	int content_w = 0;
	int content_h = 0;
	int original_w = 0;
	int original_h = 0;
};

struct ImageRGBA {
	int width = 0;
	int height = 0;
	std::vector<std::uint8_t> pixels;

	bool loadPng(const std::string& path, std::string* error = nullptr);
	bool savePng(const std::string& path, std::string* error = nullptr) const;
};

class SoftwareRenderer {
public:
	bool render(const RenderQueue& queue, const std::map<std::string, ImageRGBA>& atlases,
		int width, int height, ImageRGBA& output, std::string* error = nullptr) const;
	// Draws a queue onto an existing image without clearing it. Commands are translated by
	// (-offset_x, -offset_y), so a world-space queue can be drawn into a camera-sized frame.
	bool drawQueueOnto(const RenderQueue& queue, const std::map<std::string, ImageRGBA>& atlases,
		ImageRGBA& target, double offset_x, double offset_y, std::string* error = nullptr) const;
	bool renderTexturedLevel(const RenderQueue& queue,
		const std::map<std::string, ImageRGBA>& atlases,
		const TextureRegion& background, const TextureRegion& foreground,
		int width, int height, ImageRGBA& output, std::string* error = nullptr) const;
};

} // namespace fbw
