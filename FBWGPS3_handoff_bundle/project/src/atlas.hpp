#pragma once

#include "json.hpp"

#include <fstream>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fbw {

struct AtlasFrame {
	std::string name;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	bool rotated = false;
	bool trimmed = false;
	int source_x = 0;
	int source_y = 0;
	int source_w = 0;
	int source_h = 0;
	int original_w = 0;
	int original_h = 0;
};

class Atlas {
public:
	std::string image;
	int image_w = 0;
	int image_h = 0;
	std::vector<AtlasFrame> frames;
	std::unordered_map<std::string, size_t> frame_index;

	bool load(const std::string& path, std::string* error = nullptr) {
		std::ifstream file(path, std::ios::binary);
		if (!file) return fail(error, "cannot open atlas JSON: " + path);
		std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		std::string parse_error;
		fw::Json root = fw::Json::parse(text, &parse_error);
		if (!parse_error.empty()) return fail(error, "invalid atlas JSON: " + parse_error);
		if (!root["frames"].isArr()) return fail(error, "atlas frames must be an array");
		const fw::Json& meta = root["meta"];
		if (!meta.isObj() || !meta["size"].isObj()) return fail(error, "atlas meta.size is missing");

		const std::string parsed_image = meta["image"].str();
		const int parsed_w = meta["size"]["w"].i32(-1);
		const int parsed_h = meta["size"]["h"].i32(-1);
		if (parsed_image.empty() || parsed_w <= 0 || parsed_h <= 0)
			return fail(error, "atlas image name or dimensions are invalid");

		std::vector<AtlasFrame> parsed_frames;
		std::unordered_map<std::string, size_t> names;
		parsed_frames.reserve(root["frames"].size());
		for (size_t i = 0; i < root["frames"].size(); ++i) {
			const fw::Json& entry = root["frames"][i];
			const fw::Json& rect = entry["frame"];
			const fw::Json& source = entry["spriteSourceSize"];
			const fw::Json& original = entry["sourceSize"];
			AtlasFrame f;
			f.name = entry["filename"].str();
			f.x = rect["x"].i32(-1); f.y = rect["y"].i32(-1);
			f.w = rect["w"].i32(-1); f.h = rect["h"].i32(-1);
			f.rotated = entry["rotated"].boolean(false);
			f.trimmed = entry["trimmed"].boolean(false);
			f.source_x = source["x"].i32(-1); f.source_y = source["y"].i32(-1);
			f.source_w = source["w"].i32(-1); f.source_h = source["h"].i32(-1);
			f.original_w = original["w"].i32(-1); f.original_h = original["h"].i32(-1);
			if (f.name.empty() || !rect.isObj() || !source.isObj() || !original.isObj())
				return fail(error, "frame " + std::to_string(i) + " is missing required fields");
			if (f.x < 0 || f.y < 0 || f.w <= 0 || f.h <= 0 || f.x + f.w > parsed_w || f.y + f.h > parsed_h)
				return fail(error, "frame rectangle is outside atlas bounds: " + f.name);
			if (f.source_x < 0 || f.source_y < 0 || f.source_w <= 0 || f.source_h <= 0 ||
				f.original_w <= 0 || f.original_h <= 0 ||
				f.source_x + f.source_w > f.original_w || f.source_y + f.source_h > f.original_h)
				return fail(error, "invalid source dimensions for frame: " + f.name);
			if (names.find(f.name) != names.end()) return fail(error, "duplicate frame name: " + f.name);
			names.emplace(f.name, parsed_frames.size());
			parsed_frames.push_back(std::move(f));
		}

		const size_t slash = path.find_last_of("/\\");
		const std::string directory = slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
		const std::string png_path = directory + parsed_image;
		std::ifstream png(png_path, std::ios::binary);
		unsigned char header[24] = {};
		if (!png.read(reinterpret_cast<char*>(header), sizeof(header)))
			return fail(error, "cannot read atlas PNG header: " + png_path);
		const unsigned char signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
		for (int i = 0; i < 8; ++i)
			if (header[i] != signature[i]) return fail(error, "atlas image is not a PNG: " + png_path);
		auto be32 = [&header](int offset) -> uint32_t {
			return (uint32_t(header[offset]) << 24) | (uint32_t(header[offset + 1]) << 16) |
				(uint32_t(header[offset + 2]) << 8) | uint32_t(header[offset + 3]);
		};
		if (be32(16) != static_cast<uint32_t>(parsed_w) || be32(20) != static_cast<uint32_t>(parsed_h))
			return fail(error, "atlas PNG dimensions do not match metadata: " + png_path);

		image = parsed_image;
		image_w = parsed_w;
		image_h = parsed_h;
		frames = std::move(parsed_frames);
		frame_index = std::move(names);
		if (error) error->clear();
		return true;
	}

	const AtlasFrame* find(const std::string& name) const {
		const auto it = frame_index.find(name);
		return it == frame_index.end() ? nullptr : &frames[it->second];
	}

private:
	static bool fail(std::string* error, const std::string& message) {
		if (error) *error = message;
		return false;
	}
};

} // namespace fbw
