#pragma once

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fbw {

struct BitmapGlyph {
	int id = -1;
	int x = 0, y = 0, width = 0, height = 0;
	int xoffset = 0, yoffset = 0, xadvance = 0;
	int page = 0, channel = 0;
};

struct BitmapKerning {
	int first = -1, second = -1, amount = 0;
};

class BitmapFont {
public:
	int line_height = 0;
	int base = 0;
	int scale_w = 0;
	int scale_h = 0;
	int declared_pages = 0;
	std::vector<std::string> pages;
	std::unordered_map<int, BitmapGlyph> glyphs;
	std::vector<BitmapKerning> kernings;

	bool load(const std::string& path, std::string* error = nullptr) {
		std::ifstream in(path);
		if (!in) return fail(error, "cannot open BMFont descriptor: " + path);
		BitmapFont parsed;
		std::string line;
		int line_number = 0;
		while (std::getline(in, line)) {
			++line_number;
			const auto first = line.find_first_not_of(" \t\r");
			if (first == std::string::npos) continue;
			line = line.substr(first);
			const auto split = line.find_first_of(" \t");
			const std::string kind = line.substr(0, split);
			const auto fields = parseFields(split == std::string::npos ? std::string() : line.substr(split + 1));
			auto number = [&fields](const char* key, int fallback = -1) {
				auto it = fields.find(key);
				if (it == fields.end()) return fallback;
				try { size_t used = 0; int value = std::stoi(it->second, &used); return used == it->second.size() ? value : fallback; }
				catch (...) { return fallback; }
			};
			if (kind == "common") {
				parsed.line_height = number("lineHeight"); parsed.base = number("base");
				parsed.scale_w = number("scaleW"); parsed.scale_h = number("scaleH");
				parsed.declared_pages = number("pages");
			} else if (kind == "page") {
				const int id = number("id");
				auto file = fields.find("file");
				if (id < 0 || file == fields.end()) return fail(error, "invalid page record at line " + std::to_string(line_number));
				if (static_cast<size_t>(id) >= parsed.pages.size()) parsed.pages.resize(static_cast<size_t>(id) + 1);
				if (!parsed.pages[static_cast<size_t>(id)].empty()) return fail(error, "duplicate page id at line " + std::to_string(line_number));
				parsed.pages[static_cast<size_t>(id)] = file->second;
			} else if (kind == "char") {
				BitmapGlyph g;
				g.id = number("id"); g.x = number("x"); g.y = number("y");
				g.width = number("width"); g.height = number("height");
				g.xoffset = number("xoffset"); g.yoffset = number("yoffset");
				g.xadvance = number("xadvance"); g.page = number("page", 0); g.channel = number("chnl", 0);
				if (g.id < 0 || g.x < 0 || g.y < 0 || g.width < 0 || g.height < 0 ||
					g.xoffset < -32768 || g.yoffset < -32768 || g.xadvance < 0 || g.page < 0)
					return fail(error, "invalid glyph record at line " + std::to_string(line_number));
				if (!parsed.glyphs.emplace(g.id, g).second) return fail(error, "duplicate glyph id at line " + std::to_string(line_number));
			} else if (kind == "kerning") {
				BitmapKerning k{number("first"), number("second"), number("amount")};
				if (k.first < 0 || k.second < 0 || k.amount == -1 && fields.find("amount") == fields.end())
					return fail(error, "invalid kerning record at line " + std::to_string(line_number));
				parsed.kernings.push_back(k);
			}
		}
		if (parsed.line_height <= 0 || parsed.scale_w <= 0 || parsed.scale_h <= 0 || parsed.declared_pages <= 0)
			return fail(error, "BMFont common metrics are missing or invalid");
		if (parsed.pages.size() != static_cast<size_t>(parsed.declared_pages))
			return fail(error, "BMFont page count does not match common.pages");
		for (const auto& page : parsed.pages) if (page.empty()) return fail(error, "BMFont page IDs are not contiguous");
		if (parsed.glyphs.empty()) return fail(error, "BMFont contains no glyphs");
		for (const auto& entry : parsed.glyphs) {
			const auto& g = entry.second;
			if (g.page >= parsed.declared_pages || g.x + g.width > parsed.scale_w || g.y + g.height > parsed.scale_h)
				return fail(error, "glyph rectangle exceeds page bounds: " + std::to_string(g.id));
		}
		line_height = parsed.line_height; base = parsed.base; scale_w = parsed.scale_w; scale_h = parsed.scale_h;
		declared_pages = parsed.declared_pages; pages = std::move(parsed.pages);
		glyphs = std::move(parsed.glyphs); kernings = std::move(parsed.kernings);
		if (error) error->clear();
		return true;
	}

	const BitmapGlyph* glyph(int codepoint) const {
		const auto it = glyphs.find(codepoint);
		return it == glyphs.end() ? nullptr : &it->second;
	}

private:
	static bool fail(std::string* error, const std::string& message) {
		if (error) *error = message;
		return false;
	}
	static std::unordered_map<std::string, std::string> parseFields(const std::string& input) {
		std::unordered_map<std::string, std::string> out;
		size_t i = 0;
		while (i < input.size()) {
			while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i]))) ++i;
			const size_t key_start = i;
			while (i < input.size() && input[i] != '=' && !std::isspace(static_cast<unsigned char>(input[i]))) ++i;
			if (i >= input.size() || input[i] != '=') { while (i < input.size() && !std::isspace(static_cast<unsigned char>(input[i]))) ++i; continue; }
			const std::string key = input.substr(key_start, i - key_start);
			++i;
			std::string value;
			if (i < input.size() && input[i] == '"') {
				++i;
				while (i < input.size() && input[i] != '"') value += input[i++];
				if (i < input.size()) ++i;
			} else {
				const size_t value_start = i;
				while (i < input.size() && !std::isspace(static_cast<unsigned char>(input[i]))) ++i;
				value = input.substr(value_start, i - value_start);
			}
			out[key] = value;
		}
		return out;
	}
};

} // namespace fbw
