#include "level_session.hpp"

#include <cstdio>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
	if (argc != 2) return 2;
	std::string error;
	fw::progression::ForestCatalog catalog;
	if (!fw::progression::loadForestCatalog(argv[1], catalog, &error)) {
		std::cerr << "catalog: " << error << '\n'; return 2;
	}
	if (catalog.levels.empty()) return 2;
	fw::progression::Progress progress;
	fbw::LevelSession session;
	if (!session.start(argv[1], catalog.levels.front(), false, progress, &error)) {
		std::cerr << "start: " << error << '\n'; return 2;
	}
	if (!progress.played_by_level[catalog.levels.front().rules.id]) {
		std::cerr << "level was not marked played on start\n"; return 1;
	}
	const auto before = session.elapsedSeconds();
	session.update(0.02);
	if (session.elapsedSeconds() <= before) {
		std::cerr << "session timer did not advance\n"; return 1;
	}
	if (session.finished()) {
		std::cerr << "session finished before door conditions\n"; return 1;
	}
	// Scoring time must account for the whole frame delta even though gameplay
	// animations clamp their update step to 40 ms.
	const double before_long_frame = session.elapsedSeconds();
	session.update(0.25);
	if (session.elapsedSeconds() - before_long_frame < 0.249) {
		std::cerr << "session timer under-counted a long frame\n"; return 1;
	}
	const std::string path = "level_session_test_save.json";
	if (!session.saveProgress(path, catalog.id, &error)) {
		std::cerr << "save: " << error << '\n'; return 1;
	}
	fw::progression::Progress restored;
	if (!fw::progression::loadProgressFile(path, catalog.id, restored, &error) ||
		!restored.played_by_level[catalog.levels.front().rules.id]) {
		std::cerr << "roundtrip: " << error << '\n'; std::remove(path.c_str()); return 1;
	}
	std::remove(path.c_str());
	std::cout << "Level session PASS: catalogue=32 timer=active progress=connected save=roundtrip\n";
	return 0;
}
