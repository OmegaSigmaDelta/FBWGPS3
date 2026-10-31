// Platform-neutral application state machine: menu -> level -> pause/result, driven by fw::Input (pads only).
//
// A platform layer (PS3 ps3_main.cpp, or a Linux test) does exactly three things per frame:
//     1. fill PadState[2] and call fw::Input::update(pads, dt)
//     2. app.update(input, dt)
//     3. app.render(frame)  -> copy the RGBA frame to the display
//
// Controls (pad only):
//   Menu    : D-pad/stick Up/Down choose level, Cross start (locked levels refuse). Number of pads decides 1P/2P.
//   Playing : two pads -> pad0 = Fireboy, pad1 = Watergirl; one pad -> L1/R1/Square switch hero. START pauses.
//   Paused  : Cross/START resume, Down restart level, Circle back to menu (progress is saved).
//   Result  : Cross = next unlocked level (win) or retry (lose), Circle = menu.
// There is deliberately no "quit" in the game: on PS3 the PS button / XMB exit is handled by the platform layer.
#pragma once

#include "frame_composer.hpp"
#include "input.hpp"
#include "progress_save.hpp"

#include <algorithm>
#include <string>

namespace fbw {

struct AppConfig {
	std::string game_dir = "game";
	std::string save_path = "fbw_progress.json";   // PS3: /dev_hdd0/game/<TITLEID>/USRDIR/fbw_progress.json
	int width = 1280, height = 720;
};

class App {
public:
	bool init(const AppConfig& cfg, std::string& error) {
		cfg_ = cfg;
		if (!loadFrameAssets(cfg_.game_dir, assets_, error)) return false;
		if (!fw::progression::loadForestCatalog(cfg_.game_dir, catalog_, &error)) return false;
		auto first = std::find_if(catalog_.levels.begin(), catalog_.levels.end(), [](const auto& l) { return l.initial; });
		if (first == catalog_.levels.end()) { error = "no initial level in catalogue"; return false; }
		level_index_ = static_cast<size_t>(std::distance(catalog_.levels.begin(), first));
		fw::progression::loadProgressFile(cfg_.save_path, catalog_.id, progress_, nullptr);   // missing file = fresh progress
		return true;
	}

	void update(const fw::Input& in, double dt) {
		dt = std::clamp(dt, 0.0, 0.1);
		const fw::MenuEvent ev = in.menuEvent();
		solo_ = in.mode() == fw::Mode::OnePad;
		solo_active_ = in.activeCharacter() == 0 ? "fb" : "wg";
		if (state_ == State::Menu) { updateMenu(ev); return; }
		const auto outcome = world_.session().gameplay().outcome();
		if (outcome != LevelOutcome::Running) { updateResult(ev, outcome); return; }
		if (state_ == State::Paused) {
			if (ev == fw::MenuEvent::Confirm || in.pausePressed()) state_ = State::Playing;
			else if (ev == fw::MenuEvent::Down) startLevel();
			else if (ev == fw::MenuEvent::Back) toMenu();
			return;
		}
		if (in.pausePressed()) { state_ = State::Paused; return; }
		GameWorldInput gi;
		gi.fireboy.left = in.fireboy().left.isDown; gi.fireboy.right = in.fireboy().right.isDown; gi.fireboy.jump = in.fireboy().up.isDown;
		gi.watergirl.left = in.watergirl().left.isDown; gi.watergirl.right = in.watergirl().right.isDown; gi.watergirl.jump = in.watergirl().up.isDown;
		last_input_ = gi;
		world_.update(gi, dt);
		if (world_.session().gameplay().outcome() != LevelOutcome::Running && !outcome_saved_) {
			fw::progression::saveProgressFile(cfg_.save_path, catalog_.id, progress_, nullptr);
			outcome_saved_ = true;
		}
	}

	bool render(ImageRGBA& frame, std::string* error = nullptr) {
		frame.width = cfg_.width; frame.height = cfg_.height;
		if (state_ == State::Menu) { composeMenu(frame); return true; }
		GameplayFrameParams p;
		p.level_id = currentLevel().rules.id; p.level_type = currentLevel().rules.type;
		p.paused = state_ == State::Paused; p.solo_mode = solo_; p.last_input = last_input_;
		p.view = cameraFor(world_, solo_, solo_active_, assets_.level_image.width, assets_.level_image.height, cfg_.width, cfg_.height);
		return composeGameplayFrame(world_, assets_, p, frame, error);
	}

	void shutdown() { fw::progression::saveProgressFile(cfg_.save_path, catalog_.id, progress_, nullptr); }

	bool inMenu() const { return state_ == State::Menu; }
	bool paused() const { return state_ == State::Paused; }
	size_t levelIndex() const { return level_index_; }
	int levelId() const { return currentLevel().rules.id; }
	const fw::progression::Progress& progress() const { return progress_; }
	const GameWorld& world() const { return world_; }
	LevelOutcome outcome() const { return world_.session().gameplay().outcome(); }
	std::string lastError() const { return error_; }

private:
	enum class State { Menu, Playing, Paused };

	const fw::progression::CatalogLevel& currentLevel() const { return catalog_.levels[level_index_]; }
	bool unlocked(size_t i) const {
		const auto menu = fw::progression::calculateMenuProgress(catalog_, progress_);
		return i < menu.levels.size() && menu.levels[i].state > 0;
	}
	bool startLevel() {
		if (!world_.start(cfg_.game_dir, currentLevel(), solo_, progress_, &error_)) return false;
		if (!buildLevelImage(cfg_.game_dir, currentLevel(), assets_, error_)) return false;
		state_ = State::Playing; outcome_saved_ = false; last_input_ = GameWorldInput{};
		return true;
	}
	void toMenu() { state_ = State::Menu; fw::progression::saveProgressFile(cfg_.save_path, catalog_.id, progress_, nullptr); }

	void updateMenu(fw::MenuEvent ev) {
		const size_t n = catalog_.levels.size();
		if (ev == fw::MenuEvent::Up) level_index_ = (level_index_ + n - 1) % n;
		else if (ev == fw::MenuEvent::Down) level_index_ = (level_index_ + 1) % n;
		else if (ev == fw::MenuEvent::Confirm && unlocked(level_index_)) startLevel();
	}
	void updateResult(fw::MenuEvent ev, LevelOutcome outcome) {
		if (ev == fw::MenuEvent::Back) { toMenu(); return; }
		if (ev != fw::MenuEvent::Confirm) return;
		if (outcome == LevelOutcome::Won) {
			const auto menu = fw::progression::calculateMenuProgress(catalog_, progress_);
			for (size_t off = 1; off < catalog_.levels.size(); ++off) {
				const size_t c = (level_index_ + off) % catalog_.levels.size();
				if (menu.levels[c].state > 0) { level_index_ = c; startLevel(); return; }
			}
			toMenu();
		} else startLevel();   // lost: retry
	}

	void composeMenu(ImageRGBA& frame) const {
		const int W = frame.width, H = frame.height;
		frame.pixels.assign(static_cast<size_t>(W) * H * 4, 0);
		fillRect(frame, 0, 0, W, H, 15, 22, 29);
		const int px = W / 2 - 300, py = H / 2 - 250;
		fillRect(frame, px, py, 600, 500, 28, 43, 54); drawRectOutline(frame, px, py, 600, 500, 240, 240, 240);
		drawText(frame, W / 2 - 210, py + 28, "FIREBOY AND WATERGIRL", 3);
		drawText(frame, W / 2 - 132, py + 76, "FOREST TEMPLE", 3);
		drawText(frame, W / 2 - 90, py + 150, "SELECT LEVEL", 2);
		fillRect(frame, W / 2 - 110, py + 190, 220, 50, 70, 95, 110); drawRectOutline(frame, W / 2 - 110, py + 190, 220, 50, 255, 255, 255);
		drawText(frame, W / 2 - 52, py + 207, "LEVEL", 2); drawText(frame, W / 2 + 42, py + 207, std::to_string(currentLevel().rules.id), 2);
		drawText(frame, W / 2 - 52, py + 262, unlocked(level_index_) ? "UNLOCKED" : "LOCKED", 2);
		drawText(frame, W / 2 - 250, py + 320, "UP DOWN SELECT   CROSS PLAY", 2);
		drawText(frame, W / 2 - 250, py + 350, "TWO PADS  PAD 1 FIREBOY  PAD 2 WATERGIRL", 1);
		drawText(frame, W / 2 - 250, py + 370, "ONE PAD   L1 R1 SQUARE SWITCH HERO", 1);
		drawText(frame, W / 2 - 250, py + 410, "TOTAL STARS", 2);
		drawText(frame, W / 2 + 40, py + 410, std::to_string(fw::progression::calculateMenuProgress(catalog_, progress_).total_stars), 2);
	}

	AppConfig cfg_;
	FrameAssets assets_;
	fw::progression::ForestCatalog catalog_;
	fw::progression::Progress progress_;
	GameWorld world_;
	GameWorldInput last_input_;
	State state_ = State::Menu;
	size_t level_index_ = 0;
	bool solo_ = true, outcome_saved_ = false;
	std::string solo_active_ = "fb", error_;
};

} // namespace fbw
