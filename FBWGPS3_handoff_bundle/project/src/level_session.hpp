#pragma once

#include "gameplay_runtime.hpp"
#include "progress_save.hpp"
#include "progression_catalog.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace fbw {

// Coordinates one complete level attempt without tying gameplay rules to a
// particular physics or graphics backend. A future SDL2/PS3 loop feeds sensor
// events here and consumes the resulting LevelOutcome / RunResult.
class LevelSession {
public:
	bool start(const std::string& game_dir, const fw::progression::CatalogLevel& catalog_level,
		bool single_player, fw::progression::Progress& progress, std::string* error = nullptr) {
		fw::Level loaded;
		fw::GroundResult ground;
		try {
			loaded = fw::loadLevel(game_dir, catalog_level.filename);
			ground = fw::buildGround(loaded);
		} catch (const std::exception& ex) {
			return fail(error, std::string("could not load level: ") + ex.what());
		}
		GameplayRuntime runtime;
		if (!runtime.initialize(loaded, ground, error)) return false;
		level_ = std::move(loaded);
		ground_ = std::move(ground);
		runtime_ = std::move(runtime);
		rules_ = catalog_level.rules;
		single_player_ = single_player;
		elapsed_seconds_ = 0.0;
		finished_ = false;
		result_ = {};
		progress_ = &progress;
		fw::progression::markPlayed(progress, rules_.id);
		if (error) error->clear();
		return true;
	}

	void update(double delta_seconds) {
		if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0 || finished_) return;
		// The result clock measures wall-clock gameplay time, not the capped
		// simulation/animation step. Capping both values silently under-counted
		// elapsed time whenever a frame took longer than 40 ms.
		elapsed_seconds_ += delta_seconds;
		const double step = std::min(delta_seconds, 0.04);
		runtime_.update(step);
		if (runtime_.outcome() == LevelOutcome::Won) finish(true);
		else if (runtime_.outcome() == LevelOutcome::Lost) finish(false);
	}

	GameplayRuntime& gameplay() { return runtime_; }
	const GameplayRuntime& gameplay() const { return runtime_; }
	const fw::Level& level() const { return level_; }
	const fw::GroundResult& ground() const { return ground_; }
	double elapsedSeconds() const { return elapsed_seconds_; }
	bool finished() const { return finished_; }
	const fw::progression::RunResult& result() const { return result_; }

	bool saveProgress(const std::string& path, const std::string& game_id,
		std::string* error = nullptr) const {
		if (!progress_) return fail(error, "session has no progress object");
		return fw::progression::saveProgressFile(path, game_id, *progress_, error);
	}

private:
	fw::Level level_;
	fw::GroundResult ground_;
	GameplayRuntime runtime_;
	fw::progression::LevelRules rules_;
	fw::progression::Progress* progress_ = nullptr;
	fw::progression::RunResult result_;
	double elapsed_seconds_ = 0.0;
	bool single_player_ = false;
	bool finished_ = false;

	static bool fail(std::string* error, const std::string& message) {
		if (error) *error = message;
		return false;
	}
	void finish(bool won) {
		if (finished_) return;
		finished_ = true;
		if (!progress_) return;
		if (!won) return;
		fw::progression::RunResult run;
		run.level_id = rules_.id;
		run.elapsed_seconds = elapsed_seconds_;
		for (const auto& entry : runtime_.characters()) {
			run.diamonds += entry.second.diamonds;
			run.silver_diamonds += entry.second.silver_diamonds;
		}
		fw::progression::RunResult calculated;
		if (!fw::progression::calculateResult(rules_, run, single_player_, calculated)) return;
		result_ = calculated;
		fw::progression::mergeRun(*progress_, result_);
	}
};

} // namespace fbw
