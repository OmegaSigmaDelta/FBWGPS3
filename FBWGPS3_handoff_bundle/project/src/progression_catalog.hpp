#pragma once

#include "level.hpp"
#include "progression.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace fw { namespace progression {

struct CatalogLevel {
	LevelRules rules;
	int menu_node_id = 0; // temple.json `_id`; graph edges refer to this, not the level's public id
	std::string filename;
	double menu_x = 0.0;
	double menu_y = 0.0;
	int required_stars = 0;
	bool initial = false;
	std::string unlock_key;
};

struct ForestCatalog {
	std::string id;
	std::string menu_type;
	std::vector<CatalogLevel> levels;
	std::vector<std::pair<int, int>> edges; // `_id` -> `_id`
};

inline bool loadForestCatalog(const std::string& game_dir, ForestCatalog& output,
	std::string* error = nullptr) {
	try {
		const Json root = loadJson(game_dir + "/data/forest/temple.json");
		ForestCatalog parsed;
		parsed.id = root["id"].str();
		parsed.menu_type = root["type"].str();
		if (parsed.id.empty() || !root["levels"].isArr() || root["levels"].size() == 0)
			return fail(error, "forest temple catalogue is missing id or levels");

		std::set<int> public_ids;
		std::set<int> node_ids;
		for (const Json& entry : root["levels"].a) {
			CatalogLevel item;
			item.rules.id = entry["id"].i32();
			item.rules.type = entry["type"].str("normal");
			item.rules.time_limit_seconds = entry["time"].num();
			item.rules.single_player_time_limit_seconds = entry["mobileTime"].num();
			item.menu_node_id = entry["_id"].i32(item.rules.id);
			item.filename = entry["filename"].str();
			item.menu_x = entry["x"].num();
			item.menu_y = entry["y"].num();
			item.required_stars = entry["required"].i32();
			item.initial = entry["initial"].boolean(false);
			item.unlock_key = entry["unlock_key"].str();
			if (item.rules.id <= 0 || item.menu_node_id <= 0 || item.filename.empty())
				return fail(error, "catalogue contains invalid level id, node id, or filename");
			if (!public_ids.insert(item.rules.id).second || !node_ids.insert(item.menu_node_id).second)
				return fail(error, "catalogue contains duplicate level or menu node id");
			if (item.rules.type != "normal" && item.rules.type != "speed" &&
				item.rules.type != "puzzle" && item.rules.type != "dark")
				return fail(error, "unsupported level type for level " + std::to_string(item.rules.id));
			const Level map = loadLevel(game_dir, item.filename);
			for (const MapObject& object : map.chars) {
				if (object.type != "diamond") continue;
				if (object.props["char"].str() != "silver") ++item.rules.total_diamonds;
			}
			parsed.levels.push_back(std::move(item));
		}

		for (const Json& edge : root["edges"].a) {
			const int source = edge["source"].i32();
			const int target = edge["target"].i32();
			if (!node_ids.count(source) || !node_ids.count(target) || source == target)
				return fail(error, "tree edge references an unknown or identical menu node");
			parsed.edges.emplace_back(source, target);
		}
		if (parsed.menu_type == "tree" && parsed.edges.empty())
			return fail(error, "tree catalogue has no edges");
		output = std::move(parsed);
		if (error) error->clear();
		return true;
	} catch (const std::exception& exception) {
		return fail(error, exception.what());
	}
}

struct LevelMenuState {
	int level_id = 0;
	int state = 0; // 0 locked/unavailable, 1 unlocked, 2..4 best result (1..3 stars)
	bool locked = false;
	bool is_new = false;
};

struct MenuProgressSummary {
	int total_stars = 0;
	int minimum_required_stars = 0;
	bool game_complete = false;
	bool soft_locked = false;
	bool has_new_levels = false;
	std::vector<LevelMenuState> levels;
};

inline MenuProgressSummary calculateMenuProgress(const ForestCatalog& catalog,
	const Progress& progress, const std::set<std::string>& owned_keys = {}) {
	MenuProgressSummary result;
	result.minimum_required_stars = catalog.levels.empty() ? 0 : catalog.levels.front().required_stars;
	result.game_complete = !catalog.levels.empty();
	for (const CatalogLevel& level : catalog.levels) {
		result.total_stars += progress.bestStars(level.rules.id);
		if (progress.bestStars(level.rules.id) < 3) result.game_complete = false;
		result.minimum_required_stars = std::min(result.minimum_required_stars, level.required_stars);
	}
	result.soft_locked = result.total_stars < result.minimum_required_stars;

	std::map<int, int> states_by_level;
	if (catalog.menu_type == "tree") {
		std::vector<int> node_ids;
		Progress node_progress;
		int initial_node = 0;
		for (const CatalogLevel& level : catalog.levels) {
			node_ids.push_back(level.menu_node_id);
			if (level.initial && initial_node == 0) initial_node = level.menu_node_id;
			const BestResult& best = progress.best_by_level.count(level.rules.id)
				? progress.best_by_level.at(level.rules.id) : BestResult{};
			if (best.valid) {
				BestResult mapped = best;
				mapped.run.level_id = level.menu_node_id;
				node_progress.best_by_level[level.menu_node_id] = mapped;
			}
		}
		const auto node_states = calculateTreeStates(node_ids, catalog.edges, initial_node, node_progress);
		for (const CatalogLevel& level : catalog.levels) {
			auto it = node_states.find(level.menu_node_id);
			states_by_level[level.rules.id] = it == node_states.end() ? 0 : it->second;
		}
	} else {
		for (const CatalogLevel& level : catalog.levels) {
			const int stars = progress.bestStars(level.rules.id);
			states_by_level[level.rules.id] = stars > 0 ? std::min(4, stars + 1)
				: (result.total_stars >= level.required_stars ? 1 : 0);
		}
	}

	for (const CatalogLevel& level : catalog.levels) {
		const bool locked = !level.unlock_key.empty() && level.unlock_key != "free" &&
			owned_keys.find(level.unlock_key) == owned_keys.end();
		const bool played = progress.played_by_level.count(level.rules.id) &&
			progress.played_by_level.at(level.rules.id);
		LevelMenuState state;
		state.level_id = level.rules.id;
		state.locked = locked;
		state.state = locked ? 0 : states_by_level[level.rules.id];
		state.is_new = !played && !level.initial && !locked;
		if (state.is_new && state.state > 0) result.has_new_levels = true;
		result.levels.push_back(state);
	}
	return result;
}

}} // namespace fw::progression
