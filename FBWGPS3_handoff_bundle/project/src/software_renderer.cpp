#include "software_renderer.hpp"

#include <png.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fbw {

namespace {
bool fail(std::string* error, const std::string& message) {
	if (error) *error = message;
	return false;
}

inline std::uint8_t clampByte(int value) {
	return static_cast<std::uint8_t>(std::max(0, std::min(255, value)));
}

void blendPixel(std::uint8_t* dst, const std::uint8_t* src) {
	const int alpha = src[3];
	if (alpha == 255) {
		dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 255;
		return;
	}
	if (alpha == 0) return;
	const int inverse = 255 - alpha;
	const int out_alpha = alpha + (static_cast<int>(dst[3]) * inverse + 127) / 255;
	if (out_alpha == 0) return;
	for (int c = 0; c < 3; ++c) {
		const int premultiplied = static_cast<int>(src[c]) * alpha +
			(static_cast<int>(dst[c]) * dst[3] * inverse + 127) / 255;
		dst[c] = clampByte((premultiplied + out_alpha / 2) / out_alpha);
	}
	dst[3] = clampByte(out_alpha);
}

void drawCommand(const RenderCommand& command, const ImageRGBA& atlas, ImageRGBA& target) {
	if (command.source_content_w <= 0 || command.source_content_h <= 0 ||
		command.w <= 0.0 || command.h <= 0.0) return;
	const double radians = command.rotation_degrees * 3.14159265358979323846 / 180.0;
	const double cosine = std::cos(radians);
	const double sine = std::sin(radians);
	const double corners_x[4] = {command.x, command.x + command.w, command.x, command.x + command.w};
	const double corners_y[4] = {command.y, command.y, command.y + command.h, command.y + command.h};
	double min_x = 1e30, min_y = 1e30, max_x = -1e30, max_y = -1e30;
	for (int i = 0; i < 4; ++i) {
		const double dx = corners_x[i] - command.pivot_x;
		const double dy = corners_y[i] - command.pivot_y;
		const double x = command.pivot_x + dx * cosine - dy * sine;
		const double y = command.pivot_y + dx * sine + dy * cosine;
		min_x = std::min(min_x, x); min_y = std::min(min_y, y);
		max_x = std::max(max_x, x); max_y = std::max(max_y, y);
	}
	const int left = std::max(0, static_cast<int>(std::floor(min_x)));
	const int top = std::max(0, static_cast<int>(std::floor(min_y)));
	const int right = std::min(target.width, static_cast<int>(std::ceil(max_x)));
	const int bottom = std::min(target.height, static_cast<int>(std::ceil(max_y)));
	const double content_w = static_cast<double>(command.source_content_w);
	const double content_h = static_cast<double>(command.source_content_h);
	for (int y = top; y < bottom; ++y) {
		for (int x = left; x < right; ++x) {
			const double dx = (x + 0.5) - command.pivot_x;
			const double dy = (y + 0.5) - command.pivot_y;
			const double unrotated_x = dx * cosine + dy * sine + command.pivot_x;
			const double unrotated_y = -dx * sine + dy * cosine + command.pivot_y;
			double local_x = unrotated_x - command.x;
			double local_y = unrotated_y - command.y;
			if (local_x < 0.0 || local_y < 0.0 || local_x >= command.w || local_y >= command.h) continue;
			if (command.flip_x) local_x = command.w - local_x;
			if (command.flip_y) local_y = command.h - local_y;
			const int u = std::min(command.source_content_w - 1,
				std::max(0, static_cast<int>(local_x / command.w * content_w)));
			const int v = std::min(command.source_content_h - 1,
				std::max(0, static_cast<int>(local_y / command.h * content_h)));
			int source_x = command.source.x + u;
			int source_y = command.source.y + v;
			if (command.rotated_in_atlas) {
				// TexturePacker JSON-ARRAY stores rotated frames clockwise.
				source_x = command.source.x + (command.source_content_h - 1 - v);
				source_y = command.source.y + u;
			}
			if (source_x < 0 || source_y < 0 || source_x >= atlas.width || source_y >= atlas.height) continue;
			const std::uint8_t* src = &atlas.pixels[(static_cast<size_t>(source_y) * atlas.width + source_x) * 4];
			std::uint8_t* dst = &target.pixels[(static_cast<size_t>(y) * target.width + x) * 4];
			blendPixel(dst, src);
		}
	}
}
} // namespace


namespace {
bool validRegion(const TextureRegion& region, const std::map<std::string, ImageRGBA>& atlases,
	std::string* error) {
	const auto it = atlases.find(region.atlas_key);
	if (it == atlases.end()) return fail(error, "missing texture atlas: " + region.atlas_key);
	if (region.content_w <= 0 || region.content_h <= 0 ||
		region.source.x < 0 || region.source.y < 0 ||
		region.source.x + region.content_w > it->second.width ||
		region.source.y + region.content_h > it->second.height)
		return fail(error, "texture region is outside atlas: " + region.atlas_key);
	return true;
}

void tileRegion(const TextureRegion& region, const ImageRGBA& texture, ImageRGBA& target) {
	for (int y = 0; y < target.height; ++y) {
		const int source_y = region.source.y + (y % region.content_h);
		for (int x = 0; x < target.width; ++x) {
			const int source_x = region.source.x + (x % region.content_w);
			const std::uint8_t* src = &texture.pixels[(static_cast<size_t>(source_y) * texture.width + source_x) * 4];
			std::uint8_t* dst = &target.pixels[(static_cast<size_t>(y) * target.width + x) * 4];
			blendPixel(dst, src);
		}
	}
}

void applyMaskedTile(const TextureRegion& region, const ImageRGBA& texture,
	const ImageRGBA& mask, ImageRGBA& target) {
	for (int y = 0; y < target.height; ++y) {
		const int source_y = region.source.y + (y % region.content_h);
		for (int x = 0; x < target.width; ++x) {
			const std::uint8_t mask_alpha = mask.pixels[(static_cast<size_t>(y) * mask.width + x) * 4 + 3];
			if (mask_alpha == 0) continue;
			const int source_x = region.source.x + (x % region.content_w);
			const std::uint8_t* source = &texture.pixels[(static_cast<size_t>(source_y) * texture.width + source_x) * 4];
			std::uint8_t masked[4] = {source[0], source[1], source[2],
				static_cast<std::uint8_t>((static_cast<unsigned int>(source[3]) * mask_alpha + 127u) / 255u)};
			std::uint8_t* destination = &target.pixels[(static_cast<size_t>(y) * target.width + x) * 4];
			blendPixel(destination, masked);
		}
	}
}
} // namespace

bool ImageRGBA::loadPng(const std::string& path, std::string* error) {
	png_image image{};
	image.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&image, path.c_str()))
		return fail(error, "cannot open PNG " + path + ": " + image.message);
	image.format = PNG_FORMAT_RGBA;
	std::vector<std::uint8_t> decoded(PNG_IMAGE_SIZE(image));
	if (!png_image_finish_read(&image, nullptr, decoded.data(), 0, nullptr)) {
		const std::string message = image.message;
		png_image_free(&image);
		return fail(error, "cannot decode PNG " + path + ": " + message);
	}
	width = static_cast<int>(image.width);
	height = static_cast<int>(image.height);
	pixels = std::move(decoded);
	png_image_free(&image);
	if (error) error->clear();
	return true;
}

bool ImageRGBA::savePng(const std::string& path, std::string* error) const {
	if (width <= 0 || height <= 0 || pixels.size() != static_cast<size_t>(width) * height * 4)
		return fail(error, "invalid RGBA image for PNG output");
	png_image image{};
	image.version = PNG_IMAGE_VERSION;
	image.width = static_cast<png_uint_32>(width);
	image.height = static_cast<png_uint_32>(height);
	image.format = PNG_FORMAT_RGBA;
	if (!png_image_write_to_file(&image, path.c_str(), 0, pixels.data(), 0, nullptr))
		return fail(error, "cannot write PNG " + path + ": " + image.message);
	if (error) error->clear();
	return true;
}

bool SoftwareRenderer::render(const RenderQueue& queue,
	const std::map<std::string, ImageRGBA>& atlases, int width, int height,
	ImageRGBA& output, std::string* error) const {
	if (width <= 0 || height <= 0) return fail(error, "render target dimensions must be positive");
	output.width = width;
	output.height = height;
	output.pixels.assign(static_cast<size_t>(width) * height * 4, 0);
	for (size_t i = 0; i < output.pixels.size(); i += 4) {
		output.pixels[i] = 20;
		output.pixels[i + 1] = 24;
		output.pixels[i + 2] = 32;
		output.pixels[i + 3] = 255;
	}
	for (const RenderCommand& command : queue.ordered()) {
		const auto atlas = atlases.find(command.atlas_key);
		if (atlas == atlases.end()) return fail(error, "no decoded atlas registered for key: " + command.atlas_key);
		drawCommand(command, atlas->second, output);
	}
	if (error) error->clear();
	return true;
}


bool SoftwareRenderer::drawQueueOnto(const RenderQueue& queue,
	const std::map<std::string, ImageRGBA>& atlases, ImageRGBA& target,
	double offset_x, double offset_y, std::string* error) const {
	if (target.width <= 0 || target.height <= 0 ||
		target.pixels.size() != static_cast<size_t>(target.width) * target.height * 4)
		return fail(error, "draw target is not a valid RGBA image");
	for (RenderCommand command : queue.ordered()) {
		const auto atlas = atlases.find(command.atlas_key);
		if (atlas == atlases.end()) return fail(error, "no decoded atlas registered for key: " + command.atlas_key);
		command.x -= offset_x; command.y -= offset_y;
		command.pivot_x -= offset_x; command.pivot_y -= offset_y;
		drawCommand(command, atlas->second, target);
	}
	if (error) error->clear();
	return true;
}

bool SoftwareRenderer::renderTexturedLevel(const RenderQueue& queue,
	const std::map<std::string, ImageRGBA>& atlases,
	const TextureRegion& background, const TextureRegion& foreground,
	int width, int height, ImageRGBA& output, std::string* error) const {
	if (width <= 0 || height <= 0) return fail(error, "render target dimensions must be positive");
	if (!validRegion(background, atlases, error) || !validRegion(foreground, atlases, error)) return false;
	output.width = width;
	output.height = height;
	output.pixels.assign(static_cast<size_t>(width) * height * 4, 0);
	for (size_t i = 0; i < output.pixels.size(); i += 4) {
		output.pixels[i] = 20;
		output.pixels[i + 1] = 24;
		output.pixels[i + 2] = 32;
		output.pixels[i + 3] = 255;
	}

	const auto& background_texture = atlases.at(background.atlas_key);
	const auto& foreground_texture = atlases.at(foreground.atlas_key);
	tileRegion(background, background_texture, output);

	const std::vector<RenderCommand> ordered = queue.ordered();
	for (const RenderCommand& command : ordered) {
		if (command.layer != RenderLayer::Background) continue;
		const auto atlas = atlases.find(command.atlas_key);
		if (atlas == atlases.end()) return fail(error, "no decoded atlas registered for key: " + command.atlas_key);
		drawCommand(command, atlas->second, output);
	}

	// GroundAssets' MaskBox frames are not the final colour art: Phaser draws
	// them into an alpha bitmap, then applies that mask to a tiled ForeGround.
	ImageRGBA mask;
	mask.width = width;
	mask.height = height;
	mask.pixels.assign(static_cast<size_t>(width) * height * 4, 0);
	for (const RenderCommand& command : ordered) {
		if (command.layer != RenderLayer::Ground) continue;
		const auto atlas = atlases.find(command.atlas_key);
		if (atlas == atlases.end()) return fail(error, "no decoded atlas registered for key: " + command.atlas_key);
		drawCommand(command, atlas->second, mask);
	}
	applyMaskedTile(foreground, foreground_texture, mask, output);

	for (const RenderCommand& command : ordered) {
		if (command.layer == RenderLayer::Background || command.layer == RenderLayer::Ground) continue;
		const auto atlas = atlases.find(command.atlas_key);
		if (atlas == atlases.end()) return fail(error, "no decoded atlas registered for key: " + command.atlas_key);
		drawCommand(command, atlas->second, output);
	}
	if (error) error->clear();
	return true;
}

} // namespace fbw
