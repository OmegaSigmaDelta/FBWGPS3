#include "progression_catalog.hpp"
#include "progress_save.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
	if (argc < 3) {
		std::cerr << "usage: test_progression_catalog <gameDir> <saveFile>\n";
		return 2;
	}
	using namespace fw::progression;
	std::string error;
	ForestCatalog catalog;
	if (!loadForestCatalog(argv[1], catalog, &error)) {
		std::cerr << "catalog load failed: " << error << "\n";
		return 1;
	}
	if (catalog.id != "forest" || catalog.menu_type != "tree" || catalog.levels.size() != 32 || catalog.edges.size() != 31) {
		std::cerr << "catalog summary mismatch: levels=" << catalog.levels.size()
			<< " edges=" << catalog.edges.size() << "\n";
		return 1;
	}
	int total_diamonds = 0, speed_levels = 0, puzzle_levels = 0, initial_count = 0;
	for (const auto& level : catalog.levels) {
		total_diamonds += level.rules.total_diamonds;
		if (level.rules.type == "speed") ++speed_levels;
		if (level.rules.type == "puzzle") ++puzzle_levels;
		if (level.initial) ++initial_count;
	}
	if (speed_levels != 6 || puzzle_levels != 9 || initial_count != 1 || total_diamonds <= 0) {
		std::cerr << "catalog-derived counts mismatch: diamonds=" << total_diamonds
			<< " speed=" << speed_levels << " puzzle=" << puzzle_levels
			<< " initial=" << initial_count << "\n";
		return 1;
	}

	Progress progress;
	if (!markPlayed(progress, 4) || markPlayed(progress, 4)) {
		std::cerr << "played flag did not transition exactly once\n";
		return 1;
	}
	RunResult first{4, 31.25, 6, 0, 3};
	if (!mergeRun(progress, first) || progress.bestStars(4) != 3) {
		std::cerr << "best result merge failed\n";
		return 1;
	}
	RunResult zero_star{2, 999.0, 0, 0, 0};
	if (mergeRun(progress, zero_star) || progress.best_by_level.count(2)) {
		std::cerr << "zero-star run should not persist as a best result\n";
		return 1;
	}
	const MenuProgressSummary menu = calculateMenuProgress(catalog, progress);
	if (menu.total_stars != 3 || menu.game_complete || menu.levels.size() != 32) {
		std::cerr << "menu progression summary failed: stars=" << menu.total_stars << " complete=" << menu.game_complete << " levels=" << menu.levels.size() << "\n";
		return 1;
	}
	// In temple.json, edges use `_id` menu-node IDs. Public level IDs are deliberately
	// not the same sequence after level 7; completing public level 4 unlocks node 18 / level 14.
	const auto level14 = std::find_if(menu.levels.begin(), menu.levels.end(), [](const LevelMenuState& item) {
		return item.level_id == 14;
	});
	const auto level2 = std::find_if(menu.levels.begin(), menu.levels.end(), [](const LevelMenuState& item) {
		return item.level_id == 2;
	});
	if (level14 == menu.levels.end() || level14->state != 1 || !level14->is_new || !menu.has_new_levels ||
		level2 == menu.levels.end() || level2->state != 0) {
		std::cerr << "menu node id mapping/initial-node state failed for level 14 or level 2\n";
		return 1;
	}

	if (!saveProgressFile(argv[2], catalog.id, progress, &error)) {
		std::cerr << "save failed: " << error << "\n";
		return 1;
	}
	Progress loaded;
	if (!loadProgressFile(argv[2], catalog.id, loaded, &error)) {
		std::cerr << "load failed: " << error << "\n";
		return 1;
	}
	if (loaded.bestStars(4) != 3 || !loaded.played_by_level[4] || loaded.played_by_level[2]) {
		std::cerr << "progress save/load round-trip mismatch\n";
		return 1;
	}
	if (!mergeRun(progress, RunResult{4, 30.5, 6, 0, 3}) ||
		!saveProgressFile(argv[2], catalog.id, progress, &error) ||
		!loadProgressFile(argv[2], catalog.id, loaded, &error) ||
		std::fabs(loaded.best_by_level.at(4).run.elapsed_seconds - 30.5) > 0.001) {
		std::cerr << "replacing an existing progress file failed: " << error << "\n";
		return 1;
	}
	Progress untouched;
	const std::vector<std::string> invalid_saves = {
		"{\"version\":1,\"game\":\"different\",\"levels\":[]}",
		"{\"version\":2,\"game\":\"forest\",\"levels\":[]}",
		"{\"version\":1,\"game\":\"forest\",\"levels\":[{\"id\":1,\"played\":true,\"best\":null},{\"id\":1,\"played\":false,\"best\":null}]}",
		"{\"version\":1,\"game\":\"forest\",\"levels\":[{\"id\":1,\"played\":true,\"best\":{\"stars\":4,\"time\":1,\"diamonds\":0,\"silverDiamond\":0}}]}",
		"{\"version\":1,\"game\":\"forest\",\"levels\":[{\"id\":1,\"played\":1,\"best\":null}]}",
		"{\"version\":1,\"game\":\"forest\",\"levels\":[]} trailing"
	};
	for (const std::string& invalid : invalid_saves) {
		if (decodeProgress(invalid, catalog.id, untouched, &error) ||
			!untouched.best_by_level.empty() || !untouched.played_by_level.empty()) {
			std::cerr << "invalid save was accepted or mutated output: " << error << "\n";
			return 1;
		}
	}
	// Complete all levels with three stars: the source marks the temple complete
	// only when every level reaches three stars.
	Progress complete;
	for (const CatalogLevel& level : catalog.levels) {
		markPlayed(complete, level.rules.id);
		mergeRun(complete, RunResult{level.rules.id, 0.0, level.rules.total_diamonds, 0, 3});
	}
	const MenuProgressSummary completed_menu = calculateMenuProgress(catalog, complete);
	if (!completed_menu.game_complete || completed_menu.total_stars != 96 || completed_menu.has_new_levels) {
		std::cerr << "full temple completion calculation failed\n";
		return 1;
	}
	std::remove(argv[2]);
	std::cout << "PASS: 32 catalogue entries, 31 graph edges, derived diamond counts (" << total_diamonds
		<< "), menu-node mapping, best results, save/load validation and temple completion\n";
	return 0;
}
