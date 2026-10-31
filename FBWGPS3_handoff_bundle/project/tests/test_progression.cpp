#include "progress_save.hpp"

#include <cmath>
#include <iostream>
#include <cstdio>
#include <string>
#include <vector>

int main() {
	using namespace fw::progression;
	std::string error;
	RunResult result;
	LevelRules normal{1, "normal", 40.0, 70.0, 7};
	if (!calculateResult(normal, RunResult{1, 39.9, 7, 0, 0}, false, result, &error) || result.stars != 3) {
		std::cerr << "normal 3-star case failed: " << error << "\n"; return 1;
	}
	if (!calculateResult(normal, RunResult{1, 50.0, 6, 0, 0}, false, result, &error) || result.stars != 1) {
		std::cerr << "normal 1-star case failed\n"; return 1;
	}
	if (!calculateResult(normal, RunResult{1, 80.0, 7, 0, 0}, true, result, &error) || result.stars != 2) {
		std::cerr << "single-player time limit case failed\n"; return 1;
	}
	LevelRules puzzle{7, "puzzle", 60.0, 90.0, 12};
	if (!calculateResult(puzzle, RunResult{7, 10.0, 12, 0, 0}, false, result, &error) || result.stars != 0) {
		std::cerr << "puzzle without silver diamond should not award stars\n"; return 1;
	}
	if (!calculateResult(puzzle, RunResult{7, 90.0, 0, 1, 0}, false, result, &error) || result.stars != 3) {
		std::cerr << "puzzle silver diamond should award three stars\n"; return 1;
	}
	LevelRules dark{8, "dark", 30.0, 50.0, 4};
	if (!calculateResult(dark, RunResult{8, 31.0, 0, 0, 0}, false, result, &error) || result.stars != 2) {
		std::cerr << "dark level should award two stars after time limit\n"; return 1;
	}
	Progress progress;
	if (!markPlayed(progress, 1) || markPlayed(progress, 1)) {
		std::cerr << "played flag update failed\n"; return 1;
	}
	if (mergeRun(progress, RunResult{1, 39.0, 7, 0, 3}) != true || progress.totalStars() != 3) {
		std::cerr << "first best result merge failed\n"; return 1;
	}
	if (mergeRun(progress, RunResult{1, 20.0, 1, 0, 1}) != false || progress.bestStars(1) != 3) {
		std::cerr << "worse result replaced best\n"; return 1;
	}
	if (mergeRun(progress, RunResult{1, 38.0, 7, 0, 3}) != true ||
		std::fabs(progress.best_by_level[1].run.elapsed_seconds - 38.0) > 0.001) {
		std::cerr << "time tie-break failed\n"; return 1;
	}
	const auto states = calculateTreeStates({1, 2, 3, 4}, {{1,2},{2,3},{2,4}}, 1, progress);
	if (states.at(1) != 4 || states.at(2) != 1 || states.at(3) != 0 || states.at(4) != 0) {
		std::cerr << "tree progression states failed\n"; return 1;
	}
	const std::string save_path = "fbw_progression_test_save.json";
	if (!saveProgressFile(save_path, "forest", progress, &error) ||
		!saveProgressFile(save_path, "forest", progress, &error)) {
		std::cerr << "progress save/replace failed: " << error << "\n"; return 1;
	}
	Progress loaded;
	if (!loadProgressFile(save_path, "forest", loaded, &error)) {
		std::cerr << "progress load failed: " << error << "\n"; return 1;
	}
	if (decodeProgress("{\"version\":1,\"game\":\"other\",\"levels\":[]}", "forest", loaded, &error)) {
		std::cerr << "wrong-game save should be rejected\n"; return 1;
	}
	std::remove(save_path.c_str());
	if (loaded.totalStars() != 3 || loaded.bestStars(1) != 3 ||
		!loaded.played_by_level[1] || loaded.best_by_level[1].run.elapsed_seconds != 38.0) {
		std::cerr << "progress round-trip data mismatch\n"; return 1;
	}
	std::cout << "PASS: star rules, best-result merge, tree unlocks and progress JSON round-trip\n";
	return 0;
}
