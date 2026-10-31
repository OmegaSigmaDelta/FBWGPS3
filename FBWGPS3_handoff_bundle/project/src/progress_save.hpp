#pragma once

#include "progression.hpp"
#include "json.hpp"

#include <cstdio>
#include <cmath>
#include <set>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace fw { namespace progression {

static constexpr int SAVE_VERSION = 1;

inline Json jsonNumber(double value) {
	Json json; json.type = Json::Num; json.n = value; return json;
}
inline Json jsonInteger(int value) { return jsonNumber(static_cast<double>(value)); }
inline Json jsonBoolean(bool value) {
	Json json; json.type = Json::Bool; json.b = value; return json;
}
inline Json jsonString(const std::string& value) {
	Json json; json.type = Json::Str; json.s = value; return json;
}
inline Json jsonObject() { Json json; json.type = Json::Obj; return json; }
inline Json jsonArray() { Json json; json.type = Json::Arr; return json; }

inline Json encodeProgress(const std::string& game_id, const Progress& progress) {
	Json root = jsonObject();
	root.o.emplace_back("version", jsonInteger(SAVE_VERSION));
	root.o.emplace_back("game", jsonString(game_id));
	Json levels = jsonArray();
	std::set<int> ids;
	for (const auto& entry : progress.played_by_level) ids.insert(entry.first);
	for (const auto& entry : progress.best_by_level) ids.insert(entry.first);
	for (int id : ids) {
		if (id <= 0) continue;
		Json level = jsonObject();
		level.o.emplace_back("id", jsonInteger(id));
		auto played = progress.played_by_level.find(id);
		level.o.emplace_back("played", jsonBoolean(played != progress.played_by_level.end() && played->second));
		auto best = progress.best_by_level.find(id);
		if (best == progress.best_by_level.end() || !best->second.valid) {
			level.o.emplace_back("best", Json{});
		} else {
			const RunResult& run = best->second.run;
			Json value = jsonObject();
			value.o.emplace_back("stars", jsonInteger(run.stars));
			value.o.emplace_back("time", jsonNumber(run.elapsed_seconds));
			value.o.emplace_back("diamonds", jsonInteger(run.diamonds));
			value.o.emplace_back("silverDiamond", jsonInteger(run.silver_diamonds));
			level.o.emplace_back("best", std::move(value));
		}
		levels.a.push_back(std::move(level));
	}
	root.o.emplace_back("levels", std::move(levels));
	return root;
}

inline bool decodeProgress(const std::string& encoded, const std::string& expected_game_id,
	Progress& output, std::string* error = nullptr) {
	std::string parse_error;
	const Json root = Json::parse(encoded, &parse_error);
	if (!parse_error.empty()) return fail(error, "invalid save JSON: " + parse_error);
	if (!root.isObj() || root["version"].i32() != SAVE_VERSION)
		return fail(error, "unsupported or missing save version");
	if (root["game"].str() != expected_game_id)
		return fail(error, "save belongs to a different game");
	if (!root["levels"].isArr()) return fail(error, "save levels must be an array");

	Progress parsed;
	std::set<int> seen;
	for (const Json& level : root["levels"].a) {
		const int id = level["id"].i32();
		if (id <= 0 || !seen.insert(id).second)
			return fail(error, "save contains an invalid or duplicate level id");
		if (level["played"].type != Json::Bool)
			return fail(error, "save played flag must be boolean");
		if (level["played"].boolean(false)) parsed.played_by_level[id] = true;
		const Json& best = level["best"];
		if (best.isNull()) continue;
		if (!best.isObj()) return fail(error, "save best result must be an object or null");
		RunResult run;
		run.level_id = id;
		run.stars = best["stars"].i32(-1);
		run.elapsed_seconds = best["time"].num(-1.0);
		run.diamonds = best["diamonds"].i32(-1);
		run.silver_diamonds = best["silverDiamond"].i32(-1);
		if (run.stars < 1 || run.stars > 3 || !std::isfinite(run.elapsed_seconds) ||
			run.elapsed_seconds < 0.0 || run.diamonds < 0 || run.silver_diamonds < 0)
			return fail(error, "save best result contains invalid statistics");
		parsed.best_by_level[id] = BestResult{true, run};
	}
	output = std::move(parsed);
	if (error) error->clear();
	return true;
}

inline bool saveProgressFile(const std::string& path, const std::string& game_id,
	const Progress& progress, std::string* error = nullptr) {
	if (game_id.empty()) return fail(error, "game id cannot be empty");
	const std::string encoded = Json::stringify(encodeProgress(game_id, progress));
	const std::string temp_path = path + ".tmp";
	FILE* file = std::fopen(temp_path.c_str(), "wb");
	if (!file) return fail(error, "cannot open temporary save file: " + temp_path);
	const size_t written = std::fwrite(encoded.data(), 1, encoded.size(), file);
	const bool flushed = std::fflush(file) == 0;
	const bool closed = std::fclose(file) == 0;
	if (written != encoded.size() || !flushed || !closed) {
		std::remove(temp_path.c_str());
		return fail(error, "failed while writing temporary save file");
	}
	bool replaced = false;
#ifdef _WIN32
	replaced = MoveFileExA(temp_path.c_str(), path.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	replaced = std::rename(temp_path.c_str(), path.c_str()) == 0;
#endif
	if (!replaced) {
		std::remove(temp_path.c_str());
		return fail(error, "cannot replace save file: " + path);
	}
	if (error) error->clear();
	return true;
}

inline bool loadProgressFile(const std::string& path, const std::string& game_id,
	Progress& output, std::string* error = nullptr) {
	std::string encoded;
	if (!readFile(path, encoded)) return fail(error, "cannot read save file: " + path);
	return decodeProgress(encoded, game_id, output, error);
}

}} // namespace fw::progression
