#pragma once

#include "atlas.hpp"
#include "ground.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace fbw {

enum class RenderLayer : int {
	Background = 0,
	Ground = 100,
	Liquids = 200,
	Devices = 300,
	Characters = 400,
	Effects = 500,
	Ui = 600
};

struct SourceRect {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

struct RenderCommand {
	std::string atlas_key;
	std::string image;
	std::string frame_name;
	SourceRect source;
	int source_content_w = 0;
	int source_content_h = 0;
	int original_w = 0;
	int original_h = 0;
	double x = 0.0;
	double y = 0.0;
	double w = 0.0;
	double h = 0.0;
	double pivot_x = 0.0;
	double pivot_y = 0.0;
	double rotation_degrees = 0.0;
	bool rotated_in_atlas = false;
	bool flip_x = false;
	bool flip_y = false;
	RenderLayer layer = RenderLayer::Ground;
	std::uint64_t sequence = 0;
};

// Engine-neutral draw list. A platform backend consumes commands in sorted order
// and translates source/destination rectangles to SDL_RenderCopyEx or RSX blits.
class RenderQueue {
public:
	bool appendFrame(const Atlas& atlas, const std::string& atlas_key,
		const std::string& frame_name, double center_x, double center_y,
		double scale, RenderLayer layer, std::string* error = nullptr,
		double rotation_degrees = 0.0, bool flip_x = false, bool flip_y = false) {
		return appendFrameScaled(atlas, atlas_key, frame_name, center_x, center_y,
			scale, scale, layer, error, rotation_degrees, flip_x, flip_y);
	}

	// Independent X/Y scaling is required by the original tileSprite-based
	// devices (platform bars, ropes and stretched light strips).
	bool appendFrameScaled(const Atlas& atlas, const std::string& atlas_key,
		const std::string& frame_name, double center_x, double center_y,
		double scale_x, double scale_y, RenderLayer layer, std::string* error = nullptr,
		double rotation_degrees = 0.0, bool flip_x = false, bool flip_y = false) {
		if (!std::isfinite(center_x) || !std::isfinite(center_y) ||
			!std::isfinite(scale_x) || !std::isfinite(scale_y) ||
			scale_x <= 0.0 || scale_y <= 0.0 || !std::isfinite(rotation_degrees))
			return fail(error, "invalid sprite transform for frame: " + frame_name);
		const AtlasFrame* frame = atlas.find(frame_name);
		if (!frame) return fail(error, "frame not found in atlas " + atlas_key + ": " + frame_name);

		RenderCommand cmd;
		cmd.atlas_key = atlas_key;
		cmd.image = atlas.image;
		cmd.frame_name = frame_name;
		cmd.source = {frame->x, frame->y, frame->w, frame->h};
		cmd.source_content_w = frame->source_w;
		cmd.source_content_h = frame->source_h;
		cmd.original_w = frame->original_w;
		cmd.original_h = frame->original_h;
		const int crop_x = flip_x ? frame->original_w - frame->source_x - frame->source_w : frame->source_x;
		const int crop_y = flip_y ? frame->original_h - frame->source_y - frame->source_h : frame->source_y;
		cmd.x = center_x - frame->original_w * scale_x * 0.5 + crop_x * scale_x;
		cmd.y = center_y - frame->original_h * scale_y * 0.5 + crop_y * scale_y;
		cmd.w = frame->source_w * scale_x;
		cmd.h = frame->source_h * scale_y;
		cmd.pivot_x = center_x;
		cmd.pivot_y = center_y;
		cmd.rotation_degrees = rotation_degrees;
		cmd.rotated_in_atlas = frame->rotated;
		cmd.flip_x = flip_x;
		cmd.flip_y = flip_y;
		cmd.layer = layer;
		cmd.sequence = next_sequence_++;
		commands_.push_back(std::move(cmd));
		if (error) error->clear();
		return true;
	}

	// Places a sprite into an explicit untrimmed destination rectangle while
	// preserving TexturePacker trim offsets in the generated source crop.
	bool appendFrameRect(const Atlas& atlas, const std::string& atlas_key,
		const std::string& frame_name, double left, double top, double width,
		double height, RenderLayer layer, std::string* error = nullptr,
		double rotation_degrees = 0.0, bool flip_x = false, bool flip_y = false) {
		if (!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(width) ||
			!std::isfinite(height) || width <= 0.0 || height <= 0.0)
			return fail(error, "invalid destination rectangle for frame: " + frame_name);
		const AtlasFrame* frame = atlas.find(frame_name);
		if (!frame) return fail(error, "frame not found in atlas " + atlas_key + ": " + frame_name);
		return appendFrameScaled(atlas, atlas_key, frame_name,
			left + width * 0.5, top + height * 0.5,
			width / frame->original_w, height / frame->original_h,
			layer, error, rotation_degrees, flip_x, flip_y);
	}

	// Reproduces Ground.drawTile's canvas placement formula. The original uses
	// random variants for frameType 0; a coordinate hash makes the native output
	// stable across reloads while distributing the four variants.
	bool appendGround(const fw::GroundResult& ground, const Atlas& atlas,
		const std::string& atlas_key = "GroundAssets", int tile_w = 32,
		int tile_h = 32, std::uint32_t variant_seed = 0,
		std::string* error = nullptr) {
		if (tile_w <= 0 || tile_h <= 0) return fail(error, "ground tile dimensions must be positive");
		for (const fw::DrawOp& draw : ground.draws) {
			std::string frame_name;
			if (draw.frameType == 0) {
				const std::uint32_t tx = static_cast<std::uint32_t>(draw.px / tile_w);
				const std::uint32_t ty = static_cast<std::uint32_t>(draw.py / tile_h);
				const std::uint32_t hash = tx * 0x9E3779B1u ^ ty * 0x85EBCA77u ^ variant_seed;
				frame_name = "MaskBox0000" + std::to_string(hash % 4u);
			} else {
				frame_name = "MaskBox" + std::to_string(draw.frameType) + "0000";
			}

			const AtlasFrame* frame = atlas.find(frame_name);
			if (!frame) return fail(error, "ground frame missing: " + frame_name);

			RenderCommand cmd;
			cmd.atlas_key = atlas_key;
			cmd.image = atlas.image;
			cmd.frame_name = frame_name;
			cmd.source = {frame->x, frame->y, frame->w, frame->h};
			cmd.source_content_w = frame->source_w;
			cmd.source_content_h = frame->source_h;
			cmd.original_w = frame->original_w;
			cmd.original_h = frame->original_h;
			// Ground is first baked into a bitmap whose origin is later placed at
			// (-32,-32) by the original Phaser scene. Convert that canvas position
			// back to world coordinates here (subtracting the 32px bitmap border).
			cmd.x = static_cast<double>(draw.px) - (frame->w - tile_w) * 0.5;
			cmd.y = static_cast<double>(draw.py) - (frame->h - tile_h) * 0.5;
			cmd.w = frame->w;
			cmd.h = frame->h;
			cmd.pivot_x = cmd.x + cmd.w * 0.5;
			cmd.pivot_y = cmd.y + cmd.h * 0.5;
			cmd.rotated_in_atlas = frame->rotated;
			cmd.layer = RenderLayer::Ground;
			cmd.sequence = next_sequence_++;
			commands_.push_back(std::move(cmd));
		}
		if (error) error->clear();
		return true;
	}

	// Adds the animated liquid surface sprites from Devices/Liquid. The original
	// animation runs at 30 fps with 15 frames per loop. A frozen-state entry may
	// be supplied per GroundResult::liquids item; otherwise type 55 starts frozen.
	bool appendLiquids(const fw::GroundResult& ground, const Atlas& atlas,
		const std::string& atlas_key, double elapsed_seconds,
		const std::vector<bool>& frozen_states = {}, int tile_w = 32, int tile_h = 32,
		std::string* error = nullptr) {
		if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0 || tile_w <= 0 || tile_h <= 0)
			return fail(error, "invalid liquid animation time or tile dimensions");
		if (!frozen_states.empty() && frozen_states.size() != ground.liquids.size())
			return fail(error, "frozen-state count must match the number of liquid runs");
		const int animation_frame = static_cast<int>(std::floor(elapsed_seconds * 30.0)) % 15;
		auto frameNumber = [](int frame) {
			std::string value = std::to_string(frame);
			while (value.size() < 4) value.insert(value.begin(), '0');
			return value;
		};
		for (size_t index = 0; index < ground.liquids.size(); ++index) {
			const fw::LiquidDef& liquid = ground.liquids[index];
			std::string prefix;
			if (liquid.type == 5 || liquid.type == 55) prefix = "WaterBox";
			else if (liquid.type == 6) prefix = "FireBox";
			else if (liquid.type == 7) prefix = "GreenBox";
			else return fail(error, "unsupported liquid tile type: " + std::to_string(liquid.type));
			if (liquid.width <= 0) return fail(error, "liquid width must be positive");

			const bool frozen = frozen_states.empty() ? liquid.type == 55 : frozen_states[index];
			if (frozen && liquid.type != 5 && liquid.type != 55)
				return fail(error, "only water liquid runs can use IceBox graphics");
			const double center_y = (static_cast<double>(liquid.ty) + 0.5) * tile_h + (frozen ? 4.0 : 2.0);
			const std::string suffix = frozen ? "0000" : frameNumber(animation_frame);
			const std::string liquid_prefix = frozen ? "IceBox" : prefix;

			auto appendAt = [&](const std::string& frame, int tile_offset) {
				const double center_x = (static_cast<double>(liquid.tx + tile_offset) + 0.5) * tile_w;
				return appendFrame(atlas, atlas_key, frame, center_x, center_y, 1.0,
					RenderLayer::Liquids, error);
			};
			if (!appendAt(liquid_prefix + "Left" + suffix, 0)) return false;
			for (int tile = 1; tile < liquid.width - 1; ++tile) {
				if (!appendAt(liquid_prefix + suffix, tile)) return false;
			}
			if (!appendAt(liquid_prefix + "Right" + suffix, liquid.width - 1)) return false;
		}
		if (error) error->clear();
		return true;
	}

	const std::vector<RenderCommand>& commands() const { return commands_; }

	std::vector<RenderCommand> ordered() const {
		auto result = commands_;
		std::stable_sort(result.begin(), result.end(), [](const RenderCommand& a, const RenderCommand& b) {
			if (a.layer != b.layer) return static_cast<int>(a.layer) < static_cast<int>(b.layer);
			return a.sequence < b.sequence;
		});
		return result;
	}

	// Appends an already-built command (used to filter/re-route commands made elsewhere).
	void push(RenderCommand command) {
		command.sequence = next_sequence_++;
		commands_.push_back(std::move(command));
	}

	void clear() {
		commands_.clear();
		next_sequence_ = 0;
	}

private:
	static bool fail(std::string* error, const std::string& message) {
		if (error) *error = message;
		return false;
	}

	std::vector<RenderCommand> commands_;
	std::uint64_t next_sequence_ = 0;
};

} // namespace fbw
