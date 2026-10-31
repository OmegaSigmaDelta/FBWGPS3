#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace fw { namespace progression {

struct LevelRules {
	int id = 0;
	std::string type = "normal"; // normal, speed, puzzle, dark
	double time_limit_seconds = 0.0;
	double single_player_time_limit_seconds = 0.0;
	int total_diamonds = 0;
};

struct RunResult {
	int level_id = 0;
	double elapsed_seconds = 0.0;
	int diamonds = 0;
	int silver_diamonds = 0;
	int stars = 0;
};

struct BestResult {
	bool valid = false;
	RunResult run;
};

struct Progress {
	std::map<int, BestResult> best_by_level;
	std::map<int, bool> played_by_level;

	int totalStars() const {
		int result = 0;
		for (const auto& item : best_by_level)
			if (item.second.valid) result += item.second.run.stars;
		return result;
	}

	int bestStars(int level_id) const {
		auto it = best_by_level.find(level_id);
		return it != best_by_level.end() && it->second.valid ? it->second.run.stars : 0;
	}
};

inline bool fail(std::string* error, const std::string& message) {
	if (error) *error = message;
	return false;
}

inline bool calculateResult(const LevelRules& rules, const RunResult& run,
	bool single_player, RunResult& output, std::string* error = nullptr) {
	if (rules.id <= 0 || run.level_id != rules.id)
		return fail(error, "level id is invalid or does not match run");
	if (!std::isfinite(run.elapsed_seconds) || run.elapsed_seconds < 0.0 ||
		run.diamonds < 0 || run.silver_diamonds < 0 || rules.total_diamonds < 0)
		return fail(error, "run statistics must be finite and non-negative");
	if (rules.type != "normal" && rules.type != "speed" &&
		rules.type != "puzzle" && rules.type != "dark")
		return fail(error, "unknown level type: " + rules.type);

	const double limit = single_player && rules.single_player_time_limit_seconds > 0.0
		? rules.single_player_time_limit_seconds : rules.time_limit_seconds;
	RunResult result = run;
	result.stars = 0;
	if (rules.type == "puzzle") {
		result.stars = run.silver_diamonds > 0 ? 3 : 0;
	} else if (rules.type == "dark") {
		result.stars = run.elapsed_seconds <= limit ? 3 : 2;
	} else {
		result.stars = 1;
		if (run.diamonds >= rules.total_diamonds) ++result.stars;
		if (run.elapsed_seconds <= limit) ++result.stars;
	}
	output = result;
	if (error) error->clear();
	return true;
}

inline bool betterResult(const RunResult& candidate, const RunResult& current) {
	if (candidate.stars != current.stars) return candidate.stars > current.stars;
	if (candidate.diamonds != current.diamonds) return candidate.diamonds > current.diamonds;
	return candidate.elapsed_seconds < current.elapsed_seconds;
}

// Mirrors the original ProgressModel.update(): only positive-star runs can
// replace a stored best result; the level is marked played independently.
inline bool markPlayed(Progress& progress, int level_id) {
	if (level_id <= 0) return false;
	const bool was_played = progress.played_by_level[level_id];
	progress.played_by_level[level_id] = true;
	return !was_played;
}

inline bool mergeRun(Progress& progress, const RunResult& run) {
	if (run.level_id <= 0 || run.stars <= 0 || run.stars > 3 ||
		!std::isfinite(run.elapsed_seconds) || run.elapsed_seconds < 0.0 ||
		run.diamonds < 0 || run.silver_diamonds < 0) return false;
	auto& best = progress.best_by_level[run.level_id];
	if (!best.valid || betterResult(run, best.run)) {
		best.valid = true;
		best.run = run;
		return true;
	}
	return false;
}

// Forest Temple tree progression: the initial node is state 1. Completing a
// node with at least one star raises its state to 2/3/4; direct neighbours of
// a state>1 node are unlocked to state 1. Existing states never downgrade.
inline std::map<int, int> calculateTreeStates(const std::vector<int>& level_ids,
	const std::vector<std::pair<int, int>>& edges, int initial_level,
	const Progress& progress) {
	std::map<int, int> states;
	for (int id : level_ids) {
		const int stars = progress.bestStars(id);
		states[id] = stars > 0 ? std::min(4, stars + 1) : (id == initial_level ? 1 : 0);
	}
	for (const auto& edge : edges) {
		auto a = states.find(edge.first), b = states.find(edge.second);
		if (a == states.end() || b == states.end()) continue;
		if (a->second > 1 && b->second == 0) b->second = 1;
		if (b->second > 1 && a->second == 0) a->second = 1;
	}
	return states;
}




}} // namespace fw::progression
